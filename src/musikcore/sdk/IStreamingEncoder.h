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

/** @file IStreamingEncoder.h @brief Defines the IStreamingEncoder interface for encoders that produce encoded output in chunks. */
#pragma once

#include "IEncoder.h"
#include "IBuffer.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief An encoder that produces encoded output in individually owned
     *  chunks, allowing the caller to control when and where data is written. */
    class IStreamingEncoder: public IEncoder {
        public:
            /** @brief Prepares the encoder for use.
             *  @param rate The input sample rate in Hz.
             *  @param channels The number of input channels.
             *  @param bitrate The target output bitrate in bits per second.
             *  @return True if initialization succeeded. */
            virtual bool Initialize(size_t rate, size_t channels, size_t bitrate) = 0;

            /** @brief Encodes a buffer of PCM data.
             *  @param pcm The PCM data to encode.
             *  @param data On return, points to the newly allocated encoded data, or null if none was produced.
             *  @return The number of bytes written, or 0 if nothing was produced. */
            virtual int Encode(const IBuffer* pcm, char** data) = 0;

            /** @brief Flushes any remaining encoded data.
             *  @param data On return, points to the newly allocated trailing data, or null if none was produced.
             *  @return The number of bytes written, or 0 if nothing was produced. */
            virtual int Flush(char** data) = 0;

            /** @brief Completes encoding and finalizes the output resource.
             *  @param uri The destination URI the encoded data should be associated with. */
            virtual void Finalize(const char* uri) = 0;
     };

} } }
