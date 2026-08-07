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

/** @file IBlockingEncoder.h @brief Defines the IBlockingEncoder interface for encoders that process entire PCM buffers synchronously. */
#pragma once

#include "IEncoder.h"
#include "IBuffer.h"
#include "IDataStream.h"
#include <stddef.h>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief An encoder that synchronously writes encoded output to a data
     *  stream as PCM buffers are fed to it. */
    class IBlockingEncoder: public IEncoder {
        public:
            /** @brief Prepares the encoder for writing.
             *  @param out The data stream that encoded bytes are written to.
             *  @param rate The input sample rate in Hz.
             *  @param channels The number of input channels.
             *  @param bitrate The target output bitrate in bits per second.
             *  @return True if initialization succeeded. */
            virtual bool Initialize(IDataStream* out, size_t rate, size_t channels, size_t bitrate) = 0;

            /** @brief Encodes and writes a single buffer of PCM data.
             *  @param pcm The PCM data to encode.
             *  @return True if the buffer was successfully encoded. */
            virtual bool Encode(const IBuffer* pcm) = 0;

            /** @brief Flushes any remaining encoded data and finalizes the output stream. */
            virtual void Finalize() = 0;
     };

} } }
