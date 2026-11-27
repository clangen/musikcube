//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include "PipeWireOut.h"

#include <common/Contract.h>
#include <spa/param/audio/format-utils.h>
#include <spa/utils/dict.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

/* `Prepare()` starts a PipeWire thread loop, connects a stream to the selected
device, and waits for it to become paused or streaming. musikcore supplies
borrowed PCM blocks through `QueueAudioBuffer()`, which keeps them in a bounded
queue until PipeWire calls `Process()`. that callback copies samples into a
PipeWire buffer and returns each completed block to musikcore after the buffer
is accepted. `Stop()` cancels remaining blocks and shuts down the stream. */

namespace {
    constexpr size_t DefaultCapacity = 32768;
    constexpr int TimeoutSeconds = 5;
    constexpr char BufferSizeKey[] = "output_buffer_size_in_samples";
    constexpr char BufferCountKey[] = "output_buffer_count";
    constexpr mc_integer_setting Settings[] = {
        { .key = { BufferSizeKey, sizeof(BufferSizeKey) - 1 }, .default_value = 2048, .minimum = 256, .maximum = 32768 },
        { .key = { BufferCountKey, sizeof(BufferCountKey) - 1 }, .default_value = 16, .minimum = 8, .maximum = 64 },
    };

    void initializePipeWire() {
        static std::once_flag once;
        std::call_once(once, [] { pw_init(nullptr, nullptr); });
    }

    struct Discovery {
        pw_thread_loop* loop = nullptr;
        pw_context* context = nullptr;
        pw_core* core = nullptr;
        pw_registry* registry = nullptr;
        spa_hook coreListener {};
        spa_hook registryListener {};
        pw_core_events coreEvents {};
        pw_registry_events registryEvents {};
        std::vector<std::pair<std::string, std::string>> devices;
        int sequence = 0;
        bool done = false;
        bool failed = false;

        ~Discovery() {
            if (registry) {
                pw_proxy_destroy(reinterpret_cast<pw_proxy*>(registry));
            }
            if (core) {
                pw_core_disconnect(core);
            }
            if (context) {
                pw_context_destroy(context);
            }
            if (loop) {
                pw_thread_loop_destroy(loop);
            }
        }
    };

    void discoveryDone(void* user, uint32_t, int sequence) noexcept {
        auto& state = *static_cast<Discovery*>(user);
        if (sequence == state.sequence) {
            state.done = true;
            pw_thread_loop_signal(state.loop, false);
        }
    }

    void discoveryError(void* user, uint32_t, int, int, const char*) noexcept {
        auto& state = *static_cast<Discovery*>(user);
        state.failed = true;
        pw_thread_loop_signal(state.loop, false);
    }

    void discoveryGlobal(void* user, uint32_t id, uint32_t, const char* type, uint32_t,
        const spa_dict* properties) noexcept
    {
        if (!type || std::strcmp(type, PW_TYPE_INTERFACE_Device) != 0) {
            return;
        }
        auto& state = *static_cast<Discovery*>(user);
        const char* name = properties ? spa_dict_lookup(properties, "device.nick") : nullptr;
        if (!name || !*name) {
            name = properties ? spa_dict_lookup(properties, "alsa.card_name") : nullptr;
        }
        try {
            state.devices.emplace_back(std::to_string(id), name && *name ? name : "unknown device");
        }
        catch (...) {
            state.failed = true;
            pw_thread_loop_signal(state.loop, false);
        }
    }

