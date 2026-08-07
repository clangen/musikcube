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

/** @file Streams.h
 *  @brief Factory helpers for decoders, encoders and DSP plugins.
 *  @details Provides lookup functions that map a data stream or type name to the
 *      matching SDK decoder/encoder/DSP implementation registered by plugins. */

#include <musikcore/config.h>
#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/sdk/IDecoder.h>
#include <musikcore/sdk/IEncoder.h>
#include <musikcore/sdk/IDSP.h>
#include <musikcore/sdk/IDecoderFactory.h>

#include <memory>
#include <vector>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @namespace musik::core::audio::streams
     *  @brief Registry lookups for decoders, encoders and DSP plugins. */
    namespace streams {
        /** @return A shared decoder capable of decoding the given data stream, or nullptr.
         *  @param dataStream The data stream to inspect. */
        std::shared_ptr<musik::core::sdk::IDecoder>
            GetDecoderForDataStream(musik::core::io::DataStreamFactory::DataStreamPtr dataStream);

        /** @return A raw decoder capable of decoding the given stream, or nullptr.
         *  @param stream The data stream to inspect. */
        musik::core::sdk::IDecoder*
            GetDecoderForDataStream(musik::core::sdk::IDataStream* stream);

        /** @return An encoder for the given file type, or nullptr.
         *  @param type The MIME/file type name (e.g. "wav", "mp3"). */
        musik::core::sdk::IEncoder* GetEncoderForType(const char* type);

        /** @return All DSP plugin instances registered by plugins. */
        std::vector<std::shared_ptr<musik::core::sdk::IDSP > > GetDspPlugins();
    };

} } }
