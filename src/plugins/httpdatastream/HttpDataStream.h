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

#include "LruDiskCache.h"
#include <musikcore/plugin/services.h>
#include <musikcore/plugin/settings.h>
#include <musikcore/plugin/stream.h>
#include <curl/curl.h>
#include <atomic>
#include <condition_variable>
#include <optional>
#include <string_view>
#include <thread>

/* downloads into a private cache file while the decoder reads its published
prefix. seek can wait for future bytes; Stop wakes both open and the reader. */
class HttpDataStream {
public:
    static constexpr std::string_view RemoteTrack = "musikcore://remote-track/";

    static mc_result Create(LruDiskCache& cache, const mc_plugin_services& services, void** instance);
    static const mc_configuration_api& Configuration();

    ~HttpDataStream();

    mc_result Open(mc_string_view uri, mc_stream_open_flags flags);
    mc_result Close();
    void Interrupt();
    mc_result Info(mc_stream_info& info);
    mc_result Read(void* dst, size_t capacity, size_t& bytes, mc_read_state& state);
    mc_result Seek(uint64_t offset);
    mc_result Tell(uint64_t& offset);

private:
    struct Options {
        std::filesystem::path directory;
        int maxFiles;
        int precache;
        int chunk;
        int connectionTimeout;
        int readTimeout;
    };

    HttpDataStream(LruDiskCache& cache, Options options);

    void Download() noexcept;
    mc_result Transfer();
    size_t WriteBytes(const char* data, size_t size);
    void ReadHeader(std::string_view header);
    static size_t CurlWrite(char* data, size_t size, size_t count, void* context);
    static size_t CurlHeader(char* data, size_t size, size_t count, void* context);
    static int CurlProgress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t);

    LruDiskCache& cache;
    Options options;
    std::filesystem::path partial;
    size_t cacheId {};
    bool opened = false;
    std::string url, originalUri, authorization, contentType, extension;
    bool allowUntrusted = false;
    bool explicitContentType = false;
    FILE* reader = nullptr;
    FILE* writer = nullptr;
    std::thread worker;
    std::atomic<bool> cancelled { false };
    std::mutex mutex;
    std::condition_variable changed;
    uint64_t received = 0, available = 0, position = 0;
    std::optional<uint64_t> length;
    std::optional<mc_result> finished;
};
