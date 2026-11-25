#include "PortAudioOut.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <string_view>

namespace {
    /* PortAudio initialization is process-wide and must not race another
    instance or a device enumeration. */
    std::mutex initializationMutex;

    PaError initialize() {
        std::lock_guard lock(initializationMutex);
        return Pa_Initialize();
    }

    void terminate() {
        std::lock_guard lock(initializationMutex);
        Pa_Terminate();
    }

    PaDeviceIndex findDevice(mc_string_view id) {
        if (!id.length) {
            return Pa_GetDefaultOutputDevice();
        }
        const std::string_view requested(id.data, id.length);
        const auto count = Pa_GetDeviceCount();
        for (PaDeviceIndex index = 0; index < count; ++index) {
            const auto* info = Pa_GetDeviceInfo(index);
            if (info && info->name && info->maxOutputChannels > 0 && requested == info->name) {
                return index;
            }
        }
        return paNoDevice;
    }

    mc_result visitDevice(PaDeviceIndex index, mc_output_device_fn read, void* context) {
        const auto* info = index == paNoDevice ? nullptr : Pa_GetDeviceInfo(index);
        if (!info || !info->name || info->maxOutputChannels <= 0) {
            return MC_ERROR_NOT_FOUND;
        }
        const mc_string_view name { info->name, std::strlen(info->name) };
        const mc_output_device device { name, name };
        read(context, &device);
        return MC_OK;
    }
}

PortAudioOut::PortAudioOut() : initialized(initialize() == paNoError) {}

PortAudioOut::~PortAudioOut() {
    if (this->stream) {
        if (this->Stop() != MC_OK) {
            std::fputs("PortAudio output: failed to stop before destruction\n", stderr);
            std::terminate();
        }
    }
    if (this->initialized) {
        terminate();
    }
}

mc_result PortAudioOut::VisitDevices(mc_output_device_fn read, void* context) {
    if (initialize() != paNoError) {
        return MC_ERROR_IO;
    }
    const auto count = Pa_GetDeviceCount();
    for (PaDeviceIndex index = 0; index < count; ++index) {
        visitDevice(index, read, context);
    }
    terminate();
    return count < 0 ? MC_ERROR_IO : MC_OK;
}

mc_result PortAudioOut::ReadDefaultDevice(mc_output_device_fn read, void* context) {
    if (initialize() != paNoError) {
        return MC_ERROR_IO;
    }
    const auto result = visitDevice(Pa_GetDefaultOutputDevice(), read, context);
    terminate();
    return result;
}

mc_result PortAudioOut::GetDefaultFormat(mc_string_view id, mc_audio_format& format) {
    if (!this->initialized) {
        return MC_ERROR_IO;
    }
    const auto device = findDevice(id);
    const auto* info = device == paNoDevice ? nullptr : Pa_GetDeviceInfo(device);
    if (!info || info->maxOutputChannels <= 0 || info->defaultSampleRate <= 0) {
        return MC_ERROR_NOT_FOUND;
    }
    format = {
        .struct_size = sizeof(format),
        .sample_rate = static_cast<uint32_t>(info->defaultSampleRate),
        .channels = static_cast<uint32_t>(std::min(info->maxOutputChannels, 2)),
        .channel_layout = 0,
    };
    return MC_OK;
}

mc_result PortAudioOut::StartStream() {
    this->finished.store(false, std::memory_order_release);
    this->requestedCompletion.store(false, std::memory_order_release);
    return Pa_StartStream(this->stream) == paNoError ? MC_OK : MC_ERROR_IO;
}

mc_result PortAudioOut::Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
    if (!this->initialized) {
        return MC_ERROR_IO;
    }
    const auto device = findDevice(options.device_id);
    const auto* info = device == paNoDevice ? nullptr : Pa_GetDeviceInfo(device);
    if (!info || options.format.channels > static_cast<uint32_t>(info->maxOutputChannels)) {
        return MC_ERROR_NOT_FOUND;
    }
    PaStreamParameters output {};
    output.device = device;
    output.channelCount = static_cast<int>(options.format.channels);
    output.sampleFormat = paFloat32;
    output.suggestedLatency = info->defaultHighOutputLatency;
    const auto result = Pa_OpenStream(&this->stream, nullptr, &output, options.format.sample_rate,
        paFramesPerBufferUnspecified, paNoFlag, Callback, this);
    if (result != paNoError) {
        this->stream = nullptr;
        return MC_ERROR_IO;
    }
    if (Pa_SetStreamFinishedCallback(this->stream, Finished) != paNoError) {
        return MC_ERROR_IO;
    }
    this->format = options.format;
    this->callbacks = callbacks;
    this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : DefaultCapacity;
    this->volume.store(static_cast<float>(options.initial_volume));
    this->paused.store(false);
    this->draining.store(false);
    this->producers.store(0);
    this->failureClaimed.store(false);
    this->stopped.store(false);
    return this->StartStream();
}

