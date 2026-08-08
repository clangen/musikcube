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

/// @file SndioOut.h
/// @brief Audio output plugin built on the sndio sound server (OpenBSD).
/// @details Implements the output SDK on top of sndio's synchronous playback
/// API. PCM buffers are queued and written by a dedicated worker thread that
/// also processes control commands (pause, resume, stop, volume, drain).
/// OpenBSD-only.

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IOutput.h>
#include <sndio.h>
#include <mutex>
#include <list>
#include <thread>
#include <condition_variable>

using namespace musik::core::sdk;

/** @brief sndio-based output for OpenBSD.
 *  @details Opens a sndio output handle, negotiates the sample format and
 *  writes queued PCM buffers from a worker thread. Commands are delivered to
 *  the same thread through a command list protected by a mutex. */
class SndioOut : public IOutput {
    public:
        /** @brief Constructs a stopped output. */
        SndioOut();
        /** @brief Destroys the output and closes the sndio handle. */
        ~SndioOut();

        /* IPlugin */
        /** @brief Returns the plugin name.
         *  @return Always returns "sndio". */
        const char* Name() override { return "sndio"; };
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
        musik::core::sdk::OutputState Play(IBuffer *buffer, IBufferProvider *provider) override;
        /** @brief Returns the output latency in seconds.
         *  @return The measured latency in seconds. */
        double Latency() override;
        /** @brief Blocks until all queued audio has been written to the device. */
        void Drain() override;
        /** @brief Returns the list of available sndio devices.
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

    private:
        /** @brief Commands processed by the worker thread. */
        enum class Command: int {
            /** @brief Pause playback. */
            Pause, /** @brief Resume playback. */
            Resume, /** @brief Stop playback. */
            Stop, /** @brief Set the volume. */
            SetVolume, /** @brief Drain pending audio. */
            Drain, /** @brief Shut down the worker thread. */
            Quit
        };

        /** @brief Associates a queued buffer with its provider. */
        struct BufferContext {
            /** @brief The provider owning the buffer. */
            IBufferProvider* provider;
            /** @brief The audio buffer to play. */
            IBuffer* buffer;
        };

        /** @brief Playback state of the output. */
        enum State {
            /** @brief Playback is stopped. */
            StateStopped, /** @brief Playback is paused. */
            StatePaused, /** @brief Playback is active. */
            StatePlaying
        };

        /** @brief Counts queued buffers owned by a given provider.
         *  @param provider The provider to search for.
         *  @return The number of matching queued buffers. */
        size_t CountBuffersWithProvider(IBufferProvider* provider);
        /** @brief Worker thread body that writes buffers and handles commands. */
        void WriteLoop();
        /** @brief Queues a command for the worker thread.
         *  @param command The command to execute. */
        void PushCommand(Command command);
        /** @brief Releases all queued buffers back to their providers. */
        void DiscardBuffers();

        /* audio */
        /** @brief Current playback state. */
        State state;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
        /** @brief Measured output latency in seconds. */
        double latency;

        /* threading */
        /** @brief Pending commands for the worker thread. */
        std::list<Command> commands;
        /** @brief Queue of buffers awaiting playback. */
        std::list<BufferContext> buffers;
        /** @brief Background thread performing the writes. */
        std::unique_ptr<std::thread> writeThread;
        /** @brief Signals the worker thread when commands/buffers arrive. */
        std::condition_variable threadEvent;
        /** @brief Guards the command and buffer lists. */
        std::mutex mutex;
};
