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

/// @file WasapiOut.h
/// @brief Audio output plugin built on Windows WASAPI.
/// @details Implements the output SDK using the Windows Audio Session API. PCM
/// is copied into WASAPI render buffers on the event-driven callback. Supports
/// device enumeration and selection, volume control through the session
/// volume, pause/resume, stop, drain and sample-rate queries. Windows-only.

#include <deque>
#include <memory>
#include <mutex>
#include <atomic>

#include <mmdeviceapi.h>
#include <Audioclient.h>

#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IDevice.h>

using namespace musik::core::sdk;

/** @brief Notification client for audio device change events. */
class NotificationClient;

/** @brief WASAPI-based output for Windows.
 *  @details Owns an IMMDeviceEnumerator, the selected IMMDevice and an
 *  IAudioClient configured in shared/event-driven mode. The render client
 *  fills device buffers with queued PCM. When the default device changes the
 *  audio client is reset and reconfigured. */
class WasapiOut : public IOutput {
    public:
        /** @brief Constructs a stopped output. */
        WasapiOut();
        /** @brief Destroys the output and releases WASAPI resources. */
        ~WasapiOut();

        /* IPlugin */
        /** @brief Returns the plugin name.
         *  @return Always returns "WASAPI". */
        const char* Name() override { return "WASAPI"; };
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
        /** @brief Blocks until all queued audio has been written to the device. */
        void Drain() override;
        /** @brief Returns the list of available WASAPI devices.
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
         *  @return The device's sample rate, or -1. */
        int GetDefaultSampleRate() override;

        /** @brief Marks the output for reconfiguration when the device changes. */
        void OnDeviceChanged() { this->deviceChanged = true; }

    private:
        /** @brief Playback state of the output. */
        enum State {
            /** @brief Playback is stopped. */
            StateStopped,
            /** @brief Playback is active. */
            StatePlaying,
            /** @brief Playback is paused. */
            StatePaused
        };

        /** @brief Configures the audio client for a buffer's format.
         *  @param buffer The buffer whose format will be negotiated.
         *  @return True if the client was configured. */
        bool Configure(IBuffer *buffer);
        /** @brief Initializes the audio client and render services.
         *  @return True on success. */
        bool InitializeAudioClient();
        /** @brief Resets the output to a stopped state. */
        void Reset();
        /** @brief Resolves the preferred output device.
         *  @return The device, or null. */
        IMMDevice* GetPreferredDevice();

        /** @brief Audio device enumerator. */
        IMMDeviceEnumerator *enumerator;
        /** @brief The selected audio device. */
        IMMDevice *device;
        /** @brief The audio client session. */
        IAudioClient *audioClient;
        /** @brief Audio clock for position queries. */
        IAudioClock *audioClock;
        /** @brief Render client for filling device buffers. */
        IAudioRenderClient *renderClient;
        /** @brief Session volume control. */
        ISimpleAudioVolume *simpleAudioVolume;
        /** @brief Per-channel stream volume control. */
        IAudioStreamVolume *audioStreamVolume;
        /** @brief Device-change notification client. */
        NotificationClient *notificationClient;
        /** @brief Size of the output buffer in frames. */
        UINT32 outputBufferFrames;
        /** @brief Current playback state. */
        std::atomic<State> state;
        /** @brief Negotiated wave format. */
        WAVEFORMATEXTENSIBLE waveFormat;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
        /** @brief Measured output latency in seconds. */
        double latency;
        /** @brief Sample rate in Hz. */
        int rate;
        /** @brief Whether the default device changed and a reset is needed. */
        bool deviceChanged;
        /** @brief Guards the output state. */
        std::recursive_mutex stateMutex;
};
