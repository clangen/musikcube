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

#include "AlsaOut.h"
#include <musikcore/plugin/services.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <string>
#include <sys/eventfd.h>
#include <unistd.h>

using musik::plugins::OutputTimingEstimator;

namespace {
    constexpr size_t DefaultCapacity = 32768;
    constexpr char LatencyKey[] = "latency_ms";
    constexpr mc_integer_setting LatencySetting {
        .key = { LatencyKey, sizeof(LatencyKey) - 1 },
        .default_value = 500,
        .minimum = 10,
        .maximum = 5000,
    };
    constexpr unsigned MicrosecondsPerMillisecond = 1000;

    void reportDevice(mc_output_device_fn read, void* user, const char* id, const char* name) {
        const mc_output_device device {
            .id = { id, std::strlen(id) },
            .name = { name, std::strlen(name) },
        };
        read(user, &device);
    }
}

const mc_configuration_api& AlsaConfiguration() {
    static const mc_configuration_api configuration {
        .struct_size = sizeof(mc_configuration_api),
        .integers = &LatencySetting,
        .integer_count = 1,
        .booleans = nullptr,
        .boolean_count = 0,
        .numbers = nullptr,
        .number_count = 0,
        .texts = nullptr,
        .text_count = 0,
    };
    return configuration;
}

mc_result AlsaOut::VisitDevices(mc_output_device_fn read, void* user) {
    void** hints = nullptr;
    if (snd_device_name_hint(-1, "pcm", &hints) < 0) {
        return MC_ERROR_IO;
    }
    for (void** hint = hints; *hint; ++hint) {
        char* name = snd_device_name_get_hint(*hint, "NAME");
        char* direction = snd_device_name_get_hint(*hint, "IOID");
        if (name && (!direction || std::strcmp(direction, "Input") != 0)) {
            reportDevice(read, user, name, name);
        }
        std::free(direction);
        std::free(name);
    }
    snd_device_name_free_hint(hints);
    return MC_OK;
}

mc_result AlsaOut::ReadDefaultDevice(mc_output_device_fn read, void* user) {
    reportDevice(read, user, "default", "Default");
    return MC_OK;
}

mc_result AlsaOut::GetDefaultFormat(mc_string_view, mc_audio_format&) {
    /* ALSA may expose a range of rates without a preferred hardware rate. */
    return MC_ERROR_UNSUPPORTED;
}

