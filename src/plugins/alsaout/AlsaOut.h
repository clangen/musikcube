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

/** @file AlsaOut.h
 *  @brief ALSA (Advanced Linux Sound Architecture) audio output plugin.
 *  @details Implements the musikcube output SDK on top of the Linux ALSA
 *  PCM interface. PCM samples are written by a dedicated worker thread and
 *  queued buffers are tracked per IBufferProvider.
 */

#include "pch.h"

#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IDevice.h>

#include <list>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>

/** @brief ALSA-based output for Linux.
 *  @details Plays back raw PCM audio through the ALSA library. Uses hardware
 *  parameters negotiated with snd_pcm_hw_params, supports device enumeration
 *  and selection, volume control, pause/resume, stop and drain operations.
 */
class AlsaOut : public musik::core::sdk::IOutput {
    public:
        AlsaOut();
        virtual ~AlsaOut();

        /* IPlugin */
        /** @brief Returns the plugin name. @return Always returns "AlsaOut". */
        const char* Name() override { return "AlsaOut"; }

        /* IOutput */
        /** @brief Destroys the output instance and releases ALSA resources. */
        void Release() override;
        /** @brief Pauses audio playback. */
        void Pause() override ;
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
        /** @brief Returns the output latency in seconds.
         *  @return The measured latency in seconds. */
        double Latency() override;
        /** @brief Blocks until all queued audio has been written to the device. */
        void Drain() override;

        /** @brief Queues a buffer for playback.
         *  @param buffer The PCM buffer to play.
         *  @param provider The provider that owns the buffer.
         *  @return An OutputState indicating whether the output is playing or paused. */
        musik::core::sdk::OutputState Play(
            musik::core::sdk::IBuffer *buffer,
            musik::core::sdk::IBufferProvider *provider) override;

        /** @brief Returns the list of available ALSA devices.
         *  @return A newly allocated IDeviceList the caller must release. */
        musik::core::sdk::IDeviceList* GetDeviceList() override;
        /** @brief Selects the default output device.
         *  @param deviceId Identifier of the device to use.
         *  @return True if the device was selected successfully. */
        bool SetDefaultDevice(const char* deviceId) override;
        /** @brief Returns the default output device.
         *  @return The currently configured default device. */
        musik::core::sdk::IDevice* GetDefaultDevice() override;
        /** @brief Returns the preferred sample rate, or -1 for none.
         *  @return Always returns -1. */
        int GetDefaultSampleRate() override { return -1; }

    private:
        /** @brief Associates a queued PCM buffer with the provider that created it. */
        struct BufferContext {
            /** @brief The audio buffer to be played. */
            musik::core::sdk::IBuffer *buffer;
            /** @brief The provider that supplied the buffer. */
            musik::core::sdk::IBufferProvider *provider;
        };

        /** @brief Counts queued buffers owned by a given provider.
         *  @param provider The provider to search for.
         *  @return The number of matching queued buffers. */
        size_t CountBuffersWithProvider(musik::core::sdk::IBufferProvider* provider);
        /** @brief Derives the ALSA sample format and device parameters from a buffer. */
        void SetFormat(musik::core::sdk::IBuffer *buffer);
        /** @brief Opens and configures the PCM device. */
        void InitDevice();
        /** @brief Closes the PCM device. */
        void CloseDevice();
        /** @brief Worker thread body that writes queued buffers to the device. */
        void WriteLoop();
        /** @brief Resolves the preferred device identifier string. */
        std::string GetPreferredDeviceId();

        /** @brief Name of the configured ALSA PCM device. */
        std::string device;
        /** @brief Handle to the open ALSA PCM device. */
        snd_pcm_t* pcmHandle;
        /** @brief Hardware parameters negotiated with the device. */
        snd_pcm_hw_params_t* hardware;
        /** @brief Current sample format used on the PCM device. */
        snd_pcm_format_t pcmFormat;
        /** @brief Current access mode (e.g. interleaved) used on the device. */
        snd_pcm_access_t pcmType;

        /** @brief Number of interleaved channels. */
        size_t channels;
        /** @brief Sample rate in Hz. */
        size_t rate;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
        /** @brief Measured output latency in seconds. */
        double latency;
        /** @brief Thread-safety flags: quit, paused, initialized. */
        volatile bool quit, paused, initialized;

        /** @brief Background thread performing the PCM writes. */
        std::unique_ptr<std::thread> writeThread;
        /** @brief Guards the state machine. */
        std::recursive_mutex stateMutex;
        /** @brief Signals the write thread when new buffers are available. */
        std::condition_variable_any threadEvent;
        /** @brief Guards the queued buffer list. */
        std::mutex mutex;

        /** @brief Queue of buffers awaiting playback. */
        std::list<std::shared_ptr<BufferContext>> buffers;
};
