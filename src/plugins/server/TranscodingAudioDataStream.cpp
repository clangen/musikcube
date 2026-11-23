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

#include "TranscodingAudioDataStream.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>

namespace {
    std::atomic<int> activeCount { 0 };
}

TranscodingAudioDataStream::TranscodingAudioDataStream(
    server::EncoderOwner<musik::core::sdk::IStreamingEncoder> encoder, size_t bitrate)
: encoder(std::move(encoder))
, bitrate(bitrate) {
    ++activeCount;
}

TranscodingAudioDataStream::~TranscodingAudioDataStream() {
    /* a disconnected client may leave a partial encode. a later request must
    never mistake that partial file for a complete cached response. */
    if (!temporaryFile.empty()) {
        cache.close();
        std::error_code error;
        std::filesystem::remove(std::filesystem::u8path(temporaryFile), error);
    }
    --activeCount;
}

bool TranscodingAudioDataStream::Open(Context& context, const std::string& uri, bool requireDuration,
    const std::string& temporaryFile, const std::string& cacheFile) {
    if (!source.Open(context, uri)) {
        return false;
    }
    const bool finite = source.HasDuration();
    if (requireDuration && !finite) {
        return false;
    }
    if (finite && !temporaryFile.empty()) {
        this->temporaryFile = temporaryFile;
        this->cacheFile = cacheFile;
        cache.open(std::filesystem::u8path(temporaryFile), std::ios::binary);
        return cache.is_open();
    }
    return true;
}

mc_result TranscodingAudioDataStream::EncodeNextBlock() {
    mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
    const auto status = source.Read(pcm);
    if (status != MC_OK) {
        return status;
    }

    if (!initialized) {
        if (!pcm.frame_count || !encoder->Initialize(pcm.format.sample_rate, pcm.format.channels, bitrate)) {
            return MC_ERROR_PLUGIN;
        }
        initialized = true;
    }
    int encoded;
    if (pcm.frame_count) {
        encoded = encoder->Encode(pcm, &pending);
    }
    else {
        encoded = encoder->Flush(&pending);
        flushed = true;
    }
    if (encoded < 0 || (encoded && !pending)) {
        return MC_ERROR_PLUGIN;
    }
    pendingBytes = static_cast<size_t>(encoded);
    return MC_OK;
}

bool TranscodingAudioDataStream::PublishCache() {
    if (!cache.is_open()) {
        return true;
    }
    cache.close();
    if (cache.fail()) {
        return false;
    }
    if (!encoder->Finalize(temporaryFile.c_str())) {
        return false;
    }
    if (cacheFile.empty()) {
        /* a wait-mode request with caching disabled owns this file until
        Finish hands it to the HTTP response. it never enters the cache. */
        return true;
    }
    std::error_code error;
    std::filesystem::rename(std::filesystem::u8path(temporaryFile), std::filesystem::u8path(cacheFile), error);
    if (error) {
        /* another request can publish the same encoding first. on systems
        that cannot replace an open file, keep that complete cache entry. */
        if (!std::filesystem::is_regular_file(std::filesystem::u8path(cacheFile), error)) {
            return false;
        }
        std::filesystem::remove(std::filesystem::u8path(temporaryFile), error);
        if (error) {
            return false;
        }
    }
    temporaryFile.clear();
    return true;
}

mc_result TranscodingAudioDataStream::Read(uint64_t offset, char* dst, size_t capacity, size_t& count) {
    count = 0;
    if (offset != position) {
        return MC_ERROR_UNSUPPORTED;
    }
    while (count < capacity) {
        if (!pendingBytes) {
            if (flushed) {
                break;
            }
            const auto status = EncodeNextBlock();
            if (status != MC_OK) {
                return status;
            }
            if (!pendingBytes) {
                continue;
            }
        }
        /* the flush block can exceed the remaining caller capacity too. keep
        its unread suffix under the same ownership rules as any encoded block. */
        const auto bytes = std::min(capacity - count, pendingBytes);
        std::memcpy(dst + count, pending, bytes);
        if (cache.is_open() && !cache.write(pending, static_cast<std::streamsize>(bytes))) {
            return MC_ERROR_IO;
        }
        count += bytes;
        pending += bytes;
        pendingBytes -= bytes;
    }
    position += count;
    if (flushed && !pendingBytes && !PublishCache()) {
        return MC_ERROR_IO;
    }
    return MC_OK;
}

std::unique_ptr<server::ResponseBody> TranscodingAudioDataStream::Finish(Context& context) {
    if (!cache.is_open()) {
        return {};
    }
    char block[8192];
    size_t bytes;
    do {
        if (Read(position, block, sizeof(block), bytes) != MC_OK) {
            return {};
        }
    } while (bytes);
    if (!cacheFile.empty()) {
        return server::OpenFileBody(context, cacheFile);
    }
    return server::TakeTemporaryFileBody(context, temporaryFile);
}

int TranscodingAudioDataStream::GetActiveCount() {
    return activeCount.load();
}
