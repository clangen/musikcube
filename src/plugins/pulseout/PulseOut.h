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

/// @file PulseOut.h
/// @brief Audio output plugin built on the PulseAudio sound server.
/// @details Implements the output SDK using a simple blocking PulseAudio
/// connection (pulse_blocking_stream.h). PCM is written synchronously to the
/// server through the blocking API, with support for device enumeration,
/// volume control (hardware or software), pause/resume, stop and drain.
/// Linux-only.

#include "pch.h"

#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IDevice.h>

#include <mutex>
#include "pulse_blocking_stream.h"

/** @brief PulseAudio-based output for Linux.
 *  @details Owns a pa_blocking connection opened for playback. Buffers are
 *  written to the server immediately. Volume is applied through the stream
 *  volume when hardware volume is available, otherwise via a software gain. */
class PulseOut : public musik::core::sdk::IOutput {
    public:
        /** @brief Constructs a stopped output. */
        PulseOut();
        /** @brief Destroys the output and closes the connection. */
        virtual ~PulseOut();

        /* IPlugin */
        /** @brief Returns the plugin name.
         *  @return Always returns "PulseAudio". */
        const char* Name() override { return "PulseAudio"; }

        /* IOutput */
        /** @brief Destroys the output instance. */
        void Release() override;
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
        /** @brief Stops playback and discards queued audio. */
        void Stop() override;
        /** @brief Returns the output latency in seconds.
         *  @return The measured latency in seconds. */
        double Latency() override;
        /** @brief Blocks until all queued audio has been played by the daemon. */
        void Drain() override;

        /** @brief Plays a buffer by writing it to the PulseAudio connection.
         *  @param buffer The PCM buffer to play.
         *  @param provider The provider that owns the buffer.
         *  @return An OutputState indicating whether the output is playing or paused. */
        musik::core::sdk::OutputState Play(
            musik::core::sdk::IBuffer *buffer,
            musik::core::sdk::IBufferProvider *provider) override;

        /** @brief Returns the list of available PulseAudio devices.
         *  @return A newly allocated IDeviceList the caller must release. */
        musik::core::sdk::IDeviceList* GetDeviceList() override;
        /** @brief Selects the default output device.
         *  @param deviceId Identifier of the device to use.
         *  @return True if the device was selected successfully. */
        bool SetDefaultDevice(const char* deviceId) override;
        /** @brief Returns the default output device.
         *  @return The currently configured default device. */
        musik::core::sdk::IDevice* GetDefaultDevice() override;
        /** @brief Returns the preferred sample rate.
         *  @return Always returns -1 (no preference). */
        int GetDefaultSampleRate() override { return -1; }

    private:
        /** @brief Playback state of the output. */
        enum State {
            /** @brief Playback is stopped. */
            StateStopped,
            /** @brief Playback is paused. */
            StatePaused,
            /** @brief Playback is active. */
            StatePlaying
        };

        /** @brief Opens the PulseAudio connection for a buffer's format.
         *  @param buffer A buffer whose sample format will be used. */
        void OpenDevice(musik::core::sdk::IBuffer *buffer);
        /** @brief Closes the PulseAudio connection. */
        void CloseDevice();
        /** @brief Resolves the preferred sink device identifier.
         *  @return The sink id string, or empty for the default. */
        std::string GetPreferredDeviceId();

        /** @brief Guards the output state. */
        std::recursive_mutex stateMutex;
        /** @brief The blocking PulseAudio connection. */
        pa_blocking* audioConnection;
        /** @brief Current playback state. */
        State state;
        /** @brief Number of channels and sample rate in Hz. */
        int channels, rate;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
        /** @brief Whether the volume was changed since the last write. */
        bool volumeUpdated;
        /** @brief Whether the volume is applied in hardware (linear). */
        bool linearVolume;
};
