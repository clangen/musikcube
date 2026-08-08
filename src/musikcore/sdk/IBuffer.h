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

/** @file IBuffer.h @brief Defines the IBuffer interface, a chunk of interleaved float PCM audio data. */
#pragma once

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief An owned chunk of interleaved float PCM audio data, filled by
     *  decoders and consumed by DSP effects and output plugins. */
    class IBuffer {
        public:
            /** @brief Returns the sample rate of the audio data, in Hz.
             *  @return The sample rate in Hz. */
            virtual long SampleRate() const = 0;

            /** @brief Sets the sample rate of the audio data, in Hz.
             *  @param sampleRate The new sample rate in Hz. */
            virtual void SetSampleRate(long sampleRate) = 0;

            /** @brief Returns the number of interleaved audio channels.
             *  @return The channel count. */
            virtual int Channels() const = 0;

            /** @brief Sets the number of interleaved audio channels.
             *  @param channels The new channel count. */
            virtual void SetChannels(int channels) = 0;

            /** @brief Returns a pointer to the raw interleaved float samples.
             *  @return A pointer to the sample data, or null if the buffer is empty. */
            virtual float* BufferPointer() const = 0;

            /** @brief Returns the number of samples per channel.
             *  @return The per-channel sample count. */
            virtual long Samples() const = 0;

            /** @brief Sets the number of samples contained in the buffer.
             *  @param samples The new sample count. */
            virtual void SetSamples(long samples) = 0;

            /** @brief Returns the total capacity of the underlying storage, in bytes.
             *  @return The buffer size in bytes. */
            virtual long Bytes() const = 0;

            /** @brief Releases the buffer; callers must invoke this when done with it. */
            virtual void Release() = 0;
    };

} } }
