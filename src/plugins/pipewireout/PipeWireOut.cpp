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

constexpr size_t SAMPLES_PER_BUFFER = 2048;
constexpr size_t SAMPLE_SIZE_BYTES = sizeof(float);
constexpr size_t MAX_BUFFERS = 16;

static IPreferences* prefs = nullptr;

extern "C" void SetPreferences(IPreferences* prefs) {
    ::prefs = prefs;
}

extern "C" musik::core::sdk::ISchema* GetSchema() {
    auto schema = new TSchema<>();
    return schema;
}

void PipeWireOut::OnStreamStateChanged(void* data, enum pw_stream_state old, enum pw_stream_state state, const char* error) {
    std::cerr << "[PipeWire] state changed from " << old << " to " << state << "\n";
}

void PipeWireOut::OnStreamProcess(void* data) {
    PipeWireOut* self = static_cast<PipeWireOut*>(data);

	struct pw_buffer* pwBuffer;

	if ((pwBuffer = pw_stream_dequeue_buffer(self->pwStream)) == nullptr) {
        std::cerr << "[PipeWire] no more output buffers available to fill\n";
		return;
	}

	struct spa_buffer* spaBuffer = pwBuffer->buffer;
    auto& outBufferData = spaBuffer->datas[0];
    char* outBufferPtr = (char*) outBufferData.data;
    uint32_t outBufferRemaining = outBufferData.maxsize;
    int channelCount = 2;

    static size_t D_PROCESSED = 0;

    while (outBufferRemaining > 0) {
        BufferContext* inContext = nullptr;
        uint32_t inBufferSize = 0;
        uint32_t inBufferRemaining = 0;
        uint32_t bytesToCopy = 0;

        //std::cerr << "[PipeWire] " << outBufferRemaining << " bytes still need to be filled...\n";

        {
            std::unique_lock<std::recursive_mutex> lock(self->mutex);
            while (self->buffers.empty() && self->state == State::Playing) {
                self->bufferCondition.wait(lock);
            }

            if (self->state != State::Playing) {
                //std::cerr << "[PipeWire] not playing, so buffers will not be filled.\n";
                return;
            }

            inContext = self->buffers.front();

            channelCount = inContext->buffer->Channels();
            inBufferSize = (uint32_t) inContext->buffer->Bytes();
            inBufferRemaining = inContext->remaining;

            if (outBufferRemaining >= inBufferRemaining) {
                self->buffers.pop_front();
            }

            bytesToCopy = std::min(outBufferRemaining, inBufferRemaining);
        }

        //std::cerr << "[PipeWire] popped buffer #" << D_PROCESSED++ << ". Will fill with " << bytesToCopy << " bytes.\n";
        memcpy(outBufferPtr, inContext->readPtr, bytesToCopy);
        inContext->Advance(bytesToCopy); /* will auto release if empty */
        outBufferRemaining -= bytesToCopy;
        outBufferPtr += bytesToCopy;
    }

    outBufferData.chunk->offset = 0;
    outBufferData.chunk->stride = SAMPLE_SIZE_BYTES * channelCount;
    outBufferData.chunk->size = outBufferData.maxsize;

    pw_stream_queue_buffer(self->pwStream, pwBuffer);
}

PipeWireOut::PipeWireOut() {
    this->pwStreamEvents = {
        PW_VERSION_STREAM_EVENTS,
    };
    this->pwStreamEvents.state_changed = PipeWireOut::OnStreamStateChanged;
    this->pwStreamEvents.process = OnStreamProcess;
}

PipeWireOut::~PipeWireOut() {
    this->StopPipeWire();
}

void PipeWireOut::Release() {
    delete this;
}

void PipeWireOut::Pause() {
    this->state = State::Paused;
    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->bufferCondition.notify_all();
    }
}

void PipeWireOut::Resume() {
    this->state = State::Playing;
    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->bufferCondition.notify_all();
    }
}

void PipeWireOut::SetVolume(double volume) {
    //pw_stream_set_control(stream->stream, SPA_PROP_channelVolumes, stream->volume.channels, stream->volume.values, 0);
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

        this->state = State::Stopped;
        this->bufferCondition.notify_all();

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
            &this->pwStreamEvents,
            this);

        if (this->pwStream) {
            uint8_t builderBuffer[4096];
            spa_pod_builder builder = SPA_POD_BUILDER_INIT(builderBuffer, sizeof(builderBuffer));
            const spa_pod *params[2];

            spa_audio_info_raw audioInfo;
            spa_zero(audioInfo);
            audioInfo.format = SPA_AUDIO_FORMAT_F32;
            audioInfo.channels = (uint32_t) buffer->Channels();
            audioInfo.rate = (uint32_t) buffer->SampleRate();

            params[0] = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &audioInfo);

            if (!params[0]) {
                std::cerr << "[PipeWire] failed to create audio format\n";
                goto cleanup;
            }

            params[1] = (spa_pod*) spa_pod_builder_add_object(
                &builder,
                SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                SPA_PARAM_BUFFERS_buffers, SPA_POD_Int(MAX_BUFFERS),
                SPA_PARAM_BUFFERS_size, SPA_POD_Int(SAMPLES_PER_BUFFER * SAMPLE_SIZE_BYTES * buffer->Channels()),
                SPA_PARAM_BUFFERS_stride, SPA_POD_Int(SAMPLE_SIZE_BYTES * audioInfo.channels));

            pw_stream_flags streamFlags = (pw_stream_flags)(
                PW_STREAM_FLAG_AUTOCONNECT |
                PW_STREAM_FLAG_MAP_BUFFERS);

            result = pw_stream_connect(
                this->pwStream,
                PW_DIRECTION_OUTPUT,
                PW_ID_ANY,
                streamFlags,
                params,
                2);

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

    if (this->state != State::Playing) {
        return OutputState::InvalidState;
    }

    {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        if (this->buffers.size() >= MAX_BUFFERS) {
            return OutputState::BufferFull;
        }
        this->buffers.push_back(new BufferContext(buffer, provider));
        this->bufferCondition.notify_all();
    }

    return OutputState::BufferWritten;
}

double PipeWireOut::Latency() {
    return 0.0;
}
