/* `QueueAudioBuffer()` converts one host buffer and writes it to sndio. when
sndio is full, the caller waits in poll; `Stop()` signals a pipe, waits for the
caller to return the buffer, and then closes the device. sndio copies the PCM,
so completion releases the host buffer before the device has finished playing.
`sio_onmove()` records the last observed device position for `GetTiming()`.
the other control operations are serialized by the host. */
#include "SndioOut.h"
#include <musikcore/plugin/services.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <unistd.h>

namespace {
    constexpr size_t DefaultCapacity = 32768;

    void ReportDefault(mc_output_device_fn read, void* user) {
        const mc_output_device device {
            .id = { "default", 7 },
            .name = { "Default", 7 },
        };
        read(user, &device);
    }
}

mc_result SndioOut::VisitDevices(mc_output_device_fn, void*) {
    /* sndio does not enumerate endpoints; the host offers the default. */
    return MC_OK;
}

mc_result SndioOut::ReadDefaultDevice(mc_output_device_fn read, void* user) {
    ReportDefault(read, user);
    return MC_OK;
}

mc_result SndioOut::GetDefaultFormat(mc_string_view, mc_audio_format&) {
    return MC_ERROR_UNSUPPORTED;
}

void SndioOut::ResetTiming() noexcept {
    this->played.store(0);
    this->playedTime.store(0);
    this->generation.fetch_add(1);
}

void SndioOut::OnMove(void* context, int delta) noexcept {
    auto& self = *static_cast<SndioOut*>(context);
    if (delta > 0) {
        self.played.fetch_add(static_cast<uint64_t>(delta));
    }
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    self.playedTime.store(static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count()));
}

mc_result SndioOut::GetTiming(mc_output_timing& timing) const noexcept {
    const auto timestamp = this->playedTime.load();
    if (timestamp) {
        timing.valid_flags = MC_OUTPUT_TIMING_PLAYED;
        timing.generation = this->generation.load();
        timing.frames_played_by_driver = this->played.load();
        timing.frames_played_timestamp_ns = timestamp;
    }
    return MC_OK;
}

mc_result SndioOut::Prepare(const mc_output_options& options, const mc_output_callbacks& outputCallbacks) {
    std::lock_guard lock(this->mutex);
    this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : DefaultCapacity;
    if (this->capacity > std::numeric_limits<size_t>::max() / options.format.channels) {
        return MC_ERROR_BUFFER_TOO_SMALL;
    }
    if (pipe(this->wake) != 0 || fcntl(this->wake[0], F_SETFL, O_NONBLOCK) < 0 ||
        fcntl(this->wake[1], F_SETFL, O_NONBLOCK) < 0)
    {
        return MC_ERROR_IO;
    }

    std::string device = options.device_id.length
        ? std::string(options.device_id.data, options.device_id.length)
        : "default";
    /* preserve the legacy custom-device preference when the default is selected. */
    if (device == "default" && this->services && this->services->preferences) {
        struct Preference { std::string value; bool failed = false; } preference;
        const auto read = [](void* context, mc_string_view value) {
            auto& result = *static_cast<Preference*>(context);
            try {
                result.value.assign(value.data ? value.data : "", value.length);
            }
            catch (...) {
                result.failed = true;
            }
        };
        const auto* preferences = this->services->preferences;
        const auto status = preferences->read_text(preferences->context, { "device_id", 9 },
            { "", 0 }, read, &preference);
        if (status != MC_OK || preference.failed) {
            return MC_ERROR_INTERNAL;
        }
        if (!preference.value.empty()) {
            device = std::move(preference.value);
        }
    }
    this->handle = sio_open(device == "default" ? SIO_DEVANY : device.c_str(), SIO_PLAY, 1);
    if (!this->handle) {
        return MC_ERROR_IO;
    }

    /* sndio may negotiate a different encoding; accept only the format we
    convert to below, rather than sending it incompatible samples. */
    sio_par parameters;
    sio_initpar(&parameters);
    parameters.bits = 16;
    parameters.bps = sizeof(int16_t);
    parameters.sig = 1;
    parameters.le = SIO_LE_NATIVE;
    parameters.pchan = options.format.channels;
    parameters.rate = options.format.sample_rate;
    parameters.appbufsz = parameters.rate / 4;
    if (!sio_setpar(this->handle, &parameters) || !sio_getpar(this->handle, &parameters) ||
        parameters.bits != 16 || parameters.bps != sizeof(int16_t) || !parameters.sig ||
        parameters.le != SIO_LE_NATIVE || parameters.pchan != options.format.channels ||
        parameters.rate != options.format.sample_rate)
    {
        return MC_ERROR_UNSUPPORTED;
    }
    this->samples.resize(this->capacity * options.format.channels);
    this->descriptors.resize(static_cast<size_t>(sio_nfds(this->handle)) + 1);
    this->format = options.format;
    this->callbacks = outputCallbacks;
    this->volume = options.initial_volume;
    this->ResetTiming();
    sio_onmove(this->handle, OnMove, this);
    if (!sio_start(this->handle)) {
        return MC_ERROR_IO;
    }
    this->started = true;
    this->stopped = false;
    this->paused = false;
    return MC_OK;
}