    mc_result discover(Discovery& state) {
        initializePipeWire();
        state.loop = pw_thread_loop_new("musikcube-devices", nullptr);
        if (!state.loop) {
            return MC_ERROR_INTERNAL;
        }
        state.context = pw_context_new(pw_thread_loop_get_loop(state.loop), nullptr, 0);
        if (!state.context) {
            return MC_ERROR_INTERNAL;
        }
        state.core = pw_context_connect(state.context, nullptr, 0);
        if (!state.core) {
            return MC_ERROR_IO;
        }
        state.coreEvents.version = PW_VERSION_CORE_EVENTS;
        state.coreEvents.done = discoveryDone;
        state.coreEvents.error = discoveryError;
        pw_core_add_listener(state.core, &state.coreListener, &state.coreEvents, &state);
        state.registry = pw_core_get_registry(state.core, PW_VERSION_REGISTRY, 0);
        if (!state.registry) {
            return MC_ERROR_IO;
        }
        state.registryEvents.version = PW_VERSION_REGISTRY_EVENTS;
        state.registryEvents.global = discoveryGlobal;
        pw_registry_add_listener(state.registry, &state.registryListener, &state.registryEvents, &state);
        if (pw_thread_loop_start(state.loop) < 0) {
            return MC_ERROR_IO;
        }
        pw_thread_loop_lock(state.loop);
        state.sequence = pw_core_sync(state.core, PW_ID_CORE, 0);
        if (state.sequence < 0) {
            state.failed = true;
        }
        timespec deadline {};
        const int timed = pw_thread_loop_get_time(state.loop, &deadline, TimeoutSeconds * 1000000000LL);
        while (timed == 0 && !state.done && !state.failed) {
            if (pw_thread_loop_timed_wait_full(state.loop, &deadline) < 0) {
                state.failed = true;
            }
        }
        const bool success = timed == 0 && state.done && !state.failed;
        pw_thread_loop_unlock(state.loop);
        pw_thread_loop_stop(state.loop);
        return success ? MC_OK : MC_ERROR_IO;
    }
}

const mc_configuration_api& PipeWireConfiguration() {
    static const mc_configuration_api configuration {
        .struct_size = sizeof(mc_configuration_api),
        .integers = Settings,
        .integer_count = sizeof(Settings) / sizeof(Settings[0]),
        .booleans = nullptr,
        .boolean_count = 0,
        .numbers = nullptr,
        .number_count = 0,
        .texts = nullptr,
        .text_count = 0,
    };
    return configuration;
}

mc_result PipeWireOut::VisitDevices(mc_output_device_fn read, void* user) {
    Discovery discovery;
    const auto result = discover(discovery);
    if (result == MC_OK) {
        for (const auto& [id, name] : discovery.devices) {
            const mc_output_device device { .id = { id.data(), id.size() }, .name = { name.data(), name.size() } };
            read(user, &device);
        }
    }
    return result;
}

mc_result PipeWireOut::ReadDefaultDevice(mc_output_device_fn, void*) {
    return MC_ERROR_NOT_FOUND;
}

mc_result PipeWireOut::GetDefaultFormat(mc_string_view, mc_audio_format&) {
    return MC_ERROR_UNSUPPORTED;
}

