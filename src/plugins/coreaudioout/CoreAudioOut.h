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

#pragma once

#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IBufferProvider.h>
#include <deque>
#include <mutex>
#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>

class CoreAudioOut : public musik::core::sdk::IOutput {
    public:
        struct BufferContext {
            musik::core::sdk::IBuffer *buffer;
            musik::core::sdk::IBufferProvider *provider;
        };

        CoreAudioOut();
        virtual ~CoreAudioOut();

        /* IPlugin */
        const char* Name() override { return "CoreAudio"; }

        /* IOutput */
        void Release() override;
        void Pause() override;
        void Resume() override;
        void SetVolume(double volume) override;
        double GetVolume() override;
        void Stop() override;
        double Latency() override { return 0.0; }
        void Drain() override;

        musik::core::sdk::OutputState Play(
            musik::core::sdk::IBuffer *buffer,
            musik::core::sdk::IBufferProvider *provider) override;

        musik::core::sdk::IDeviceList* GetDeviceList() override;
        bool SetDefaultDevice(const char* deviceId) override;
        musik::core::sdk::IDevice* GetDefaultDevice() override;
        int GetDefaultSampleRate() override { return -1; }

        void NotifyBufferCompleted(BufferContext *context);

    private:
        enum State {
            StateStopped,
            StatePaused,
            StatePlaying
        };

        AudioStreamBasicDescription audioFormat;
        AudioQueueRef audioQueue;
        double volume;
        int bufferCount;
        std::recursive_mutex mutex;
        bool quit;
        State state;
};
