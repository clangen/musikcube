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
#include <spa/param/audio/format-utils.h>
#include <spa/utils/result.h>
#include <unistd.h>
#include <iostream>

static IPreferences* prefs = nullptr;

extern "C" void SetPreferences(IPreferences* prefs) {
    ::prefs = prefs;
}

extern "C" musik::core::sdk::ISchema* GetSchema() {
    auto schema = new TSchema<>();
    return schema;
}

static void onStreamStageChanged(
    void *data,
    pw_stream_state old,
    pw_stream_state state,
    const char *error)
{
    std::cerr << "[PipeWire] state changed from " << old << " to " << state << "\n";
}

static void onStreamProcess(void *userdata) {
    std::cerr << "clclcl onProcess\n";
}

static const struct pw_stream_events streamEvents = {
    PW_VERSION_STREAM_EVENTS,
    .process = onStreamProcess,
    .state_changed = onStreamStageChanged
};

PipeWireOut::PipeWireOut() {
}

PipeWireOut::~PipeWireOut() {
}

void PipeWireOut::Release() {
    delete this;
}

void PipeWireOut::Pause() {
    this->state = State::Paused;
}

void PipeWireOut::Resume() {
    this->state = State::Playing;
}

void PipeWireOut::SetVolume(double volume) {
    this->volume = volume;
}

double PipeWireOut::GetVolume() {
    return this->volume;
}

void PipeWireOut::Stop() {
    this->state = State::Stopped;
}

void PipeWireOut::Drain() {
}

IDeviceList* PipeWireOut::GetDeviceList() {
    return nullptr;
}

bool PipeWireOut::SetDefaultDevice(const char* deviceId) {
    return false;
}

IDevice* PipeWireOut::GetDefaultDevice() {
    return nullptr;
}

void PipeWireOut::StopPipeWire() {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    if (this->pwThreadLoop) {
        pw_thread_loop_stop(this->pwThreadLoop);
        pw_thread_loop_destroy(this->pwThreadLoop);
        this->pwThreadLoop = nullptr;
    }

    if (this->pwStream) {
        pw_stream_destroy(this->pwStream);
        this->pwStream = nullptr;
    }

    this->initialized = false;
}

bool PipeWireOut::StartPipeWire(IBuffer* buffer) {
    std::unique_lock<std::recursive_mutex> lock(this->mutex);

    pw_init(nullptr, nullptr);

    this->pwThreadLoop = pw_thread_loop_new("musikcube", nullptr);
    if (this->pwThreadLoop) {
        int result;

        if ((result = pw_thread_loop_start(this->pwThreadLoop)) != 0) {
            std::cerr << "[PipeWire] error starting thread loop: " << spa_strerror(result) << "\n";
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
            &streamEvents,
            this);

        if (this->pwStream) {
            uint8_t intBuffer[1024];

            spa_pod_builder builder = 
                SPA_POD_BUILDER_INIT(intBuffer, sizeof(intBuffer));

            const spa_pod *params[1];

            spa_audio_info_raw audioInfo = SPA_AUDIO_INFO_RAW_INIT(
                .format = SPA_AUDIO_FORMAT_F32,
                .channels = (uint32_t) buffer->Channels(),
                .rate = (uint32_t) buffer->SampleRate());

            params[0] = spa_format_audio_raw_build(
                &builder, SPA_PARAM_EnumFormat, &audioInfo);

            result = pw_stream_connect(
                this->pwStream,
                PW_DIRECTION_OUTPUT,
                PW_ID_ANY,
                (pw_stream_flags)(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS),
                params,
                1);

            if (result == 0) {
                pw_thread_loop_unlock(this->pwThreadLoop);
                std::cerr << "[PipeWire] stream created and connected\n";
                this->initialized = true;
                return true;
            }
            else {
                std::cerr << "[PipeWire] error starting stream: " << spa_strerror(result) << "\n";
            }
        }
    }

cleanup:
    pw_thread_loop_unlock(this->pwThreadLoop);
    std::cerr << "[PipeWire] stream not initialized.\n";
    this->StopPipeWire();
    return false;
}

OutputState PipeWireOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    if (!this->initialized) {
        if (!this->StartPipeWire(buffer)) {
            return OutputState::InvalidState;
        }
    }

    // std::cerr << "clclcl here\n";

    /* TODO: if buffer format changes, drain, then update the params! */

    if (this->state == State::Paused) {
        return OutputState::InvalidState;
    }

    /* order of operations matters, otherwise overflow. */
    int micros = ((buffer->Samples() * 1000) / buffer->SampleRate() * 1000) / buffer->Channels();
    usleep((long)((float) micros));
    provider->OnBufferProcessed(buffer);
    return OutputState::BufferWritten;
}

double PipeWireOut::Latency() {
    return 0.0;
}
