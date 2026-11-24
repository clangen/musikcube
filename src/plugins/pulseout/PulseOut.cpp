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

#include <pulse/pulseaudio.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
    constexpr size_t DefaultCapacity = 32768;
    constexpr char LinearVolumeKey[] = "force_linear_volume";

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
    std::lock_guard lock(this->mutex);
    if (options.format.channels > PA_CHANNELS_MAX) {
        return MC_ERROR_UNSUPPORTED;
    }
    this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : DefaultCapacity;
    if (this->capacity > std::numeric_limits<size_t>::max() / options.format.channels / sizeof(float)) {
        return MC_ERROR_BUFFER_TOO_SMALL;
    }

    this->linearVolume = false;
    if (this->services && this->services->preferences) {
        const auto* prefs = this->services->preferences;
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
    const std::string device(options.device_id.data ? options.device_id.data : "", options.device_id.length);
    int error = 0;
    this->connection = pa_blocking_new(nullptr, "musikcube", PA_STREAM_PLAYBACK,
        device.empty() ? nullptr : device.c_str(), "musikcube", &spec, nullptr, nullptr, &error);
    if (!this->connection && !device.empty()) {
        /* a listed device can disappear or become unavailable before open. */
        this->connection = pa_blocking_new(nullptr, "musikcube", PA_STREAM_PLAYBACK,
            nullptr, "musikcube", &spec, nullptr, nullptr, &error);
    }
    if (!this->connection) {
        return MC_ERROR_IO;
    }

    this->format = options.format;
    this->callbacks = outputCallbacks;
    this->volume = options.initial_volume;
    const int normalized = this->linearVolume
        ? static_cast<int>(std::round(PA_VOLUME_NORM * this->volume))
        : static_cast<int>(pa_sw_volume_from_linear(this->volume));
    if (pa_blocking_set_volume(this->connection, normalized, &error) < 0) {
        return MC_ERROR_IO;
    }
    this->paused = false;
    this->stopped = false;
    return MC_OK;
}

mc_result PulseOut::QueueAudioBuffer(const mc_output_buffer& block) noexcept {
    std::unique_lock lock(this->mutex);
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
    if (this->active) {
        return MC_ERROR_IN_FLIGHT;
    }

    /* `Stop()` can cancel the write while the mutex is released. the connection
    remains alive until the completion callback returns. */
    this->active = true;
    auto* connection = this->connection;
    lock.unlock();
    int error = 0;
    const int result = pa_blocking_write(connection, block.pcm.data,
        block.pcm.frame_count * this->format.channels * sizeof(float), &error);
    lock.lock();

    const auto outcome = this->stopped ? MC_ERROR_CANCELLED : result < 0 ? MC_ERROR_IO : MC_OK;
    const auto callback = this->callbacks;
    lock.unlock();
    callback.completed(callback.context, block.token, outcome);
    lock.lock();
    this->active = false;
    this->finished.notify_all();
    return MC_OK;
}

mc_result PulseOut::Pause() {
    pa_blocking* connection;
    {
        std::lock_guard lock(this->mutex);
        if (this->stopped) {
            return MC_ERROR_CANCELLED;
        }
        this->controlActive = true;
        connection = this->connection;
    }

    const int result = pa_blocking_cork(connection, true, nullptr);
    std::lock_guard lock(this->mutex);
    this->controlActive = false;
    this->finished.notify_all();
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (result < 0) {
        return MC_ERROR_IO;
    }
    this->paused = true;
    return MC_OK;
}

mc_result PulseOut::Resume() {
    pa_blocking* connection;
    bool paused;
    {
        std::lock_guard lock(this->mutex);
        if (this->stopped) {
            return MC_ERROR_CANCELLED;
        }
        this->controlActive = true;
        connection = this->connection;
        paused = this->paused;
    }

    const int result = paused ? pa_blocking_cork(connection, false, nullptr) : 0;
    std::lock_guard lock(this->mutex);
    this->controlActive = false;
    this->finished.notify_all();
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (result < 0) {
        return MC_ERROR_IO;
    }
    this->paused = false;
    if (this->callbacks.ready) {
        this->callbacks.ready(this->callbacks.context);
    }
    return MC_OK;
}

mc_result PulseOut::SetVolume(double value) {
    pa_blocking* connection;
    int normalized;
    {
        std::lock_guard lock(this->mutex);
        if (!this->connection || this->stopped) {
            this->volume = value;
            return MC_OK;
        }
        this->controlActive = true;
        connection = this->connection;
        normalized = this->linearVolume
            ? static_cast<int>(std::round(PA_VOLUME_NORM * value))
            : static_cast<int>(pa_sw_volume_from_linear(value));
    }

    const int result = pa_blocking_set_volume(connection, normalized, nullptr);
    std::lock_guard lock(this->mutex);
    this->controlActive = false;
    this->finished.notify_all();
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (result < 0) {
        return MC_ERROR_IO;
    }
    this->volume = value;
    return MC_OK;
}

double PulseOut::GetVolume() const {
    std::lock_guard lock(this->mutex);
    return this->volume;
}

mc_result PulseOut::Stop() noexcept {
    std::unique_lock lock(this->mutex);
    this->stopped = true;

    /* unblock a writer or drain before freeing the connection they use. */
    if (this->connection) {
        pa_blocking_cancel(this->connection);
    }
    this->finished.wait(lock, [this] { return !this->active && !this->controlActive; });

    if (this->connection) {
        pa_blocking_free(this->connection);
        this->connection = nullptr;
    }
    return MC_OK;
}

mc_result PulseOut::Drain() {
    std::unique_lock lock(this->mutex);
    if (this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    if (this->paused || this->active) {
        return MC_ERROR_IN_FLIGHT;
    }

    /* keep the connection alive while `Stop()` interrupts the unlocked drain. */
    this->active = true;
    auto* connection = this->connection;
    lock.unlock();
    const int result = pa_blocking_drain(connection, nullptr);
    lock.lock();

    const auto outcome = this->stopped ? MC_ERROR_CANCELLED : result < 0 ? MC_ERROR_IO : MC_OK;
    this->active = false;
    this->finished.notify_all();
    return outcome;
}
