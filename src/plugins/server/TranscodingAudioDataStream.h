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

#include "ServerMedia.h"
#include <musikcore/sdk/IStreamingEncoder.h>
#include <fstream>

/* produces compressed bytes on the HTTP worker as the client reads. the
encoder owns each output block until the next encode or flush call. */
class TranscodingAudioDataStream final : public server::ResponseBody {
public:
    TranscodingAudioDataStream(server::EncoderOwner<musik::core::sdk::IStreamingEncoder> encoder, size_t bitrate);
    ~TranscodingAudioDataStream() override;

    bool Open(Context& context, const std::string& uri, bool requireDuration, const std::string& temporaryFile,
        const std::string& cacheFile);

    std::unique_ptr<server::ResponseBody> Finish(Context& context);

    mc_result Read(uint64_t offset, char* dst, size_t capacity, size_t& count) override;
    std::optional<uint64_t> Length() const override { return {}; }
    bool Seekable() const override { return false; }
    bool IsCaching() const { return cache.is_open(); }

    static int GetActiveCount();

private:
    mc_result EncodeNextBlock();
    bool PublishCache();

    server::DecodedAudio source;
    server::EncoderOwner<musik::core::sdk::IStreamingEncoder> encoder;
    size_t bitrate;
    bool initialized = false;
    bool flushed = false;
    char* pending = nullptr;
    size_t pendingBytes = 0;
    uint64_t position = 0;
    std::ofstream cache;
    std::string temporaryFile;
    std::string cacheFile;
};
