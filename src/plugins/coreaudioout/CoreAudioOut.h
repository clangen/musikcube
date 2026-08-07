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

/** @file CoreAudioOut.h
 *  @brief CoreAudio (macOS) audio output plugin.
 *  @details Implements the musikcube output SDK on top of the macOS CoreAudio
 *  AudioQueue API. Buffers are enqueued with AudioQueueEnqueueBuffer and
 *  recycled asynchronously as playback progresses.
 */

#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IBufferProvider.h>
#include <deque>
#include <mutex>
#include <AudioToolbox/AudioQueue.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>

/** @brief CoreAudio-based output for macOS.
 *  @details Plays back PCM audio using an AudioQueue. Maintains a small pool
 *  of queued buffers, supports volume control, pause/resume, stop and drain,
 *  and reports device lists through the CoreAudio HAL.
 */
class CoreAudioOut : public musik::core::sdk::IOutput {
    public:
        /** @brief Associates an enqueued PCM buffer with its provider. */
        struct BufferContext {
            /** @brief The audio buffer enqueued for playback. */
            musik::core::sdk::IBuffer *buffer;
            /** @brief The provider that supplied the buffer. */
            musik::core::sdk::IBufferProvider *provider;
        };

        CoreAudioOut();
        virtual ~CoreAudioOut();

        /* IPlugin */
        /** @brief Returns the plugin name. @return Always returns "CoreAudio". */
        const char* Name() override { return "CoreAudio"; }

        /* IOutput */
        /** @brief Destroys the output instance and tears down the AudioQueue. */
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
        /** @brief Stops playback and discards queued buffers. */
        void Stop() override;
        /** @brief Returns the output latency in seconds.
         *  @return Always returns 0.0. */
        double Latency() override { return 0.0; }
        /** @brief Blocks until all queued audio has been consumed. */
        void Drain() override;

        /** @brief Enqueues a buffer for playback.
         *  @param buffer The PCM buffer to play.
         *  @param provider The provider that owns the buffer.
         *  @return An OutputState indicating whether the output is playing or paused. */
        musik::core::sdk::OutputState Play(
            musik::core::sdk::IBuffer *buffer,
            musik::core::sdk::IBufferProvider *provider) override;

        /** @brief Returns the list of available CoreAudio devices.
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

        /** @brief Called by the AudioQueue when a buffer has finished playing.
         *  @param context The buffer context that has completed. */
        void NotifyBufferCompleted(BufferContext *context);

    private:
        /** @brief Internal playback state. */
        enum State {
            /** @brief Output is stopped. */
            StateStopped,
            /** @brief Output is paused. */
            StatePaused,
            /** @brief Output is actively playing. */
            StatePlaying
        };

        /** @brief Stream description of the PCM format being played. */
        AudioStreamBasicDescription audioFormat;
        /** @brief Handle to the underlying AudioQueue. */
        AudioQueueRef audioQueue;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
        /** @brief Number of buffers currently queued. */
        int bufferCount;
        /** @brief Guards state changes. */
        std::recursive_mutex mutex;
        /** @brief Flag indicating the output should shut down. */
        bool quit;
        /** @brief Current playback state. */
        State state;
};
