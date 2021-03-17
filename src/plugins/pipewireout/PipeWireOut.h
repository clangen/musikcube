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

#pragma once

#include <musikcore/sdk/IOutput.h>
#include <pipewire/pipewire.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <deque>
#include <condition_variable>

using namespace musik::core::sdk;

class PipeWireOut : public IOutput {
    public:
        PipeWireOut();
        ~PipeWireOut();

        /* IPlugin */
        const char* Name() override { return "PipeWire"; };
        void Release() override;

        /* IOutput */
        void Pause() override;
        void Resume() override;
        void SetVolume(double volume) override;
        double GetVolume() override;
        void Stop() override;
        OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        double Latency() override;
        void Drain() override;
        IDeviceList* GetDeviceList() override;
        bool SetDefaultDevice(const char* deviceId) override;
        IDevice* GetDefaultDevice() override;

    private:
        bool StartPipeWire(IBuffer* buffer);
        void StopPipeWire();
        void DiscardInputBuffers();

        static void OnStreamStateChanged(
            void* userdata,
            enum pw_stream_state old,
            enum pw_stream_state state,
            const char* error);

        static void OnStreamProcess(void* userdata);

        static void OnDrained(void* userdata);

        struct InBufferContext {
            InBufferContext(IBuffer* buffer, IBufferProvider* provider) {
                this->buffer = buffer; this->provider = provider;
                this->readPtr = (char*) buffer->BufferPointer();
                this->remaining = (uint32_t) buffer->Bytes();
            }
            void Advance(int count) {
                bool release = count >= remaining;
                this->remaining -= count;
                this->readPtr += count;
                if (release) {
                    this->provider->OnBufferProcessed(this->buffer);
                    delete this;
                }
            }
            void Discard() {
                this->provider->OnBufferProcessed(this->buffer);
                delete this;
            }
            IBuffer* buffer{nullptr};
            IBufferProvider* provider{nullptr};
            uint32_t remaining{0};
            char* readPtr;
        };

        struct OutBufferContext {
            void Initialize(pw_buffer* buffer) {
                this->buffer = buffer;
                if (buffer) {
                    struct spa_buffer* spaBuffer = buffer->buffer;
                    this->writePtr = (char*) spaBuffer->datas[0].data;
                    this->remaining = spaBuffer->datas[0].maxsize;
                    this->total = this->remaining;
                }
                else {
                    this->Reset();
                }
            }
            void Reset() {
                this->buffer = nullptr;
                this->writePtr = nullptr;
                this->remaining = 0;
                this->total = 0;
            }
            void Advance(int count) {
                this->remaining -= count;
                this->writePtr += count;
            }
            void Finalize(pw_stream* stream, uint32_t stride) {
                if (this->Valid()) {
                    spa_data& data = this->buffer->buffer->datas[0];
                    data.chunk->offset = 0;
                    data.chunk->stride = stride;
                    data.chunk->size = this->total - this->remaining;
                    pw_stream_queue_buffer(stream, this->buffer);
                    this->Reset();
                }
            }
            bool Valid() {
                return this->buffer != nullptr;
            }
            pw_buffer* buffer{nullptr};
            uint32_t remaining{0};
            uint32_t total{0};
            char* writePtr{nullptr};
        };

        enum class State {
            Stopped, Paused, Playing, Shutdown
        };

        std::deque<InBufferContext*> buffers;
        std::recursive_mutex mutex;
        std::atomic<bool> initialized{false};
        std::atomic<State> state{State::Stopped};
        std::condition_variable_any bufferCondition, drainCondition;
        double volume{1.0};
        pw_stream_events pwStreamEvents;
        pw_thread_loop* pwThreadLoop {nullptr};
        pw_stream* pwStream{nullptr};
        OutBufferContext outBufferContext;
        long channelCount{0};
        long sampleRate{0};
};
