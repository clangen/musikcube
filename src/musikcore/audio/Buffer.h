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

/** @file Buffer.h
 *  @brief A fixed-allocation PCM audio buffer implementing the SDK IBuffer interface.
 *  @details The Buffer owns (or optionally wraps) a block of interleaved
 *      floating-point PCM samples together with its format (sample rate and
 *      channel count). It is used throughout the audio pipeline to move decoded
 *      audio between decoders, DSP plugins and output devices. Storage grows on
 *      demand as samples are written. */

#include <musikcore/config.h>
#include <musikcore/sdk/IBuffer.h>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    class Buffer;
    class Stream;

    /** @brief A PCM audio buffer implementing the SDK IBuffer interface.
     *  @details Holds interleaved float samples plus format metadata. The backing
     *      allocation grows on demand unless the ImmutableSize flag is set.
     *      Instances are normally heap-allocated and released via Release(). */
    class Buffer : public musik::core::sdk::IBuffer {
        public:
            /** @brief Flags controlling buffer allocation and ownership behaviour. */
            enum Flags {
                NoFlags = 0,       /**< Default: buffer owns and grows its storage. */
                ImmutableSize = 1, /**< Storage is never resized; Copy() fails if it does not fit. */
                NoDelete = 2       /**< Release() does not delete the buffer (borrowed storage). */
            };

            /** @brief Creates an empty buffer.
             *  @param flags Optional allocation/ownership flags. */
            Buffer(Flags flags = NoFlags) noexcept;
            /** @brief Wraps an externally owned buffer without taking ownership.
             *  @param buffer Pointer to the interleaved PCM samples.
             *  @param samples Number of samples (frames * channels). */
            Buffer(float* buffer, int samples) noexcept;

            virtual ~Buffer();

            /** @return The current sample rate, in Hz. */
            long SampleRate() const noexcept override;
            /** @param sampleRate The new sample rate, in Hz. */
            void SetSampleRate(long sampleRate) noexcept override;
            /** @return The number of interleaved channels. */
            int Channels() const noexcept override;
            /** @param channels The new channel count. */
            void SetChannels(int channels) noexcept override;
            /** @return Pointer to the raw interleaved float sample data. */
            float* BufferPointer() const noexcept override;
            /** @return The number of valid samples (frames * channels). */
            long Samples() const noexcept override;
            /** @param samples The new sample count; grows the backing store if needed. */
            void SetSamples(long samples) override;
            /** @return The size of the backing store, in bytes. */
            long Bytes() const noexcept override;
            /** @brief Frees the buffer. With default flags this deletes the instance. */
            void Release() noexcept override { delete this; }

            /** @return The playback position, in seconds. */
            double Position() const noexcept;
            /** @param position The playback position, in seconds. */
            void SetPosition(double position) noexcept;
            /** @brief Copies samples into the buffer at the given offset.
             *  @param buffer Source interleaved float samples.
             *  @param samples Number of source samples to copy.
             *  @param offset Destination offset, in samples. */
            void Copy(float const* buffer, long samples, long offset = 0);
            /** @brief Copies the sample rate and channel count from another buffer.
             *  @param fromBuffer Buffer whose format should be adopted. */
            void CopyFormat(Buffer* fromBuffer) noexcept;

        private:
            /** @brief Grows the backing store to fit the requested sample count. */
            void ResizeBuffer();

            float *buffer;           /**< Backing store or borrowed sample data. */
            long samples;            /**< Current valid sample count. */
            long internalBufferSize; /**< Allocated capacity, in samples. */
            long sampleRate;         /**< Format: sample rate, in Hz. */
            int channels;            /**< Format: channel count. */
            double position;         /**< Playback position, in seconds. */
            int flags;               /**< Bitwise combination of Flags values. */
    };

} } }
