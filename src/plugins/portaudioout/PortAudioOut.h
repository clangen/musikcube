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

#pragma once

#include "config.h"

#include <musikcore/sdk/IOutput.h>
#include <portaudio.h>
#include <deque>
#include <mutex>
#include <memory>
#include <condition_variable>

using namespace musik::core::sdk;

class PortAudioOut : public IOutput {
    public:
        PortAudioOut();
        ~PortAudioOut();

        /* IPlugin */
        const char* Name() override { return "PortAudio"; };
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
        int GetDefaultSampleRate() override;

    friend int portAudioStreamCallback(
        const void *input,
        void *output,
        unsigned long frameCount,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData);

    private:
        enum State {
            StateStopped,
            StatePaused,
            StatePlaying,
            StateDraining,
        };

        struct FormatContext {
            bool IsSame(IBuffer* buffer) {
                return
                    this->sampleRate == buffer->SampleRate() &&
                    this->channels == buffer->Channels();
            }
            void From(IBuffer* buffer) {
                this->sampleRate = buffer->SampleRate();
                this->channels = buffer->Channels();
            }
            long sampleRate { 0 };
            long channels { 0 };
        };

        struct BufferContext {
            BufferContext(IBuffer* buffer, IBufferProvider* provider) {
                this->buffer = buffer;
                this->provider = provider;
                this->remainingFrameCount = buffer->Samples() / buffer->Channels();
            }
            IBuffer* buffer { nullptr };
            IBufferProvider* provider { nullptr };
            unsigned long framesWritten { 0 };
            unsigned long remainingFrameCount { 0 };
            float gain { -1.0 };
        };

        std::recursive_mutex mutex;
        std::condition_variable_any bufferEvent;
        PaStream* paStream { nullptr };
        IDeviceList* deviceList { nullptr };
        std::deque<std::shared_ptr<BufferContext>> buffers;
        FormatContext formatContext;
        State state;
        double volume;
};