void PortAudioOut::FinishProducer() noexcept {
    auto state = this->producers.load();
    for (;;) {
        /* a failure that overlaps this submission must return its token before
        `Stop()` can finish waiting for the producer. */
        if ((state & FailureFlag) && (state & ~FailureFlag) == 1) {
            this->ReturnFailedBuffers();
        }
        if (this->producers.compare_exchange_weak(state, state - 1)) {
            this->producers.notify_all();
            return;
        }
    }
}

mc_result PortAudioOut::QueueAudioBuffer(const mc_output_buffer& buffer) noexcept {
    /* `Stop()` must see every producer that can still accept a buffer. */
    this->producers.fetch_add(1);
    if (this->stopped.load()) {
        this->FinishProducer();
        return MC_ERROR_CANCELLED;
    }
    if (this->producers.load() & FailureFlag) {
        this->FinishProducer();
        return MC_ERROR_IO;
    }
    if (buffer.pcm.frame_count > this->capacity) {
        this->FinishProducer();
        return MC_ERROR_BUFFER_TOO_SMALL;
    }
    const auto head = this->head.load(std::memory_order_relaxed);
    const auto tail = this->tail.load(std::memory_order_acquire);
    const auto queued = this->queuedFrames.load(std::memory_order_acquire);
    if (head - tail >= SlotCount || buffer.pcm.frame_count > this->capacity - queued) {
        this->FinishProducer();
        return MC_ERROR_QUEUE_FULL;
    }
    this->queuedFrames.fetch_add(buffer.pcm.frame_count, std::memory_order_relaxed);
    this->slots[head % SlotCount] = { buffer.pcm.data, buffer.pcm.frame_count, buffer.token };
    this->head.store(head + 1, std::memory_order_release);
    this->FinishProducer();
    return MC_OK;
}

int PortAudioOut::Callback(const void*, void* output, unsigned long frames,
    const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* context) noexcept
{
    auto& self = *static_cast<PortAudioOut*>(context);
    auto* target = static_cast<float*>(output);
    const auto channels = self.format.channels;
    size_t remaining = frames;
    if (self.paused.load(std::memory_order_acquire)) {
        std::fill_n(target, remaining * channels, 0.0f);
        return paContinue;
    }
    const float gain = self.volume.load(std::memory_order_relaxed);
    while (remaining) {
        const auto tail = self.tail.load(std::memory_order_relaxed);
        if (tail == self.head.load(std::memory_order_acquire)) {
            std::fill_n(target, remaining * channels, 0.0f);
            break;
        }
        const auto& slot = self.slots[tail % SlotCount];
        const size_t count = std::min(remaining, slot.frames - self.offset);
        const auto* source = slot.samples + self.offset * channels;
        for (size_t sample = 0; sample < count * channels; ++sample) {
            target[sample] = source[sample] * gain;
        }
        target += count * channels;
        remaining -= count;
        self.offset += count;
        self.queuedFrames.fetch_sub(count, std::memory_order_release);
        if (self.offset == slot.frames) {
            const auto token = slot.token;
            self.offset = 0;
            self.callbacks.completed(self.callbacks.context, token, MC_OK);
            self.tail.store(tail + 1, std::memory_order_release);
            self.callbacks.ready(self.callbacks.context);
        }
        else {
            /* a frame-capacity rejection can become retryable before the
            entire slot has finished playing. */
            self.callbacks.ready(self.callbacks.context);
        }
    }
    if (self.draining.load(std::memory_order_acquire) &&
        self.head.load(std::memory_order_acquire) == self.tail.load(std::memory_order_acquire))
    {
        self.requestedCompletion.store(true, std::memory_order_release);
        return paComplete;
    }
    return paContinue;
}

void PortAudioOut::Wake() noexcept {
    this->wakeGeneration.fetch_add(1, std::memory_order_release);
    this->wakeGeneration.notify_all();
}

void PortAudioOut::Finished(void* context) noexcept {
    auto& self = *static_cast<PortAudioOut*>(context);
    /* keep `Stop()` from returning tokens while this callback is still using
    the queue, even if the device stops without a host request. */
    self.producers.fetch_add(1);
    if (!self.stopped.load() && !self.aborting.load(std::memory_order_acquire) &&
        !self.requestedCompletion.load(std::memory_order_acquire))
    {
        /* a vanished device cannot consume the queue or free another slot. */
        self.producers.fetch_or(FailureFlag);
        self.callbacks.ready(self.callbacks.context);
    }
    self.finished.store(true, std::memory_order_release);
    self.Wake();
    self.FinishProducer();
}

