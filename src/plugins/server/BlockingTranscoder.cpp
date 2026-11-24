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

#include "BlockingTranscoder.h"

#include <atomic>
#include <filesystem>

namespace {
    std::atomic<int> activeCount { 0 };

    struct ActiveTranscode {
        ActiveTranscode() { ++activeCount; }
        ~ActiveTranscode() { --activeCount; }
    };
}

bool BlockingTranscoder::Run(Context& context, mc_encoder& encoder, const std::string& uri,
    const std::string& temp, const std::string& destination, size_t bitrate) {
    ActiveTranscode active;
    server::DecodedAudio input;
    if (!input.Open(*context.media, *context.scratch, uri) || !input.HasDuration()) {
        return false;
    }

    server::MediaStream output;
    if (!output.Open(*context.media, temp,
        MC_STREAM_OPEN_WRITE | MC_STREAM_OPEN_CREATE | MC_STREAM_OPEN_TRUNCATE))
    {
        return false;
    }

    mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
    bool success = input.Read(pcm) == MC_OK && pcm.frame_count &&
        encoder.api->initialize(encoder.instance, &output.Handle(), pcm.format.sample_rate, pcm.format.channels, bitrate) == MC_OK;
    while (success && pcm.frame_count) {
        mc_encoded_block block = MC_ENCODED_BLOCK_DEFAULT;
        success = encoder.api->encode(encoder.instance, &pcm, &block) == MC_OK && input.Read(pcm) == MC_OK;
    }
    success = success && encoder.api->finalize(encoder.instance, { nullptr, 0 }) == MC_OK;
    success = output.Close() && success;

    std::error_code error;
    if (success) {
        std::filesystem::rename(std::filesystem::u8path(temp), std::filesystem::u8path(destination), error);
        if (!error) {
            return true;
        }
    }
    std::filesystem::remove(std::filesystem::u8path(temp), error);
    return false;
}

int BlockingTranscoder::GetActiveCount() {
    return activeCount.load();
}
