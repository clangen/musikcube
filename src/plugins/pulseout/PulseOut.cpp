//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "PulseOut.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using musik::plugins::OutputTimingEstimator;

namespace {
    constexpr size_t DefaultCapacity = 32768;
    constexpr char LinearVolumeKey[] = "force_linear_volume";

    struct PulseLoopLock {
        explicit PulseLoopLock(pa_threaded_mainloop* loop) : loop(loop) { pa_threaded_mainloop_lock(loop); }
        ~PulseLoopLock() { pa_threaded_mainloop_unlock(this->loop); }
        PulseLoopLock(const PulseLoopLock&) = delete;
        PulseLoopLock& operator=(const PulseLoopLock&) = delete;
        pa_threaded_mainloop* const loop;
    };

    struct Discovery {
        pa_threaded_mainloop* loop = nullptr;
        std::vector<std::pair<std::string, std::string>> devices;
        std::string defaultDevice;
        bool failed = false;
    };

    void contextChanged(pa_context*, void* user) {
        auto& state = *static_cast<Discovery*>(user);
        pa_threaded_mainloop_signal(state.loop, 0);
    }

    void sinkInfo(pa_context*, const pa_sink_info* info, int end, void* user) {
        auto& state = *static_cast<Discovery*>(user);
        if (info && info->name) {
            try {
                state.devices.emplace_back(info->name, info->description ? info->description : info->name);
            }
            catch (...) {
                state.failed = true;
            }
        }
        if (end) {
            state.failed = state.failed || end < 0;
            pa_threaded_mainloop_signal(state.loop, 0);
        }
    }

    void serverInfo(pa_context*, const pa_server_info* info, void* user) {
        auto& state = *static_cast<Discovery*>(user);
        if (info && info->default_sink_name) {
            try {
                state.defaultDevice = info->default_sink_name;
            }
            catch (...) {
                state.failed = true;
            }
        }
        else {
            state.failed = true;
        }
        pa_threaded_mainloop_signal(state.loop, 0);
    }