mc_result PortAudioOut::Pause() {
    std::lock_guard lock(this->streamMutex);
    if (this->stopped.load(std::memory_order_acquire)) {
        return MC_ERROR_CANCELLED;
    }
    this->paused.store(true, std::memory_order_release);
    this->Wake();
    return MC_OK;
}

mc_result PortAudioOut::Resume() {
    std::lock_guard lock(this->streamMutex);
    if (this->stopped.load(std::memory_order_acquire)) {
        return MC_ERROR_CANCELLED;
    }
    this->paused.store(false, std::memory_order_release);
    return MC_OK;
}

mc_result PortAudioOut::SetVolume(double value) {
    this->volume.store(static_cast<float>(value), std::memory_order_relaxed);
    return MC_OK;
}

double PortAudioOut::GetVolume() const {
    return this->volume.load(std::memory_order_relaxed);
}

void PortAudioOut::ReturnPending(mc_result result) noexcept {
    auto tail = this->tail.load(std::memory_order_relaxed);
    const auto head = this->head.load(std::memory_order_acquire);
    while (tail != head) {
        this->callbacks.completed(this->callbacks.context, this->slots[tail % SlotCount].token, result);
        ++tail;
    }
    this->tail.store(head, std::memory_order_release);
    this->queuedFrames.store(0, std::memory_order_release);
    this->offset = 0;
}

void PortAudioOut::ReturnFailedBuffers() noexcept {
    if (!this->failureClaimed.exchange(true)) {
        this->ReturnPending(MC_ERROR_IO);
    }
}

mc_result PortAudioOut::Stop() noexcept {
    this->stopped.store(true);
    this->Wake();
    auto producers = this->producers.load();
    while (producers & ~FailureFlag) {
        this->producers.wait(producers, std::memory_order_acquire);
        producers = this->producers.load();
    }
    std::lock_guard lock(this->streamMutex);
    if (!this->stream) {
        return MC_OK;
    }
    const auto aborted = Pa_AbortStream(this->stream);
    if (aborted != paNoError && aborted != paStreamIsStopped) {
        return MC_ERROR_IO;
    }
    this->ReturnPending(MC_ERROR_CANCELLED);
    if (Pa_CloseStream(this->stream) != paNoError) {
        return MC_ERROR_IO;
    }
    this->stream = nullptr;
    return MC_OK;
}

mc_result PortAudioOut::Drain() {
    if (this->paused.load(std::memory_order_acquire)) {
        return MC_ERROR_IN_FLIGHT;
    }
    this->draining.store(true, std::memory_order_release);
    for (;;) {
        const auto generation = this->wakeGeneration.load(std::memory_order_acquire);
        if (this->stopped.load(std::memory_order_acquire)) {
            return MC_ERROR_CANCELLED;
        }
        if (this->producers.load() & FailureFlag) {
            return MC_ERROR_IO;
        }
        if (this->paused.load(std::memory_order_acquire)) {
            return MC_ERROR_IN_FLIGHT;
        }
        if (this->finished.load(std::memory_order_acquire)) {
            break;
        }
        this->wakeGeneration.wait(generation, std::memory_order_acquire);
    }
    this->draining.store(false, std::memory_order_release);
    std::lock_guard lock(this->streamMutex);
    if (this->stopped.load(std::memory_order_acquire)) {
        return MC_ERROR_CANCELLED;
    }
    if (this->producers.load() & FailureFlag) {
        return MC_ERROR_IO;
    }
    /* a callback returning paComplete drains the device but PortAudio still
    requires `Pa_StopStream()` before the stream can be started again. */
    const auto stopped = Pa_StopStream(this->stream);
    if (stopped != paNoError && stopped != paStreamIsStopped) {
        return MC_ERROR_IO;
    }
    return this->StartStream();
}

mc_result PortAudioOut::Flush() {
    std::lock_guard lock(this->streamMutex);
    if (this->stopped.load(std::memory_order_acquire)) {
        return MC_ERROR_CANCELLED;
    }
    this->aborting.store(true, std::memory_order_release);
    const auto result = Pa_AbortStream(this->stream);
    this->aborting.store(false, std::memory_order_release);
    if (result != paNoError && result != paStreamIsStopped) {
        return MC_ERROR_IO;
    }
    this->ReturnPending(MC_ERROR_CANCELLED);
    this->draining.store(false, std::memory_order_release);
    return this->StartStream();
}
