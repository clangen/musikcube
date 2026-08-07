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

/// @file NullOut.h
/// @brief No-op audio output plugin.
/// @details Implements the output SDK but discards all audio, so playback runs
/// as fast as possible without a physical device. Useful for headless testing,
/// transcoding benchmarks and environments with no audio hardware.

#include "config.h"

#include <musikcore/sdk/IOutput.h>

using namespace musik::core::sdk;

/** @brief Output that discards all audio data.
 *  @details Accepts every buffer and immediately reports it as processed. The
 *  playback state is tracked so the transport behaves normally (play, pause,
 *  resume, stop), but no sound is ever produced. */
class NullOut : public IOutput {
    public:
        /** @brief Constructs a stopped output. */
        NullOut();
        /** @brief Destroys the output. */
        ~NullOut();

        /* IPlugin */
        /** @brief Returns the plugin name.
         *  @return Always returns "Null". */
        const char* Name() override { return "Null"; };
        /** @brief Destroys the output instance. */
        void Release() override;

        /* IOutput */
        /** @brief Pauses playback. */
        void Pause() override;
        /** @brief Resumes playback after a pause. */
        void Resume() override;
        /** @brief Sets the playback volume.
         *  @param volume Volume in the range 0.0 (mute) to 1.0 (full). */
        void SetVolume(double volume) override;
        /** @brief Returns the current playback volume.
         *  @return Volume in the range 0.0 to 1.0. */
        double GetVolume() override;
        /** @brief Stops playback. */
        void Stop() override;
        /** @brief Processes a buffer by immediately discarding it.
         *  @param buffer The buffer to consume.
         *  @param provider The provider owning the buffer.
         *  @return The new output state. */
        OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        /** @brief Returns the output latency.
         *  @return Always returns 0.0. */
        double Latency() override;
        /** @brief Blocks until all queued audio has been processed. */
        void Drain() override;
        /** @brief Returns the list of available devices.
         *  @return An empty device list. */
        IDeviceList* GetDeviceList() override;
        /** @brief Selecting a device has no effect.
         *  @param deviceId Device identifier (ignored).
         *  @return Always returns true. */
        bool SetDefaultDevice(const char* deviceId) override;
        /** @brief Returns the default output device.
         *  @return Always returns null. */
        IDevice* GetDefaultDevice() override;
        /** @brief Returns the preferred sample rate.
         *  @return Always returns -1 (no preference). */
        int GetDefaultSampleRate() override;

    private:
        /** @brief Internal playback state. */
        enum State {
            /** @brief Playback is stopped. */
            StateStopped,
            /** @brief Playback is paused. */
            StatePaused,
            /** @brief Playback is active. */
            StatePlaying
        };

        /** @brief Current playback state. */
        State state;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
};
