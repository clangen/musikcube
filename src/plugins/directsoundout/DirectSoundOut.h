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

/** @file DirectSoundOut.h
 *  @brief DirectSound (Windows) audio output plugin.
 *  @details Implements the musikcube output SDK on top of the legacy Windows
 *  DirectSound8 API. PCM data is written into a secondary buffer which is
 *  mixed to the selected output device.
 */

#include <deque>
#include <memory>
#include <mutex>
#include <atomic>

#include <Windows.h>
#include <Mmsystem.h>
#include <Mmreg.h>
#include <KS.h>
#include <Ksmedia.h>
#include <dsound.h>

#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IDevice.h>

using namespace musik::core::sdk;

/** @brief DirectSound-based output for Windows.
 *  @details Plays back raw PCM audio through a DirectSound8 secondary buffer.
 *  Supports device enumeration and selection, volume, pause/resume, stop and
 *  drain operations. The write position is tracked against the primary buffer.
 */
class DirectSoundOut : public IOutput {
    public:
        DirectSoundOut();
        ~DirectSoundOut();

        /* IPlugin */
        /** @brief Returns the plugin name. @return Always returns "DirectSound". */
        const char* Name() override { return "DirectSound"; };
        /** @brief Destroys the output instance and releases DirectSound objects. */
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
        /** @brief Stops playback and discards queued buffers. */
        void Stop() override;
        /** @brief Queues a buffer for playback.
         *  @param buffer The PCM buffer to play.
         *  @param provider The provider that owns the buffer.
         *  @return An OutputState indicating whether the output is playing or paused. */
        OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        /** @brief Returns the output latency in seconds.
         *  @return The measured latency in seconds. */
        double Latency() override;
        /** @brief Blocks until all queued audio has been played. */
        void Drain() override;
        /** @brief Returns the list of available DirectSound devices.
         *  @return A newly allocated IDeviceList the caller must release. */
        IDeviceList* GetDeviceList() override;
        /** @brief Selects the default output device.
         *  @param deviceId Identifier of the device to use.
         *  @return True if the device was selected successfully. */
        bool SetDefaultDevice(const char* deviceId) override;
        /** @brief Returns the default output device.
         *  @return The currently configured default device. */
        IDevice* GetDefaultDevice() override;
        /** @brief Returns the preferred sample rate, or -1 for none.
         *  @return Always returns -1. */
        int GetDefaultSampleRate() override { return -1; }

    private:
        /** @brief Internal playback state. */
        enum State {
            /** @brief Output is stopped. */
            StateStopped,
            /** @brief Output is actively playing. */
            StatePlaying,
            /** @brief Output is paused. */
            StatePaused
        };

        /** @brief Configures the DirectSound buffers for a given PCM format.
         *  @param buffer A buffer whose format drives the configuration.
         *  @return True if the configuration succeeded. */
        bool Configure(IBuffer *buffer);
        /** @brief Rebuilds the DirectSound buffers. */
        void Reset();
        /** @brief Clears pending buffer state after a reset. */
        void ResetBuffers();
        /** @brief Resolves the preferred device GUID.
         *  @return The GUID of the preferred output device, or null for default. */
        LPCGUID GetPreferredDeviceId();

        /** @brief Current playback state, updated atomically. */
        std::atomic<State> state;

        /** @brief Wave format description of the PCM stream. */
        WAVEFORMATEXTENSIBLE waveFormat;
        /** @brief DirectSound8 object used to create buffers. */
        IDirectSound8 *outputContext;
        /** @brief Primary (mixer) buffer. */
        IDirectSoundBuffer *primaryBuffer;
        /** @brief Secondary buffer receiving the PCM samples. */
        IDirectSoundBuffer8 *secondaryBuffer;
        /** @brief Size of the secondary buffer in bytes. */
        DWORD bufferSize;
        /** @brief Next write position within the secondary buffer. */
        DWORD writeOffset;
        /** @brief Sample rate in Hz. */
        int rate;
        /** @brief Number of interleaved channels. */
        int channels;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
        /** @brief Measured output latency in seconds. */
        double latency;
        /** @brief True after the first buffer has been written. */
        bool firstBufferWritten;
        /** @brief Guards state changes. */
        std::recursive_mutex stateMutex;
};
