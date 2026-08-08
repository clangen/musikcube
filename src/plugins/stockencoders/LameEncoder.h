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

///

/// @file LameEncoder.h
/// @brief Real-time MP3 streaming encoder built on the LAME library.
/// @details Converts interleaved PCM (float) into MP3 frames using libmp3lame.
/// Multi-channel input is downmixed to stereo/float as needed. Encoded frames
/// are returned to the caller for on-demand streaming transcodes.

#include <musikcore/sdk/IStreamingEncoder.h>
#include <musikcore/sdk/DataBuffer.h>

#pragma warning(push, 0)
#include <lame/lame.h>
#pragma warning(pop)

/** @brief Streaming MP3 encoder backed by LAME.
 *  @details Implements IStreamingEncoder so audio can be encoded on demand in
 *  real time. Encode() returns newly produced MP3 bytes, Flush() drains the
 *  encoder, and Finalize() completes the stream for the given URI. */
class LameEncoder: public musik::core::sdk::IStreamingEncoder {
    /** @brief Buffer type alias. */
    using IBuffer = musik::core::sdk::IBuffer;

    public:
        /** @brief Constructs an uninitialized encoder. */
        LameEncoder();

        /** @brief Destroys the encoder and frees the LAME handle. */
        virtual void Release() override;
        /** @brief Initializes the LAME encoder.
         *  @param rate Sample rate in Hz.
         *  @param channels Input channel count.
         *  @param bitrate Target bitrate in bits per second.
         *  @return True if the encoder initialized successfully. */
        virtual bool Initialize(size_t rate, size_t channels, size_t bitrate) override;
        /** @brief Encodes a block of PCM.
         *  @param pcm The interleaved float PCM buffer.
         *  @param data Receives a pointer to the encoded bytes.
         *  @return The number of encoded bytes, or a negative error code. */
        virtual int Encode(const IBuffer* pcm, char** data) override;
        /** @brief Flushes the encoder of remaining samples.
         *  @param data Receives a pointer to the encoded bytes.
         *  @return The number of encoded bytes, or a negative error code. */
        virtual int Flush(char** data) override;
        /** @brief Completes the encoding for the given URI.
         *  @param uri The URI of the output track. */
        virtual void Finalize(const char* uri) override;

    private:
        /** @brief Buffer holding encoded MP3 bytes. */
        DataBuffer<unsigned char> encodedBytes;
        /** @brief Buffer for stereo/float downmix of the input. */
        DataBuffer<float> downmix;
        /** @brief LAME encoder handle. */
        lame_t lame;
};