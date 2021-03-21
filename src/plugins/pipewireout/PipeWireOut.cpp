//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/ISchema.h>
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/String.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/utils/result.h>
#include <unistd.h>
#include <chrono>

constexpr const char* TAG = "PipeWireOut";
constexpr size_t SAMPLES_PER_BUFFER = 2048;
constexpr size_t SAMPLE_SIZE_BYTES = sizeof(float);
constexpr size_t MAX_BUFFERS = 16;
constexpr const char* PREF_DEVICE_ID = "device_id";

static std::atomic<bool> pipeWireInitialized(false);

static IPreferences* prefs = nullptr;
static IDebug* debug = nullptr;

extern "C" void SetDebug(IDebug* debug) {
    ::debug = debug;
}

extern "C" void SetPreferences(IPreferences* prefs) {
    ::prefs = prefs;
    prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    prefs->Save();
}

extern "C" musik::core::sdk::ISchema* GetSchema() {
    auto schema = new TSchema<>();
    return schema;
}

static std::string getDeviceId() {
    return getPreferenceString<std::string>(prefs, PREF_DEVICE_ID, "");
}

void PipeWireOut::OnStreamStateChanged(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error) {
    ::debug->Info(TAG, str::format("state changed from %d to %d", old, state).c_str());
}

void PipeWireOut::OnCoreDone(void* userdata, uint32_t id, int seq) {
    auto context = static_cast<DeviceListContext*>(userdata);
    if (seq == context->eventId) {
        ::debug->Info(TAG, "device refresh finished");
        pw_main_loop_quit(context->loop);
    }
}

void PipeWireOut::OnCoreError(void* userdata, uint32_t id, int seq, int res, const char *message) {
    auto context = static_cast<DeviceListContext*>(userdata);
    pw_main_loop_quit(context->loop);
}

void PipeWireOut::OnRegistryGlobal(void *userdata, uint32_t id, uint32_t permissions, const char *type, uint32_t version, const struct spa_dict *props) {
    auto context = static_cast<DeviceListContext*>(userdata);
    if (std::string(type) == "PipeWire:Interface:Device") {
        std::string formattedName = "unknown device";
        if (props) {
            auto dict = pw_properties_new_dict(props);
            auto name = pw_properties_get(dict, "device.nick");
            if (!name || !strlen(name)) name = pw_properties_get(dict, "alsa.card_name");
            if (!name || !strlen(name)) name = pw_properties_get(dict, "api.alsa.card.name");
            if (!name || !strlen(name)) name = "unknown device";
            auto addr = pw_properties_get(dict, "object.path");
            if (!addr || !strlen(addr)) addr = pw_properties_get(dict, "device.bus-path");
            formattedName = name;
            if (addr && strlen(addr)) {
                formattedName += " (" + std::string(addr) + ")";
            }
            pw_properties_free(dict);
        }
        ::debug->Info(TAG, str::format("detected PipeWire:Interface:Device with id=%d", id).c_str());
        context->instance->deviceList.Add(std::to_string(id), formattedName);
    }
}

void PipeWireOut::OnDrained(void* data) {
    ::debug->Info(TAG, "drained");
    PipeWireOut* self = static_cast<PipeWireOut*>(data);
    self->drainCondition.notify_all();
}

void PipeWireOut::OnStreamProcess(void* data) {
    PipeWireOut* self = static_cast<PipeWireOut*>(data);

    {
        std::unique_lock<std::recursive_mutex> lock(self->mutex);

        if (self->state != State::Playing) {
            return;
        }

        OutBufferContext& outContext = self->outBufferContext;

        if (!outContext.Valid()) {
            outContext.Initialize(pw_stream_dequeue_buffer(self->pwStream));
            if (!outContext.Valid()) {
                // std::cerr << "[PipeWire] no more output buffers available to fill\n";
                return;
            }
        }

        uint32_t channelCount;

        while (outContext.remaining > 0 && !self->buffers.empty()) {
            //std::cerr << "[PipeWire] " << outContext.remaining << " bytes still need to be filled...\n";

            InBufferContext* inContext = self->buffers.front();
            channelCount = (uint32_t) inContext->buffer->Channels();
            uint32_t inBufferSize = (uint32_t) inContext->buffer->Bytes();
            uint32_t bytesToCopy = std::min(outContext.remaining, inContext->remaining);

            if (outContext.remaining >= inContext->remaining) {
                self->buffers.pop_front();
                self->bufferCondition.notify_all();
            }

            memcpy(outContext.writePtr, inContext->readPtr, bytesToCopy);
            inContext->Advance(bytesToCopy); /* will `delete this` if emptied */
            outContext.Advance(bytesToCopy);
        }

        if (outContext.remaining == 0) {
            outContext.Finalize(self->pwStream, SAMPLE_SIZE_BYTES * channelCount);
        }
    }
}

PipeWireOut::PipeWireOut() {
    this->pwStreamEvents = { PW_VERSION_STREAM_EVENTS };
    this->pwStreamEvents.state_changed = PipeWireOut::OnStreamStateChanged;
    this->pwStreamEvents.process = PipeWireOut::OnStreamProcess;
    this->pwStreamEvents.drained = PipeWireOut::OnDrained;
}

