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

/// @file PortAudioOut.h
/// @brief Audio output plugin built on the cross-platform PortAudio library.
/// @details Implements the output SDK on top of PortAudio's callback-based
/// playback API. PCM buffers are queued and consumed from the audio callback
/// under a lock. Supports device enumeration and selection, volume control,
/// pause/resume, stop and drain operations on every platform PortAudio
/// supports.

#include "config.h"

#include <musikcore/sdk/IOutput.h>
#include <portaudio.h>
#include <deque>
#include <mutex>
#include <memory>
#include <condition_variable>

using namespace musik::core::sdk;

/** @brief Cross-platform output implemented with PortAudio.
 *  @details Creates a PaStream that pulls queued PCM buffers from the callback
 *  thread. The callback is a friend of this class so it can directly consume
 *  the queue. The output adapts its stream format to the first buffer received. */
class PortAudioOut : public IOutput {
    public:
        /** @brief Constructs a stopped output. */
        PortAudioOut();
        /** @brief Destroys the output and stops the PortAudio stream. */
        ~PortAudioOut();

        /* IPlugin */
        /** @brief Returns the plugin name.
         *  @return Always returns "PortAudio". */
        const char* Name() override { return "PortAudio"; };
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
        /** @brief Returns the list of available PortAudio devices.
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
         *  @return The default PortAudio sample rate, or -1. */
        int GetDefaultSampleRate() override;

    /** @brief PortAudio callback that copies queued PCM into the device.
     *  @param input Not used for output streams.
     *  @param output Destination interleaved samples.
     *  @param frameCount Number of frames to write.
     *  @param timeInfo Timestamps for the current buffer.
     *  @param statusFlags Status flags of the stream.
     *  @param userData The PortAudioOut instance.
     *  @return paContinue or paComplete. */
    friend int portAudioStreamCallback(
        const void *input,
        void *output,
        unsigned long frameCount,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData);

    private:
        /** @brief Playback state of the output. */
        enum State {
            /** @brief Playback is stopped. */
            StateStopped,
            /** @brief Playback is paused. */
            StatePaused,
            /** @brief Playback is active. */
            StatePlaying,
            /** @brief Playback is draining. */
            StateDraining,
        };

        /** @brief Tracks the sample format of the current stream. */
        struct FormatContext {
            /** @brief Returns whether a buffer matches the stream format.
             *  @param buffer The buffer to compare.
             *  @return True if sample rate and channels match. */
            bool IsSame(IBuffer* buffer) {
                return
                    this->sampleRate == buffer->SampleRate() &&
                    this->channels == buffer->Channels();
            }
            /** @brief Adopts the format of a buffer.
             *  @param buffer The buffer whose format to use. */
            void From(IBuffer* buffer) {
                this->sampleRate = buffer->SampleRate();
                this->channels = buffer->Channels();
            }
            /** @brief Sample rate in Hz. */
            long sampleRate { 0 };
            /** @brief Number of channels. */
            long channels { 0 };
        };

        /** @brief Tracks the consumption of one queued PCM buffer. */
        struct BufferContext {
            /** @brief Wraps a buffer and its provider.
             *  @param buffer The PCM buffer to play.
             *  @param provider The provider owning the buffer. */
            BufferContext(IBuffer* buffer, IBufferProvider* provider) {
                this->buffer = buffer;
                this->provider = provider;
                this->remainingFrameCount = buffer->Samples() / buffer->Channels();
            }
            /** @brief The PCM buffer being played. */
            IBuffer* buffer { nullptr };
            /** @brief The provider owning the buffer. */
            IBufferProvider* provider { nullptr };
            /** @brief Frames already written to the device. */
            unsigned long framesWritten { 0 };
            /** @brief Frames remaining to write. */
            unsigned long remainingFrameCount { 0 };
            /** @brief Applied gain (volume), or -1 when unset. */
            float gain { -1.0 };
        };

        /** @brief Guards the queue and state. */
        std::recursive_mutex mutex;
        /** @brief Signals the callback thread when buffers are available. */
        std::condition_variable_any bufferEvent;
        /** @brief Open PortAudio stream. */
        PaStream* paStream { nullptr };
        /** @brief Cached device list. */
        IDeviceList* deviceList { nullptr };
        /** @brief Queue of buffers awaiting playback. */
        std::deque<std::shared_ptr<BufferContext>> buffers;
        /** @brief Format of the open stream. */
        FormatContext formatContext;
        /** @brief Current playback state. */
        State state;
        /** @brief Current playback volume, 0.0 to 1.0. */
        double volume;
};