mc_result SndioOut::WaitForDevice() {
    int count;
    {
        std::lock_guard lock(this->mutex);
        if (this->stopped) {
            return MC_ERROR_CANCELLED;
        }
        count = sio_pollfd(this->handle, this->descriptors.data(), POLLOUT);
        this->descriptors[count] = {
            .fd = this->wake[0],
            .events = POLLIN,
            .revents = 0,
        };
    }
    int result;
    do {
        result = poll(this->descriptors.data(), static_cast<nfds_t>(count + 1), -1);
    } while (result < 0 && errno == EINTR);
    if (result < 0) {
        return MC_ERROR_IO;
    }
    std::lock_guard lock(this->mutex);
    if (this->stopped || this->descriptors[count].revents) {
        return MC_ERROR_CANCELLED;
    }
    return (sio_revents(this->handle, this->descriptors.data()) & POLLHUP) ? MC_ERROR_IO : MC_OK;
}

mc_result SndioOut::QueueAudioBuffer(const mc_output_buffer& block) noexcept {
    {
        std::lock_guard lock(this->mutex);
        if (this->stopped) {
            return MC_ERROR_CANCELLED;
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
        if (this->paused) {
            return MC_ERROR_QUEUE_FULL;
        }
        if (this->operationActive) {
            return MC_ERROR_IN_FLIGHT;
        }

        /* retain the host buffer until the converted samples are written. */
        this->operationActive = true;
        const float gain = static_cast<float>(this->volume * this->volume * this->volume);
        for (size_t i = 0; i < block.pcm.frame_count * this->format.channels; ++i) {
            const float input = block.pcm.data[i] * gain;
            const float sample = std::isfinite(input) ? std::clamp(input, -1.0f, 1.0f) : 0.0f;
            this->samples[i] = static_cast<int16_t>(std::lrint(sample * 32767.0f));
        }
    }

    const size_t length = block.pcm.frame_count * this->format.channels * sizeof(int16_t);
    const auto* bytes = reinterpret_cast<const unsigned char*>(this->samples.data());
    size_t written = 0;
    mc_result outcome = MC_OK;
    /* a zero-length nonblocking write means poll until sndio or Stop() wakes us. */
    while (written < length) {
        size_t count;
        {
            std::lock_guard lock(this->mutex);
            if (this->stopped) {
                outcome = MC_ERROR_CANCELLED;
                break;
            }
            count = sio_write(this->handle, bytes + written, length - written);
            if (!count && sio_eof(this->handle)) {
                outcome = MC_ERROR_IO;
                break;
            }
        }
        written += count;
        if (!count && (outcome = this->WaitForDevice()) != MC_OK) {
            break;
        }
    }

    /* return each accepted token before Stop() can release the device. */
    this->callbacks.completed(this->callbacks.context, block.token, outcome);
    {
        std::lock_guard lock(this->mutex);
        this->operationActive = false;
        this->operationEnded.notify_all();
    }
    return MC_OK;
}

mc_result SndioOut::Pause() {
    std::lock_guard lock(this->mutex);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->operationActive) {
        return MC_ERROR_IN_FLIGHT;
    }

    /* sndio has no pause operation; stop drains its queued audio. */
    if (!this->paused && !sio_stop(this->handle)) {
        return MC_ERROR_IO;
    }
    this->started = false;
    this->paused = true;
    return MC_OK;
}

mc_result SndioOut::Resume() {
    std::lock_guard lock(this->mutex);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->paused) {
        this->ResetTiming();
        if (!sio_start(this->handle)) {
            return MC_ERROR_IO;
        }
        this->started = true;
        this->paused = false;
        if (this->callbacks.ready) {
            this->callbacks.ready(this->callbacks.context);
        }
    }
    return MC_OK;
}

mc_result SndioOut::SetVolume(double value) {
    std::lock_guard lock(this->mutex);
    this->volume = value;
    return MC_OK;
}

double SndioOut::GetVolume() const {
    std::lock_guard lock(this->mutex);
    return this->volume;
}

mc_result SndioOut::Stop() noexcept {
    std::unique_lock lock(this->mutex);
    this->stopped = true;
    if (this->wake[1] >= 0) {
        const char signal = 1;
        ssize_t result;
        do {
            result = write(this->wake[1], &signal, 1);
        } while (result < 0 && errno == EINTR);
        if (result < 0 && errno != EAGAIN) {
            std::fputs("sndio output: cannot wake the producer\n", stderr);
            std::abort();
        }
    }
    this->operationEnded.wait(lock, [this] { return !this->operationActive; });
    if (this->handle) {
        if (this->started) {
            sio_flush(this->handle);
        }
        sio_close(this->handle);
        this->handle = nullptr;
    }
    for (int& descriptor : this->wake) {
        if (descriptor >= 0) {
            close(descriptor);
        }
        descriptor = -1;
    }
    this->started = false;
    return MC_OK;
}

mc_result SndioOut::Drain() {
    std::lock_guard lock(this->mutex);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->operationActive || this->paused) {
        return MC_ERROR_IN_FLIGHT;
    }

    /* sndio forces playback to start and drains the endpoint in sio_stop().
    this call can delay a concurrent Stop() until the device has drained. */
    if (!sio_stop(this->handle)) {
        return MC_ERROR_IO;
    }
    this->started = false;
    this->ResetTiming();
    if (!sio_start(this->handle)) {
        return MC_ERROR_IO;
    }
    this->started = true;
    return MC_OK;
}

mc_result SndioOut::Flush() {
    std::lock_guard lock(this->mutex);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->operationActive) {
        return MC_ERROR_IN_FLIGHT;
    }

    /* discard queued sound without closing the selected endpoint. */
    if (this->started && !sio_flush(this->handle)) {
        return MC_ERROR_IO;
    }
    this->started = false;
    this->ResetTiming();
    if (!sio_start(this->handle)) {
        return MC_ERROR_IO;
    }
    this->started = true;
    this->paused = false;
    if (this->callbacks.ready) {
        this->callbacks.ready(this->callbacks.context);
    }
    return MC_OK;
}