mc_result AlsaOut::Prepare(const mc_output_options& options, const mc_output_callbacks& outputCallbacks) {
    std::lock_guard lock(this->mutex);
    this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : DefaultCapacity;
    if (this->capacity > std::numeric_limits<size_t>::max() / options.format.channels) {
        return MC_ERROR_BUFFER_TOO_SMALL;
    }

    int32_t latencyMs = LatencySetting.default_value;
    if (this->services && this->services->settings) {
        const auto* settings = this->services->settings;
        const auto result = settings->read_integer(settings->context, &LatencySetting, &latencyMs);
        if (result != MC_OK) {
            return result;
        }
    }

    const std::string device = options.device_id.length
        ? std::string(options.device_id.data, options.device_id.length) : "default";

    /* a stopped writer must be able to wake even if ALSA never becomes ready. */
    this->wake = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (this->wake < 0) {
        return MC_ERROR_IO;
    }
    if (snd_pcm_open(&this->pcm, device.c_str(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) {
        /* a listed device can disappear or become unavailable before open. */
        if (device == "default" ||
            snd_pcm_open(&this->pcm, "default", SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0)
        {
            return MC_ERROR_IO;
        }
    }
    if (snd_pcm_set_params(this->pcm, SND_PCM_FORMAT_FLOAT_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
        options.format.channels, options.format.sample_rate, 1,
        static_cast<unsigned>(latencyMs) * MicrosecondsPerMillisecond) < 0)
    {
        return MC_ERROR_IO;
    }

    const int count = snd_pcm_poll_descriptors_count(this->pcm);
    if (count < 0) {
        return MC_ERROR_IO;
    }
    this->descriptors.resize(static_cast<size_t>(count) + 1);
    if (snd_pcm_poll_descriptors(this->pcm, this->descriptors.data(), count) < 0) {
        return MC_ERROR_IO;
    }
    this->descriptors.back() = { .fd = this->wake, .events = POLLIN, .revents = 0 };

    /* keep the host's PCM untouched while applying software volume. */
    this->scaled.resize(this->capacity * options.format.channels);
    this->format = options.format;
    this->callbacks = outputCallbacks;
    this->volume = options.initial_volume;
    this->paused = false;
    this->scheduledFrames = 0;
    this->timingEstimator.Reset(this->format.sample_rate);
    this->stopped = false;
    return MC_OK;
}

mc_result AlsaOut::WaitForDevice() {
    int result;
    do {
        result = poll(this->descriptors.data(), this->descriptors.size(), -1);
    } while (result < 0 && errno == EINTR);
    return result < 0 ? MC_ERROR_IO : MC_OK;
}

void AlsaOut::EndOperation() noexcept {
    this->operationActive = false;
    this->operationEndedEvent.notify_all();
}

void AlsaOut::PublishTiming() noexcept {
    /* prepared PCM does not move until ALSA starts playback or drains it. */
    const auto state = snd_pcm_state(this->pcm);
    snd_pcm_sframes_t delay = 0;
    if ((state == SND_PCM_STATE_RUNNING || state == SND_PCM_STATE_DRAINING) &&
        snd_pcm_delay(this->pcm, &delay) >= 0)
    {
        const auto lead = std::max<snd_pcm_sframes_t>(0, delay) * 1e9 / this->format.sample_rate;
        this->timingEstimator.Publish(this->scheduledFrames,
            OutputTimingEstimator::Now() + static_cast<uint64_t>(lead));
    }
}

mc_result AlsaOut::QueueAudioBuffer(const mc_output_buffer& block) noexcept {
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

        this->operationActive = true;
        const size_t samples = block.pcm.frame_count * this->format.channels;
        /* a cubic curve gives the volume control a more natural low range. */
        const float volume = static_cast<float>(this->volume);
        const float gain = volume * volume * volume;
        for (size_t i = 0; i < samples; ++i) {
            this->scaled[i] = block.pcm.data[i] * gain;
        }
    }

    size_t written = 0;
    mc_result outcome = MC_OK;
    while (written < block.pcm.frame_count) {
        bool wait = false;
        {
            std::lock_guard lock(this->mutex);
            if (this->stopped) {
                outcome = MC_ERROR_CANCELLED;
                break;
            }

            const auto result = snd_pcm_writei(this->pcm, this->scaled.data() + written * this->format.channels,
                block.pcm.frame_count - written);
            if (result > 0) {
                written += static_cast<size_t>(result);
                this->scheduledFrames = this->timingEstimator.Submit(static_cast<size_t>(result));
                this->PublishTiming();
            }
            else if (result == -EAGAIN || result == 0) {
                wait = true;
            }
            else if (snd_pcm_recover(this->pcm, static_cast<int>(result), 1) < 0) {
                outcome = MC_ERROR_IO;
                break;
            }
            else if (result == -EPIPE) {
                /* an underrun means the previous run has already reached its tail. */
                this->timingEstimator.Publish(this->scheduledFrames, OutputTimingEstimator::Now());
            }
        }

        /* `Stop()` can signal the eventfd while this thread waits. */
        if (wait) {
            outcome = this->WaitForDevice();
            if (outcome != MC_OK) {
                break;
            }
        }
    }

    mc_output_callbacks callback;
    {
        std::lock_guard lock(this->mutex);
        if (this->stopped) {
            outcome = MC_ERROR_CANCELLED;
        }
        callback = this->callbacks;
    }

    /* return the borrowed buffer before allowing `Stop()` to close the device. */
    callback.completed(callback.context, block.token, outcome);

    {
        std::lock_guard lock(this->mutex);
        this->EndOperation();
    }

    return MC_OK;
}

mc_result AlsaOut::Pause() {
    std::lock_guard lock(this->mutex);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (snd_pcm_pause(this->pcm, 1) < 0) {
        return MC_ERROR_IO;
    }
    this->timingEstimator.Pause();
    this->paused = true;
    return MC_OK;
}

mc_result AlsaOut::Resume() {
    std::lock_guard lock(this->mutex);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->paused && snd_pcm_pause(this->pcm, 0) < 0) {
        return MC_ERROR_IO;
    }
    this->timingEstimator.Resume();
    this->paused = false;
    if (this->callbacks.ready) {
        this->callbacks.ready(this->callbacks.context);
    }
    return MC_OK;
}

mc_result AlsaOut::SetVolume(double value) {
    std::lock_guard lock(this->mutex);
    this->volume = value;
    return MC_OK;
}

double AlsaOut::GetVolume() const {
    std::lock_guard lock(this->mutex);
    return this->volume;
}

mc_result AlsaOut::Stop() noexcept {
    std::unique_lock lock(this->mutex);
    this->stopped = true;

    /* wake a blocked write or drain, then wait until its callback has returned. */
    if (this->wake >= 0) {
        const uint64_t signal = 1;
        ssize_t result;
        do {
            result = write(this->wake, &signal, sizeof(signal));
        } while (result < 0 && errno == EINTR);
        if (result < 0 && errno != EAGAIN) {
            std::fputs("ALSA output: cannot wake a stopped producer\n", stderr);
            std::abort();
        }
    }
    this->operationEndedEvent.wait(lock, [this] { return !this->operationActive; });

    this->timingEstimator.Reset(0);
    if (this->pcm) {
        snd_pcm_drop(this->pcm);
        if (snd_pcm_close(this->pcm) < 0) {
            std::fputs("ALSA output: cannot close the device\n", stderr);
            std::abort();
        }
        this->pcm = nullptr;
    }
    if (this->wake >= 0) {
        close(this->wake);
        this->wake = -1;
    }
    return MC_OK;
}

mc_result AlsaOut::Drain() {
    {
        std::lock_guard lock(this->mutex);
        if (this->stopped) {
            return MC_ERROR_CANCELLED;
        }
        if (this->paused || this->operationActive) {
            return MC_ERROR_IN_FLIGHT;
        }
        this->operationActive = true;
    }

    mc_result outcome = MC_OK;
    for (;;) {
        {
            std::lock_guard lock(this->mutex);
            if (this->stopped) {
                outcome = MC_ERROR_CANCELLED;
                break;
            }
            const int result = snd_pcm_drain(this->pcm);
            if (result >= 0) {
                break;
            }
            if (result != -EAGAIN) {
                outcome = MC_ERROR_IO;
                break;
            }
            this->PublishTiming();
        }

        outcome = this->WaitForDevice();
        if (outcome != MC_OK) {
            break;
        }
    }

    {
        std::lock_guard lock(this->mutex);
        /* ALSA needs preparation after a completed drain before it can play again. */
        if (this->stopped) {
            outcome = MC_ERROR_CANCELLED;
        }
        else if (outcome == MC_OK) {
            this->timingEstimator.Publish(this->scheduledFrames, OutputTimingEstimator::Now());
            if (snd_pcm_prepare(this->pcm) < 0) {
                outcome = MC_ERROR_IO;
            }
        }
        this->EndOperation();
    }
    return outcome;
}

mc_result AlsaOut::Flush() {
    std::lock_guard lock(this->mutex);
    if (this->stopped || !this->pcm) {
        return MC_ERROR_CANCELLED;
    }
    if (this->operationActive) {
        return MC_ERROR_IN_FLIGHT;
    }
    /* Drop queued frames and return the existing PCM handle to PREPARED. */
    if (snd_pcm_drop(this->pcm) < 0) {
        return MC_ERROR_IO;
    }
    this->scheduledFrames = 0;
    this->timingEstimator.Reset(this->format.sample_rate);
    if (snd_pcm_prepare(this->pcm) < 0) {
        return MC_ERROR_IO;
    }
    this->paused = false;
    if (this->callbacks.ready) {
        this->callbacks.ready(this->callbacks.context);
    }
    return MC_OK;
}