    mc_result discover(Discovery& state, bool defaultOnly) {
        state.loop = pa_threaded_mainloop_new();
        if (!state.loop) {
            return MC_ERROR_INTERNAL;
        }
        pa_context* context = pa_context_new(pa_threaded_mainloop_get_api(state.loop), "musikcube");
        if (!context) {
            pa_threaded_mainloop_free(state.loop);
            return MC_ERROR_INTERNAL;
        }
        pa_context_set_state_callback(context, contextChanged, &state);
        if (pa_context_connect(context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0 ||
            pa_threaded_mainloop_start(state.loop) < 0)
        {
            pa_context_disconnect(context);
            pa_context_unref(context);
            pa_threaded_mainloop_free(state.loop);
            return MC_ERROR_IO;
        }
        pa_threaded_mainloop_lock(state.loop);
        while (PA_CONTEXT_IS_GOOD(pa_context_get_state(context)) &&
            pa_context_get_state(context) != PA_CONTEXT_READY)
        {
            pa_threaded_mainloop_wait(state.loop);
        }
        mc_result result = MC_ERROR_IO;
        if (pa_context_get_state(context) == PA_CONTEXT_READY) {
            pa_operation* operation = defaultOnly
                ? pa_context_get_server_info(context, serverInfo, &state)
                : pa_context_get_sink_info_list(context, sinkInfo, &state);
            if (operation) {
                while (pa_operation_get_state(operation) == PA_OPERATION_RUNNING &&
                    PA_CONTEXT_IS_GOOD(pa_context_get_state(context)))
                {
                    pa_threaded_mainloop_wait(state.loop);
                }
                if (pa_operation_get_state(operation) == PA_OPERATION_DONE && !state.failed) {
                    result = MC_OK;
                }
                pa_operation_unref(operation);
            }
        }
        pa_context_disconnect(context);
        pa_context_unref(context);
        pa_threaded_mainloop_unlock(state.loop);
        pa_threaded_mainloop_stop(state.loop);
        pa_threaded_mainloop_free(state.loop);
        return result;
    }

    void reportDevice(mc_output_device_fn read, void* user, const std::string& id, const std::string& name) {
        const mc_output_device device {
            .id = { id.data(), id.size() },
            .name = { name.data(), name.size() },
        };
        read(user, &device);
    }
}

mc_result PulseOut::VisitDevices(mc_output_device_fn read, void* user) {
    Discovery state;
    const auto status = discover(state, false);
    if (status == MC_OK) {
        for (const auto& [id, name] : state.devices) {
            reportDevice(read, user, id, name);
        }
    }
    return status;
}

mc_result PulseOut::ReadDefaultDevice(mc_output_device_fn read, void* user) {
    Discovery state;
    const auto status = discover(state, true);
    if (status == MC_OK && !state.defaultDevice.empty()) {
        reportDevice(read, user, state.defaultDevice, state.defaultDevice);
        return MC_OK;
    }
    return status == MC_OK ? MC_ERROR_NOT_FOUND : status;
}

mc_result PulseOut::GetDefaultFormat(mc_string_view, mc_audio_format&) {
    return MC_ERROR_UNSUPPORTED;
}

mc_result PulseOut::Prepare(const mc_output_options& options, const mc_output_callbacks& outputCallbacks) {
    std::lock_guard lock(this->lifecycle);
    if (options.format.channels > PA_CHANNELS_MAX) {
        return MC_ERROR_UNSUPPORTED;
    }
    this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : DefaultCapacity;
    if (this->capacity > std::numeric_limits<size_t>::max() / options.format.channels / sizeof(float)) {
        return MC_ERROR_BUFFER_TOO_SMALL;
    }

    this->linearVolume = false;
    if (this->services && this->services->settings) {
        const auto* prefs = this->services->settings;
        const mc_string_view key { LinearVolumeKey, sizeof(LinearVolumeKey) - 1 };
        const auto status = prefs->read_boolean(prefs->context, key, false, &this->linearVolume);
        if (status != MC_OK) {
            return status;
        }
    }

    const pa_sample_spec spec {
        .format = PA_SAMPLE_FLOAT32NE,
        .rate = options.format.sample_rate,
        .channels = static_cast<uint8_t>(options.format.channels),
    };
    this->format = options.format;
    this->callbacks = outputCallbacks;
    this->volume = options.initial_volume;

    this->loop = pa_threaded_mainloop_new();
    if (!this->loop) {
        return MC_ERROR_INTERNAL;
    }
    this->context = pa_context_new(pa_threaded_mainloop_get_api(this->loop), "musikcube");
    if (!this->context) {
        return MC_ERROR_INTERNAL;
    }

    auto* mainloop = pa_threaded_mainloop_get_api(this->loop);
    this->writeEvent = mainloop->defer_new(mainloop, DeferredWrite, this);
    if (!this->writeEvent) {
        return MC_ERROR_INTERNAL;
    }
    mainloop->defer_enable(this->writeEvent, 0);
    pa_context_set_state_callback(this->context, ContextChanged, this);
    if (pa_context_connect(this->context, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0 ||
        pa_threaded_mainloop_start(this->loop) < 0)
    {
        return MC_ERROR_IO;
    }
    this->loopStarted = true;
    PulseLoopLock loopLock(this->loop);
    while (PA_CONTEXT_IS_GOOD(pa_context_get_state(this->context)) &&
        pa_context_get_state(this->context) != PA_CONTEXT_READY)
    {
        pa_threaded_mainloop_wait(this->loop);
    }
    if (pa_context_get_state(this->context) != PA_CONTEXT_READY) {
        return MC_ERROR_IO;
    }

    const std::string device(options.device_id.data ? options.device_id.data : "", options.device_id.length);
    if (!this->ConnectStream(spec, device.empty() ? nullptr : device.c_str()) &&
        (device.empty() || !this->ConnectStream(spec, nullptr)))
    {
        return MC_ERROR_IO;
    }
    if (!this->RequestVolume(this->volume)) {
        return MC_ERROR_IO;
    }

    this->scheduledFrames = 0;
    this->timingEstimator.Reset(this->format.sample_rate);
    this->failed = false;
    this->paused = false;
    this->stopped = false;
    return MC_OK;
}

bool PulseOut::ConnectStream(const pa_sample_spec& spec, const char* device) {
    this->stream = pa_stream_new(this->context, "musikcube", &spec, nullptr);
    if (!this->stream) {
        return false;
    }
    pa_stream_set_state_callback(this->stream, StreamChanged, this);
    constexpr auto flags = static_cast<pa_stream_flags_t>(
        PA_STREAM_INTERPOLATE_TIMING | PA_STREAM_ADJUST_LATENCY | PA_STREAM_AUTO_TIMING_UPDATE);
    if (pa_stream_connect_playback(this->stream, device, nullptr, flags, nullptr, nullptr) >= 0) {
        while (PA_STREAM_IS_GOOD(pa_stream_get_state(this->stream)) &&
            pa_stream_get_state(this->stream) != PA_STREAM_READY)
        {
            pa_threaded_mainloop_wait(this->loop);
        }
        if (pa_stream_get_state(this->stream) == PA_STREAM_READY) {
            pa_stream_set_write_callback(this->stream, Writable, this);
            pa_stream_set_latency_update_callback(this->stream, LatencyChanged, this);
            return true;
        }
    }
    this->CloseStream();
    return false;
}

void PulseOut::CloseStream() noexcept {
    if (this->stream) {
        pa_stream_set_state_callback(this->stream, nullptr, nullptr);
        pa_stream_set_write_callback(this->stream, nullptr, nullptr);
        pa_stream_set_latency_update_callback(this->stream, nullptr, nullptr);
        pa_stream_disconnect(this->stream);
        pa_stream_unref(this->stream);
        this->stream = nullptr;
    }
}

void PulseOut::ContextChanged(pa_context* context, void* user) noexcept {
    auto& output = *static_cast<PulseOut*>(user);
    if (!output.stopped && !PA_CONTEXT_IS_GOOD(pa_context_get_state(context))) {
        output.Fail();
    }
    pa_threaded_mainloop_signal(output.loop, 0);
}

void PulseOut::StreamChanged(pa_stream* stream, void* user) noexcept {
    auto& output = *static_cast<PulseOut*>(user);
    if (!output.stopped && !PA_STREAM_IS_GOOD(pa_stream_get_state(stream))) {
        output.Fail();
    }
    pa_threaded_mainloop_signal(output.loop, 0);
}

void PulseOut::Writable(pa_stream*, size_t bytes, void* user) noexcept {
    static_cast<PulseOut*>(user)->WritePending(bytes);
}

void PulseOut::DeferredWrite(pa_mainloop_api* mainloop, pa_defer_event* event, void* user) noexcept {
    mainloop->defer_enable(event, 0);
    auto& output = *static_cast<PulseOut*>(user);
    if (!output.stopped && !output.paused && !output.failed && output.pending) {
        output.WritePending(pa_stream_writable_size(output.stream));
    }
}

void PulseOut::LatencyChanged(pa_stream*, void* user) noexcept {
    static_cast<PulseOut*>(user)->PublishTiming();
}

void PulseOut::PublishTiming() noexcept {
    if (!this->stopped && !this->failed && !this->paused && this->scheduledFrames) {
        pa_usec_t latency = 0;
        int negative = 0;
        if (pa_stream_get_latency(this->stream, &latency, &negative) >= 0) {
            this->timingEstimator.Publish(this->scheduledFrames,
                OutputTimingEstimator::Now() + (negative ? 0 : latency * 1000));
        }
    }
}

void PulseOut::WritePending(size_t writableBytes) noexcept {
    if (this->stopped || this->paused || this->failed || !this->pending) {
        return;
    }
    if (writableBytes == static_cast<size_t>(-1)) {
        this->Fail();
        return;
    }

    const size_t channels = this->format.channels;
    const size_t stride = channels * sizeof(float);
    const size_t frames = std::min(writableBytes / stride, this->pending->pcm.frame_count - this->writtenFrames);
    if (!frames) {
        return;
    }
    if (pa_stream_write(this->stream, this->pending->pcm.data + this->writtenFrames * channels,
        frames * stride, nullptr, 0, PA_SEEK_RELATIVE) < 0)
    {
        this->Fail();
        return;
    }
    this->writtenFrames += frames;
    this->scheduledFrames += frames;
    this->PublishTiming();
    if (this->writtenFrames == this->pending->pcm.frame_count) {
        this->CompletePending(MC_OK);
        if (this->callbacks.ready) {
            this->callbacks.ready(this->callbacks.context);
        }
    }
}

void PulseOut::CompletePending(mc_result result) noexcept {
    if (this->pending) {
        const auto token = this->pending->token;
        this->pending.reset();
        this->writtenFrames = 0;
        this->callbacks.completed(this->callbacks.context, token, result);
        pa_threaded_mainloop_signal(this->loop, 0);
    }
}

void PulseOut::Fail() noexcept {
    if (!this->failed) {
        this->failed = true;
        this->timingEstimator.Reset(0);
        this->CompletePending(MC_ERROR_IO);
        if (this->callbacks.ready) {
            this->callbacks.ready(this->callbacks.context);
        }
    }
}

mc_result PulseOut::QueueAudioBuffer(const mc_output_buffer& block) noexcept {
    std::lock_guard lock(this->lifecycle);
    if (!this->loopStarted) {
        return MC_ERROR_CANCELLED;
    }
    PulseLoopLock loopLock(this->loop);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->failed) {
        return MC_ERROR_IO;
    }
    if (block.pcm.format.sample_rate != this->format.sample_rate ||
        block.pcm.format.channels != this->format.channels ||
        block.pcm.format.channel_layout != this->format.channel_layout)
    {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    if (block.pcm.frame_count > this->capacity) {
        return MC_ERROR_BUFFER_TOO_SMALL;
    }
    if (this->pending) {
        return MC_ERROR_QUEUE_FULL;
    }

    this->timingEstimator.Submit(block.pcm.frame_count);
    this->pending = block;
    /* an earlier writable callback may have fired before PCM arrived. */
    pa_threaded_mainloop_get_api(this->loop)->defer_enable(this->writeEvent, 1);
    return MC_OK;
}

mc_result PulseOut::Pause() {
    std::lock_guard lock(this->lifecycle);
    if (!this->loopStarted) {
        return MC_ERROR_CANCELLED;
    }
    PulseLoopLock loopLock(this->loop);
    return this->SetPaused(true);
}

mc_result PulseOut::Resume() {
    std::lock_guard lock(this->lifecycle);
    if (!this->loopStarted) {
        return MC_ERROR_CANCELLED;
    }
    PulseLoopLock loopLock(this->loop);
    return this->SetPaused(false);
}

mc_result PulseOut::SetPaused(bool value) {
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->failed) {
        return MC_ERROR_IO;
    }
    if (this->paused == value) {
        return MC_OK;
    }
    auto* operation = pa_stream_cork(this->stream, value, nullptr, nullptr);
    if (!operation) {
        return MC_ERROR_IO;
    }
    pa_operation_unref(operation);
    this->paused = value;
    if (value) {
        this->timingEstimator.Pause();
    }
    else {
        this->timingEstimator.Resume();
        pa_threaded_mainloop_get_api(this->loop)->defer_enable(this->writeEvent, 1);
        if (!this->pending && this->callbacks.ready) {
            this->callbacks.ready(this->callbacks.context);
        }
    }
    return MC_OK;
}

bool PulseOut::RequestVolume(double value) {
    const auto normalized = this->linearVolume
        ? static_cast<pa_volume_t>(std::round(PA_VOLUME_NORM * value))
        : pa_sw_volume_from_linear(value);
    pa_cvolume levels {};
    pa_cvolume_set(&levels, this->format.channels, normalized);
    auto* operation = pa_context_set_sink_input_volume(this->context, pa_stream_get_index(this->stream),
        &levels, nullptr, nullptr);
    if (!operation) {
        return false;
    }
    pa_operation_unref(operation);
    return true;
}

mc_result PulseOut::SetVolume(double value) {
    std::lock_guard lock(this->lifecycle);
    if (this->loopStarted) {
        PulseLoopLock loopLock(this->loop);
        if (!this->stopped && (this->failed || !this->RequestVolume(value))) {
            return MC_ERROR_IO;
        }
    }
    this->volume = value;
    return MC_OK;
}

double PulseOut::GetVolume() const {
    std::lock_guard lock(this->lifecycle);
    return this->volume;
}

mc_result PulseOut::Stop() noexcept {
    std::unique_lock lock(this->lifecycle);
    if (!this->loop) {
        return MC_OK;
    }

    {
        PulseLoopLock loopLock(this->loop);
        this->stopped = true;
        this->timingEstimator.Reset(0);
        this->CompletePending(MC_ERROR_CANCELLED);
        pa_threaded_mainloop_signal(this->loop, 0);
    }
    /* the drain still needs the loop to cancel its native operation. */
    this->finished.wait(lock, [this] { return !this->draining; });
    if (!this->loop) {
        return MC_OK;
    }

    {
        PulseLoopLock loopLock(this->loop);
        this->CloseStream();
        if (this->context) {
            pa_context_set_state_callback(this->context, nullptr, nullptr);
            pa_context_disconnect(this->context);
            pa_context_unref(this->context);
            this->context = nullptr;
        }
        if (this->writeEvent) {
            pa_threaded_mainloop_get_api(this->loop)->defer_free(this->writeEvent);
            this->writeEvent = nullptr;
        }
    }
    if (this->loopStarted) {
        pa_threaded_mainloop_stop(this->loop);
        this->loopStarted = false;
    }
    pa_threaded_mainloop_free(this->loop);
    this->loop = nullptr;
    return MC_OK;
}

void PulseOut::OperationComplete(pa_stream*, int success, void* user) noexcept {
    auto& output = *static_cast<PulseOut*>(user);
    output.operationSucceeded = success != 0;
    pa_threaded_mainloop_signal(output.loop, 0);
}

mc_result PulseOut::WaitForOperation(pa_operation* operation) {
    if (!operation) {
        return MC_ERROR_IO;
    }
    while (!this->stopped && !this->failed && pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(this->loop);
    }
    if (pa_operation_get_state(operation) == PA_OPERATION_RUNNING) {
        pa_operation_cancel(operation);
    }
    pa_operation_unref(operation);
    return this->stopped ? MC_ERROR_CANCELLED : this->failed || !this->operationSucceeded ? MC_ERROR_IO : MC_OK;
}

mc_result PulseOut::Drain() {
    std::unique_lock lock(this->lifecycle);
    if (!this->loopStarted) {
        return MC_ERROR_CANCELLED;
    }

    mc_result result;
    {
        PulseLoopLock loopLock(this->loop);
        if (this->stopped) {
            return MC_ERROR_CANCELLED;
        }
        if (this->paused || this->draining) {
            return MC_ERROR_IN_FLIGHT;
        }
        this->draining = true;
        lock.unlock();
        while (this->pending && !this->stopped && !this->failed) {
            pa_threaded_mainloop_wait(this->loop);
        }
        if (this->stopped || this->failed) {
            result = this->stopped ? MC_ERROR_CANCELLED : MC_ERROR_IO;
        }
        else {
            this->operationSucceeded = false;
            result = this->WaitForOperation(pa_stream_drain(this->stream, OperationComplete, this));
            if (result == MC_OK) {
                this->timingEstimator.Publish(this->scheduledFrames, OutputTimingEstimator::Now());
            }
        }
    }
    lock.lock();
    this->draining = false;
    this->finished.notify_all();
    return result;
}

mc_result PulseOut::Flush() {
    std::lock_guard lock(this->lifecycle);
    if (!this->loopStarted) {
        return MC_ERROR_CANCELLED;
    }
    PulseLoopLock loopLock(this->loop);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->failed) {
        return MC_ERROR_IO;
    }

    /* pending PCM must not enter the server while its queue is being flushed. */
    this->scheduledFrames = 0;
    this->timingEstimator.Reset(this->format.sample_rate);
    this->CompletePending(MC_ERROR_CANCELLED);
    this->operationSucceeded = false;
    const auto result = this->WaitForOperation(pa_stream_flush(this->stream, OperationComplete, this));
    if (result != MC_OK) {
        this->Fail();
        return result;
    }
    const bool wasPaused = this->paused;
    const auto resumed = this->SetPaused(false);
    if (resumed == MC_OK && !wasPaused && this->callbacks.ready) {
        this->callbacks.ready(this->callbacks.context);
    }
    return resumed;
}