PipeWireOut::~PipeWireOut() {
    this->StopPipeWire();
}

void PipeWireOut::Release() {
    delete this;
}

void PipeWireOut::Pause() {
    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->state = State::Paused;
    }
    {
        if (this->pwThreadLoop && this->pwStream) {
            pw_thread_loop_lock(this->pwThreadLoop);
            pw_stream_set_active(this->pwStream, false);
            pw_thread_loop_unlock(this->pwThreadLoop);
        }
    }
}

void PipeWireOut::Resume() {
    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->state = State::Playing;
    }
    {
        if (this->pwThreadLoop && this->pwStream) {
            pw_thread_loop_lock(this->pwThreadLoop);
            pw_stream_set_active(this->pwStream, true);
            pw_thread_loop_unlock(this->pwThreadLoop);
        }
    }
}

void PipeWireOut::SetVolume(double volume) {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    if (this->pwThreadLoop && this->pwStream) {
        pw_thread_loop_lock(this->pwThreadLoop);
        float* channelVolumes = new float[this->channelCount];
        for (long i = 0; i < this->channelCount; i++) {
            channelVolumes[i] = volume;
        }
        pw_stream_set_control(
            this->pwStream,
            SPA_PROP_channelVolumes,
            this->channelCount,
            channelVolumes,
            0);
        delete[] channelVolumes;
        pw_thread_loop_unlock(this->pwThreadLoop);
    }
    this->volume = volume;
}

double PipeWireOut::GetVolume() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    return this->volume;
}

void PipeWireOut::Stop() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    this->DiscardInputBuffers();
    this->state = State::Stopped;
    if (this->pwThreadLoop && this->pwStream) {
        pw_thread_loop_lock(this->pwThreadLoop);
        pw_stream_set_active(this->pwStream, false);
        pw_stream_flush(this->pwStream, false);
        pw_thread_loop_unlock(this->pwThreadLoop);
    }
}

void PipeWireOut::DiscardInputBuffers() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    for (auto& buffer : this->buffers) {
        buffer->Discard();
    }
    this->buffers.clear();
    this->bufferCondition.notify_all();
}

void PipeWireOut::Drain() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    while (this->buffers.size()) {
        bufferCondition.wait(lock);
    }
    if (this->pwThreadLoop && this->pwStream) {
        pw_thread_loop_lock(this->pwThreadLoop);
        pw_stream_flush(this->pwStream, true);
        pw_thread_loop_unlock(this->pwThreadLoop);
        drainCondition.wait_for(lock, std::chrono::milliseconds(10000));
    }
}

IDeviceList* PipeWireOut::GetDeviceList() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    this->RefreshDeviceList();
    return this->deviceList.Clone();
}

bool PipeWireOut::SetDefaultDevice(const char* deviceId) {
    if (getDeviceId() != deviceId) {
        setDefaultDevice<IPreferences, Device, IOutput>(prefs, this, PREF_DEVICE_ID, deviceId);
    }
    return true;
}

IDevice* PipeWireOut::GetDefaultDevice() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);
    this->RefreshDeviceList();
    return this->deviceList.ResolveDevice(getDeviceId());
}

void PipeWireOut::StopPipeWire() {
    if (::debug) ::debug->Info(TAG, "shutdown started");

    this->Stop();

    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->state = State::Shutdown;
    }

    if (this->pwThreadLoop) {
        pw_thread_loop_stop(this->pwThreadLoop);

        if (this->pwStream) {
            this->outBufferContext.Finalize(
                this->pwStream,
                SAMPLE_SIZE_BYTES * this->channelCount);
            pw_stream_destroy(this->pwStream);
            this->pwStream = nullptr;
        }

        pw_thread_loop_destroy(this->pwThreadLoop);
        this->pwThreadLoop = nullptr;
    }

    this->initialized = false;
    this->channelCount = 0;
    this->sampleRate = 0;

    if (::debug) ::debug->Info(TAG, "shutdown complete");
}