mc_result PipeWireOut::Prepare(const mc_output_options& options, const mc_output_callbacks& outputCallbacks) {
    std::lock_guard lock(this->lifecycle);
    initializePipeWire();
    this->capacity = options.queue_capacity_frames ? options.queue_capacity_frames : DefaultCapacity;
    int32_t bufferSize = Settings[0].default_value;
    int32_t bufferCount = Settings[1].default_value;
    if (this->services && this->services->settings) {
        const auto* settings = this->services->settings;
        auto status = settings->read_integer(settings->context, &Settings[0], &bufferSize);
        if (status == MC_OK) {
            status = settings->read_integer(settings->context, &Settings[1], &bufferCount);
        }
        if (status != MC_OK) {
            return status;
        }
    }

    if (options.format.channels > SPA_AUDIO_MAX_CHANNELS ||
        static_cast<uint64_t>(bufferSize) * options.format.channels * sizeof(float) > INT32_MAX)
    {
        return MC_ERROR_UNSUPPORTED;
    }

    uint32_t target = PW_ID_ANY;
    if (options.device_id.length) {
        const auto begin = options.device_id.data;
        const auto end = begin + options.device_id.length;
        const auto parsed = std::from_chars(begin, end, target);
        if (parsed.ec != std::errc() || parsed.ptr != end) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
    }

    this->maxQueuedBuffers = static_cast<size_t>(bufferCount);
    this->format = options.format;
    this->callbacks = outputCallbacks;
    this->volume.store(static_cast<float>(options.initial_volume));
    musik::contract::Assert(this->queuedBuffers.Empty(), "PipeWire prepared with buffers still owned");
    this->queuedFrames = 0;
    this->paused = this->failed = this->draining = this->drained = false;
    this->stopped = false;
    this->events = {};
    this->events.version = PW_VERSION_STREAM_EVENTS;
    this->events.state_changed = StateChanged;
    this->events.process = Process;
    this->events.drained = Drained;

    this->loop = pw_thread_loop_new("musikcube-output", nullptr);
    if (!this->loop) {
        return MC_ERROR_INTERNAL;
    }
    if (pw_thread_loop_start(this->loop) < 0) {
        return MC_ERROR_IO;
    }
    this->loopStarted = true;

    pw_thread_loop_lock(this->loop);
    this->stream = pw_stream_new_simple(pw_thread_loop_get_loop(this->loop), "musikcube",
        pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE, "Music", nullptr),
        &this->events, this);
    if (!this->stream) {
        pw_thread_loop_unlock(this->loop);
        return MC_ERROR_IO;
    }

    uint8_t storage[4096];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(storage, sizeof(storage));
    spa_audio_info_raw audio {};
    audio.format = SPA_AUDIO_FORMAT_F32;
    audio.channels = this->format.channels;
    audio.rate = this->format.sample_rate;
    const spa_pod* params[2];
    params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audio);
    params[1] = static_cast<const spa_pod*>(spa_pod_builder_add_object(&builder,
        SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
        SPA_PARAM_BUFFERS_buffers, SPA_POD_Int(bufferCount),
        SPA_PARAM_BUFFERS_size, SPA_POD_Int(static_cast<int32_t>(bufferSize * options.format.channels * sizeof(float))),
        SPA_PARAM_BUFFERS_stride, SPA_POD_Int(static_cast<int32_t>(options.format.channels * sizeof(float)))));
    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS);
    if (!params[0] || !params[1] || pw_stream_connect(this->stream, PW_DIRECTION_OUTPUT, target, flags, params, 2) < 0) {
        pw_thread_loop_unlock(this->loop);
        return MC_ERROR_IO;
    }

    /* a server that never connects must not hold up output initialization. */
    timespec deadline {};
    const int timed = pw_thread_loop_get_time(this->loop, &deadline, TimeoutSeconds * 1000000000LL);
    while (timed == 0 && !this->failed && pw_stream_get_state(this->stream, nullptr) != PW_STREAM_STATE_PAUSED &&
        pw_stream_get_state(this->stream, nullptr) != PW_STREAM_STATE_STREAMING)
    {
        if (pw_thread_loop_timed_wait_full(this->loop, &deadline) < 0) {
            this->failed = true;
        }
    }
    const auto result = timed == 0 && !this->failed ? MC_OK : MC_ERROR_IO;
    pw_thread_loop_unlock(this->loop);
    return result;
}

void PipeWireOut::StateChanged(void* user, pw_stream_state, pw_stream_state state, const char*) noexcept {
    auto& output = *static_cast<PipeWireOut*>(user);
    if (state == PW_STREAM_STATE_ERROR) {
        output.failed = true;
        output.ReturnPending(MC_ERROR_IO);
        if (output.callbacks.ready) {
            output.callbacks.ready(output.callbacks.context);
        }
    }
    pw_thread_loop_signal(output.loop, false);
}

void PipeWireOut::ReturnPending(mc_result result) noexcept {
    while (auto* queuedBuffer = this->queuedBuffers.Front()) {
        const auto token = queuedBuffer->buffer.token;
        this->queuedBuffers.TryPop();
        this->callbacks.completed(this->callbacks.context, token, result);
    }
    this->queuedFrames = 0;
}

