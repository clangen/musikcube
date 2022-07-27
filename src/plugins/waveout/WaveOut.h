//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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
#include <deque>
#include <memory>
#include <mutex>
#include "WaveOutBuffer.h"
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IDevice.h>

using namespace musik::core::sdk;

class WaveOut : public IOutput {
    public:
        typedef std::shared_ptr<WaveOutBuffer> WaveOutBufferPtr;

        WaveOut();
        ~WaveOut();

        /* IPlugin */
        const char* Name() override { return "WaveOut"; };
        void Release() override;

        /* IOutput */
        void Pause() override;
        void Resume() override;
        void SetVolume(double volume) override;
        double GetVolume() override;
        void Stop() override;
        OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        double Latency() override { return 0.0; }
        void Drain() override { }
        IDeviceList* GetDeviceList() override;
        bool SetDefaultDevice(const char* deviceId) override;
        IDevice* GetDefaultDevice() override;
        int GetDefaultSampleRate() override { return -1; }

        void OnBufferWrittenToOutput(WaveOutBuffer *buffer);

        static DWORD WINAPI WaveCallbackThreadProc(LPVOID params);

    private:
        friend class WaveOutBuffer;

        void SetFormat(IBuffer *buffer);
        void StartWaveOutThread();
        void StopWaveOutThread();
        void ClearBufferQueue();
        void NotifyBufferProcessed(WaveOutBufferPtr buffer);

        UINT GetPreferredDeviceId();

        WaveOutBufferPtr GetEmptyBuffer();

        /* note we apparently use a std::list<> here, and not std::set<> because
        when we need to do a lookup we have a WaveOutBuffer*, and not a shared_ptr.
        we could fix this up by using boost::enable_shared_from_this */
        typedef std::deque<WaveOutBufferPtr> BufferList;

        /* instance state relating to output device, including the thread that
        drives the callback message pump */
        HWAVEOUT waveHandle;
        WAVEFORMATPCMEX waveFormat;
        DWORD threadId;
        HANDLE threadHandle;

        /* stream information. */
        int currentChannels;
        long currentSampleRate;
        double currentVolume;
        bool playing;

        /* a queue of buffers we've received from the core Player, and have enqueued
        to the output device. we need to notify the IBufferProvider when they have finished
        playing. */
        BufferList queuedBuffers;
        BufferList freeBuffers;

        /* used to protect access to the WaveOut and message pump */
        std::recursive_mutex outputDeviceMutex;

        /* used to protect access to the queue of buffers that in flight */
        std::recursive_mutex bufferQueueMutex;
};