bool PipeWireOut::StartPipeWire(IBuffer* buffer) {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    this->pwThreadLoop = pw_thread_loop_new("musikcube", nullptr);
    if (this->pwThreadLoop) {
        int result;

        if ((result = pw_thread_loop_start(this->pwThreadLoop)) != 0) {
            ::debug->Error(TAG, str::format("error starting thread loop: %s", spa_strerror(result)).c_str());
            goto cleanup;
        };

        pw_thread_loop_lock(this->pwThreadLoop);

        this->pwStream = pw_stream_new_simple(
            pw_thread_loop_get_loop(this->pwThreadLoop),
            "musikcube",
            pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CATEGORY, "Playback",
                PW_KEY_MEDIA_ROLE, "Music",
                NULL),
            &this->pwStreamEvents,
            this);

        if (this->pwStream) {
            uint8_t builderBuffer[4096];
            spa_pod_builder builder = SPA_POD_BUILDER_INIT(builderBuffer, sizeof(builderBuffer));
            const spa_pod *params[1];

            this->channelCount = buffer->Channels();
            this->sampleRate = buffer->SampleRate();

            spa_audio_info_raw audioInfo;
            spa_zero(audioInfo);
            audioInfo.format = SPA_AUDIO_FORMAT_F32;
            audioInfo.channels = (uint32_t) this->channelCount;
            audioInfo.rate = (uint32_t) this->sampleRate;

            params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audioInfo);

            if (!params[0]) {
                ::debug->Error(TAG, str::format(
                    "failed to create audio format. channels=%d, rate=%d",
                    this->channelCount,
                    this->sampleRate).c_str());
                goto cleanup;
            }

            // params[1] = (spa_pod*) spa_pod_builder_add_object(
            //     &builder,
            //     SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
            //     SPA_PARAM_BUFFERS_buffers, SPA_POD_Int(MAX_BUFFERS),
            //     SPA_PARAM_BUFFERS_size, SPA_POD_Int(SAMPLES_PER_BUFFER * SAMPLE_SIZE_BYTES * buffer->Channels()),
            //     SPA_PARAM_BUFFERS_stride, SPA_POD_Int(SAMPLE_SIZE_BYTES * audioInfo.channels));

            pw_stream_flags streamFlags = (pw_stream_flags)(
                PW_STREAM_FLAG_AUTOCONNECT |
                PW_STREAM_FLAG_MAP_BUFFERS);

            result = pw_stream_connect(
                this->pwStream,
                PW_DIRECTION_OUTPUT,
                this->deviceList.ResolveId(getDeviceId()),
                streamFlags,
                params,
                1);

            if (result == 0) {
                pw_thread_loop_unlock(this->pwThreadLoop);
                ::debug->Info(TAG, "new stream created and connected successfully");
                this->initialized = true;
                return true;
            }
            else {
                ::debug->Error(TAG, str::format(
                    "error starting stream: %s",
                    spa_strerror(result)).c_str());
            }
        }
    }

cleanup:
    pw_thread_loop_unlock(this->pwThreadLoop);
    ::debug->Error(TAG, "stream not initialized");
    this->StopPipeWire();
    return false;
}

OutputState PipeWireOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    if (!this->initialized) {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        if (!pipeWireInitialized) {
            pw_init(nullptr, nullptr);
            pipeWireInitialized = true;
        }
        this->RefreshDeviceList();
        if (!this->StartPipeWire(buffer)) {
            return OutputState::InvalidState;
        }
        this->SetVolume(this->volume);
    }

    if (this->channelCount != buffer->Channels() || this->sampleRate != buffer->SampleRate()) {
        State lastState = this->state;
        this->Drain();
        this->StopPipeWire();
        if (!this->StartPipeWire(buffer)) {
            return OutputState::InvalidState;
        }
        this->state = lastState;
    }

    if (this->state != State::Playing) {
        return OutputState::InvalidState;
    }

    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        if (this->buffers.size() >= MAX_BUFFERS) {
            return OutputState::BufferFull;
        }
        this->buffers.push_back(new InBufferContext(buffer, provider));
        bufferCondition.notify_all();
    }

    return OutputState::BufferWritten;
}

double PipeWireOut::Latency() {
    /* CAL TODO: i see how to set latency, but not a good way to query
    PipeWire to see what it actually is */
    return 0.0;
}

void PipeWireOut::RefreshDeviceList() {
    /* https://github.com/PipeWire/pipewire/blob/master/src/tools/pw-dump.c */

    if (!pipeWireInitialized) {
        pw_init(nullptr, nullptr);
        pipeWireInitialized = true;
    }

    DeviceListContext deviceListContext(this);

    deviceListContext.loop = pw_main_loop_new(nullptr);
    if (!deviceListContext.loop) {
        ::debug->Error(TAG, "RefreshDeviceList() could not create main loop");
        return;
    }

    auto loop = pw_main_loop_get_loop(deviceListContext.loop);
    if (!loop) {
        ::debug->Error(TAG, "RefreshDeviceList() could not resolve loop from main_loop??");
        return;
    }

    deviceListContext.context = pw_context_new(loop, nullptr, 0);
    if (!deviceListContext.context) {
        ::debug->Error(TAG, "RefreshDeviceList() could not create context");
        return;
    }

    deviceListContext.core = pw_context_connect(deviceListContext.context, nullptr, 0);
    if (deviceListContext.core == nullptr) {
        ::debug->Error(TAG, "RefreshDeviceList() could not connect to core");
        return;
    }

    pw_core_add_listener(
        deviceListContext.core,
        &deviceListContext.coreListener,
        &deviceListContext.coreEvents,
        &deviceListContext);

    deviceListContext.registry = pw_core_get_registry(
        deviceListContext.core, PW_VERSION_REGISTRY, 0);

    if (deviceListContext.registry) {
        pw_registry_add_listener(
            deviceListContext.registry,
            &deviceListContext.registryListener,
            &deviceListContext.registryEvents,
            &deviceListContext);

        deviceListContext.eventId = pw_core_sync(
            deviceListContext.core, PW_ID_CORE, 0);

        this->deviceList.Reset();

        pw_main_loop_run(deviceListContext.loop);
    }
}