void PipeWireOut::Process(void* user) noexcept {
    auto& output = *static_cast<PipeWireOut*>(user);
    if (output.stopped || output.paused || output.failed) {
        return;
    }
    pw_buffer* buffer = pw_stream_dequeue_buffer(output.stream);
    if (!buffer) {
        return;
    }

    if (!buffer->buffer || buffer->buffer->n_datas == 0 || !buffer->buffer->datas[0].data ||
        !buffer->buffer->datas[0].chunk || buffer->buffer->datas[0].maxsize < output.format.channels * sizeof(float))
    {
        output.failed = true;
        output.ReturnPending(MC_ERROR_IO);
        if (output.callbacks.ready) {
            output.callbacks.ready(output.callbacks.context);
        }
        pw_thread_loop_signal(output.loop, false);
        pw_stream_queue_buffer(output.stream, buffer);
        return;
    }

    /* copy borrowed PCM into PipeWire's buffer before returning tokens to musikcore. */
    auto& data = buffer->buffer->datas[0];
    const size_t channels = output.format.channels;
    const size_t stride = channels * sizeof(float);
    const size_t maximum = data.maxsize / stride;
    auto* destination = static_cast<float*>(data.data);
    size_t frames = 0;
    std::array<uint64_t, MaxQueuedBuffers> completed {};
    size_t completedCount = 0;
    const float gain = output.volume.load(std::memory_order_relaxed);
    while (frames < maximum) {
        auto* queuedBuffer = output.queuedBuffers.Front();
        if (!queuedBuffer) {
            break;
        }
        const size_t remaining = queuedBuffer->buffer.pcm.frame_count - queuedBuffer->framesCopied;
        const size_t copy = std::min(maximum - frames, remaining);
        const float* source = queuedBuffer->buffer.pcm.data + queuedBuffer->framesCopied * channels;
        for (size_t i = 0; i < copy * channels; ++i) {
            destination[frames * channels + i] = source[i] * gain;
        }
        frames += copy;
        queuedBuffer->framesCopied += copy;
        output.queuedFrames -= copy;
        if (queuedBuffer->framesCopied == queuedBuffer->buffer.pcm.frame_count) {
            completed[completedCount++] = queuedBuffer->buffer.token;
            output.queuedBuffers.TryPop();
        }
    }

    const bool madeSpace = frames > 0;
    if (!frames && !output.draining) {
        std::memset(destination, 0, maximum * stride);
        frames = maximum;
    }
    data.chunk->offset = 0;
    data.chunk->stride = static_cast<int32_t>(stride);
    data.chunk->size = static_cast<uint32_t>(frames * stride);

    /* only report success once PipeWire has accepted the copied samples. */
    const bool accepted = pw_stream_queue_buffer(output.stream, buffer) >= 0;
    for (size_t i = 0; i < completedCount; ++i) {
        output.callbacks.completed(output.callbacks.context, completed[i], accepted ? MC_OK : MC_ERROR_IO);
    }
    if (!accepted) {
        output.failed = true;
        output.ReturnPending(MC_ERROR_IO);
    }
    if ((madeSpace || !accepted) && output.callbacks.ready) {
        output.callbacks.ready(output.callbacks.context);
    }
    if (completedCount || !accepted) {
        pw_thread_loop_signal(output.loop, false);
    }
}

void PipeWireOut::Drained(void* user) noexcept {
    auto& output = *static_cast<PipeWireOut*>(user);
    output.drained = true;
    pw_thread_loop_signal(output.loop, false);
}

