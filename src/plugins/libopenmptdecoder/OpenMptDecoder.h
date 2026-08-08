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

/// @file OpenMptDecoder.h
/// @brief Decoder for tracked music modules built on libopenmpt.
/// @details Supports a wide range of tracker formats such as MOD, S3M, XM, IT,
/// plus newer formats handled by libopenmpt. Module sub-tracks are addressed
/// through the OpenMptDataStream, and playback renders 16-bit PCM at the
/// module's native sample rate.

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IDataStream.h>

#pragma warning(push, 0)
#include <libopenmpt/libopenmpt.h>
#pragma warning(pop)

/** @brief Decodes tracked-music modules using the libopenmpt library.
 *  @details Owns an openmpt_module handle created from the data stream. The
 *  stream wrapper determines whether this decoder must also own (and later
 *  release) the underlying IDataStream. */
class OpenMptDecoder: public musik::core::sdk::IDecoder {
    public:
        /** @brief Constructs a decoder with no loaded module. */
        OpenMptDecoder();
        /** @brief Destroys the decoder and frees the module handle. */
        ~OpenMptDecoder();

        /** @brief Destroys the decoder instance. */
        void Release() override;
        /** @brief Seeks to a position in the current module.
         *  @param seconds Target position in seconds.
         *  @return The actual position reached, in seconds. */
        double SetPosition(double seconds) override;
        /** @brief Fills a buffer with the next block of decoded PCM.
         *  @param buffer The buffer to fill.
         *  @return True if data was written, false at end of track. */
        bool GetBuffer(musik::core::sdk::IBuffer* buffer) override;
        /** @brief Returns the total duration of the current module.
         *  @return Duration in seconds. */
        double GetDuration() override;
        /** @brief Opens the decoder against an OpenMpt data stream.
         *  @param stream The OpenMptDataStream to decode.
         *  @return True if the module opened successfully. */
        bool Open(musik::core::sdk::IDataStream* stream) override;
        /** @brief Returns whether the module has been fully consumed.
         *  @return True when exhausted. */
        bool Exhausted() override;
        /** @brief libopenmpt renders at its native rate.
         *  @param rate Requested rate (ignored). */
        void SetPreferredSampleRate(int rate) override { }

        /** @brief Returns the data stream being decoded.
         *  @return The wrapped stream, or null. */
        musik::core::sdk::IDataStream* Stream() { return this->stream; }

    private:
        /** @brief libopenmpt module playback handle. */
        openmpt_module* module;
        /** @brief The data stream being decoded. */
        musik::core::sdk::IDataStream* stream{ nullptr };
        /** @brief Whether the decoder owns the wrapped stream. */
        bool isWrappedStream{ false };
};
