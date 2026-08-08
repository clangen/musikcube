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

/// @file WaveOut.h
/// @brief Audio output plugin built on the Windows multimedia waveOut API.
/// @details Implements the output SDK using legacy waveOut* functions. PCM
/// buffers are queued to the device and recycled asynchronously as they finish
/// playing. Supports device enumeration and selection, volume control,
/// pause/resume and stop. Windows-only.

#include "config.h"
#include <deque>
#include <memory>
#include <mutex>
#include "WaveOutBuffer.h"
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IDevice.h>

using namespace musik::core::sdk;

/** @brief Legacy waveOut API output for Windows.
 *  @details Enqueues buffers to the waveOut device from a dedicated thread that
 *  pumps the callback messages. Finished buffers are released back to their
 *  IBufferProvider. */
class WaveOut : public IOutput {
    public:
        /** @brief Shared pointer to a waveOut buffer. */
        typedef std::shared_ptr<WaveOutBuffer> WaveOutBufferPtr;

        /** @brief Constructs a stopped output. */
        WaveOut();
        /** @brief Destroys the output and closes the device. */
        ~WaveOut();

        /* IPlugin */
        /** @brief Returns the plugin name.
         *  @return Always returns "WaveOut". */
        const char* Name() override { return "WaveOut"; };
        /** @brief Destroys the output instance. */
        void Release() override;

        /* IOutput */
        /** @brief Pauses audio playback. */
        void Pause() override;
        /** @brief Resumes audio playback after a pause. */
        void Resume() override;
        /** @brief Sets the playback volume.
         *  @param volume Volume in the range 0.0 (mute) to 1.0 (full). */
        void SetVolume(double volume) override;
        /** @brief Returns the current playback volume.
         *  @return Volume in the range 0.0 to 1.0. */
        double GetVolume() override;
        /** @brief Stops playback and clears queued buffers. */
        void Stop() override;
        /** @brief Queues a buffer for playback.
         *  @param buffer The PCM buffer to play.
         *  @param provider The provider that owns the buffer.
         *  @return An OutputState indicating whether the output is playing or paused. */
        OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        /** @brief Returns the output latency.
         *  @return Always returns 0.0. */
        double Latency() override { return 0.0; }
        /** @brief Draining is handled synchronously by waveOut.
         *  @note No-op for this output. */
        void Drain() override { }
        /** @brief Returns the list of available waveOut devices.
         *  @return A newly allocated IDeviceList the caller must release. */
        IDeviceList* GetDeviceList() override;
        /** @brief Selects the default output device.
         *  @param deviceId Identifier of the device to use.
         *  @return True if the device was selected successfully. */
        bool SetDefaultDevice(const char* deviceId) override;
        /** @brief Returns the default output device.
         *  @return The currently configured default device. */
        IDevice* GetDefaultDevice() override;
        /** @brief Returns the preferred sample rate.
         *  @return Always returns -1 (no preference). */
        int GetDefaultSampleRate() override { return -1; }

        /** @brief Recycles a buffer after it has finished playing.
         *  @param buffer The buffer that finished playing. */
        void OnBufferWrittenToOutput(WaveOutBuffer *buffer);

        /** @brief Thread procedure pumping the waveOut callback messages.
         *  @param params The WaveOut instance.
         *  @return 0 on exit. */
        static DWORD WINAPI WaveCallbackThreadProc(LPVOID params);

    private:
        /** @brief Gives WaveOutBuffer access to the queue. */
        friend class WaveOutBuffer;

        /** @brief Negotiates the wave format from a buffer.
         *  @param buffer The buffer whose format will be used. */
        void SetFormat(IBuffer *buffer);
        /** @brief Starts the callback message pump thread. */
        void StartWaveOutThread();
        /** @brief Stops the callback message pump thread. */
        void StopWaveOutThread();
        /** @brief Releases all queued buffers back to their providers. */
        void ClearBufferQueue();
        /** @brief Returns a buffer to its provider.
         *  @param buffer The buffer to process. */
        void NotifyBufferProcessed(WaveOutBufferPtr buffer);

        /** @brief Resolves the preferred device id.
         *  @return The waveOut device id. */
        UINT GetPreferredDeviceId();

        /** @brief Returns an empty buffer from the free list.
         *  @return A reusable buffer, or null. */
        WaveOutBufferPtr GetEmptyBuffer();

        /* note we apparently use a std::list<> here, and not std::set<> because
        when we need to do a lookup we have a WaveOutBuffer*, and not a shared_ptr.
        we could fix this up by using std::enable_shared_from_this */
        /** @brief Queue of waveOut buffers. */
        typedef std::deque<WaveOutBufferPtr> BufferList;

        /* instance state relating to output device, including the thread that
        drives the callback message pump */
        /** @brief Handle to the open waveOut device. */
        HWAVEOUT waveHandle;
        /** @brief Negotiated wave format. */
        WAVEFORMATPCMEX waveFormat;
        /** @brief Id and handle of the callback thread. */
        DWORD threadId;
        HANDLE threadHandle;

        /* stream information. */
        /** @brief Number of interleaved channels. */
        int currentChannels;
        /** @brief Sample rate in Hz. */
        long currentSampleRate;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double currentVolume;
        /** @brief Whether playback is active. */
        bool playing;

        /* a queue of buffers we've received from the core Player, and have enqueued
        to the output device. we need to notify the IBufferProvider when they have finished
        playing. */
        /** @brief Buffers currently in flight to the device. */
        BufferList queuedBuffers;
        /** @brief Buffers available for reuse. */
        BufferList freeBuffers;

        /* used to protect access to the WaveOut and message pump */
        /** @brief Guards the output device and message pump. */
        std::recursive_mutex outputDeviceMutex;

        /* used to protect access to the queue of buffers that in flight */
        /** @brief Guards the in-flight buffer queue. */
        std::recursive_mutex bufferQueueMutex;
};