mc_result PipeWireOut::QueueAudioBuffer(const mc_output_buffer& block) noexcept {
    std::lock_guard lock(this->lifecycle);
    if (!this->loop || this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    pw_thread_loop_lock(this->loop);
    mc_result result = MC_OK;
    if (this->failed) {
        result = MC_ERROR_IO;
    }
    else if (block.pcm.format.sample_rate != this->format.sample_rate ||
        block.pcm.format.channels != this->format.channels ||
        block.pcm.format.channel_layout != this->format.channel_layout)
    {
        result = MC_ERROR_INVALID_ARGUMENT;
    }
    else if (block.pcm.frame_count > this->capacity) {
        result = MC_ERROR_BUFFER_TOO_SMALL;
    }
    else if (this->queuedBuffers.Size() == this->maxQueuedBuffers ||
        this->queuedFrames > this->capacity - block.pcm.frame_count)
    {
        result = MC_ERROR_QUEUE_FULL;
    }
    else {
        const QueuedBuffer queuedBuffer {
            .buffer = block,
            .framesCopied = 0,
        };
        if (this->queuedBuffers.TryPush(queuedBuffer)) {
            this->queuedFrames += block.pcm.frame_count;
        }
        else {
            result = MC_ERROR_QUEUE_FULL;
        }
    }
    pw_thread_loop_unlock(this->loop);
    return result;
}

mc_result PipeWireOut::Pause() {
    std::lock_guard lock(this->lifecycle);
    if (!this->loop || this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    pw_thread_loop_lock(this->loop);
    const int result = pw_stream_set_active(this->stream, false);
    if (result >= 0) {
        this->paused = true;
    }
    pw_thread_loop_unlock(this->loop);
    return result < 0 ? MC_ERROR_IO : MC_OK;
}

mc_result PipeWireOut::Resume() {
    std::lock_guard lock(this->lifecycle);
    if (!this->loop || this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    pw_thread_loop_lock(this->loop);
    const int result = pw_stream_set_active(this->stream, true);
    if (result >= 0) {
        this->paused = false;
        if (this->callbacks.ready && this->queuedBuffers.Size() < this->maxQueuedBuffers &&
            this->queuedFrames < this->capacity)
        {
            this->callbacks.ready(this->callbacks.context);
        }
    }
    pw_thread_loop_unlock(this->loop);
    return result < 0 ? MC_ERROR_IO : MC_OK;
}

mc_result PipeWireOut::SetVolume(double value) {
    this->volume.store(static_cast<float>(value));
    return MC_OK;
}

double PipeWireOut::GetVolume() const {
    return this->volume.load();
}

mc_result PipeWireOut::Stop() noexcept {
    std::unique_lock lock(this->lifecycle);
    if (!this->loop) {
        this->stopped = true;
        return MC_OK;
    }

    if (this->loopStarted) {
        pw_thread_loop_lock(this->loop);
        this->stopped = true;
        if (this->stream) {
            pw_stream_set_active(this->stream, false);
            pw_stream_flush(this->stream, false);
        }
        this->ReturnPending(MC_ERROR_CANCELLED);
        pw_thread_loop_signal(this->loop, false);
        pw_thread_loop_unlock(this->loop);

        /* drains still use the loop after waking, so join them before stopping it. */
        this->finished.wait(lock, [this] { return this->activeDrains == 0; });
        pw_thread_loop_stop(this->loop);
        this->loopStarted = false;
    }

    if (this->stream) {
        pw_stream_destroy(this->stream);
        this->stream = nullptr;
    }
    pw_thread_loop_destroy(this->loop);
    this->loop = nullptr;
    return MC_OK;
}

mc_result PipeWireOut::Drain() {
    std::unique_lock lock(this->lifecycle);
    if (!this->loop || this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    pw_thread_loop_lock(this->loop);
    if (this->paused) {
        pw_thread_loop_unlock(this->loop);
        return MC_ERROR_IN_FLIGHT;
    }

    ++this->activeDrains;
    auto* loop = this->loop;
    lock.unlock();
    this->draining = true;
    this->drained = false;

    /* first return queued buffers to musikcore, then wait for PipeWire's own drain. */
    while (!this->queuedBuffers.Empty() && !this->stopped && !this->failed) {
        pw_thread_loop_wait(loop);
    }
    if (!this->stopped && !this->failed && pw_stream_flush(this->stream, true) < 0) {
        this->failed = true;
    }
    while (!this->drained && !this->stopped && !this->failed) {
        pw_thread_loop_wait(loop);
    }

    const auto result = this->stopped ? MC_ERROR_CANCELLED : this->failed ? MC_ERROR_IO : MC_OK;
    this->draining = false;
    pw_thread_loop_unlock(loop);
    lock.lock();
    --this->activeDrains;
    this->finished.notify_all();
    return result;
}

mc_result PipeWireOut::Flush() {
    std::lock_guard lock(this->lifecycle);
    if (!this->loop || this->stopped) {
        return MC_ERROR_CANCELLED;
    }
    pw_thread_loop_lock(this->loop);
    /* Prevent the process callback from copying old PCM while both the
    PipeWire queue and the host's borrowed buffers are discarded. */
    const int stopped = pw_stream_set_active(this->stream, false);
    const int flushed = stopped < 0 ? stopped : pw_stream_flush(this->stream, false);
    if (flushed >= 0) {
        this->ReturnPending(MC_ERROR_CANCELLED);
    }
    const int resumed = flushed < 0 ? flushed : pw_stream_set_active(this->stream, true);
    if (resumed >= 0) {
        this->paused = false;
        this->draining = false;
        this->drained = false;
        if (this->callbacks.ready) {
            this->callbacks.ready(this->callbacks.context);
        }
    }
    pw_thread_loop_signal(this->loop, false);
    pw_thread_loop_unlock(this->loop);
    return resumed < 0 ? MC_ERROR_IO : MC_OK;
}
