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

/* stream offsets are 64-bit on 32-bit Linux too. select the matching libc
interfaces before any system header declares stat or seek. */
#if defined(__linux__) && !defined(_FILE_OFFSET_BITS)
#define _FILE_OFFSET_BITS 64
#endif

#include "config.h"
#include "HttpDataStream.h"
#include <common/String.h>
#include <musikcore/plugin.h>
#include <../../3rdparty/include/nlohmann/json.hpp>
#include <../../3rdparty/include/websocketpp/base64/base64.hpp>

#include <algorithm>
#include <charconv>
#include <cstring>
#include <memory>

namespace {
    constexpr mc_integer_setting setting(std::string_view key, int value, int minimum) {
        return {
            .key = {
                .data = key.data(),
                .length = key.size(),
            },
            .default_value = value,
            .minimum = minimum,
            .maximum = INT32_MAX,
        };
    }

    constexpr auto MaxFiles = setting("max_cache_files", 35, INT32_MIN);
    constexpr auto Precache = setting("precache_buffer_size_bytes", 524288, 32768);
    constexpr auto Chunk = setting("chunk_size_bytes", 131072, 32768);
    constexpr auto ConnectionTimeout = setting("connection_timeout_seconds", 15, 1);
    constexpr auto ReadTimeout = setting("read_timeout_seconds", 30, 1);

    constexpr mc_integer_setting Settings[] = { MaxFiles, Precache, Chunk, ConnectionTimeout, ReadTimeout };

    mc_string_view view(const std::string& value) {
        return {
            .data = value.data(),
            .length = value.size(),
        };
    }

    char lower(char c) {
        return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }

    bool seekFile(FILE* file, uint64_t offset) {
        if (offset > INT64_MAX) {
            return false;
        }
#ifdef _WIN32
        return _fseeki64(file, static_cast<int64_t>(offset), SEEK_SET) == 0;
#else
        return fseeko(file, static_cast<off_t>(offset), SEEK_SET) == 0;
#endif
    }
}

const mc_configuration_api& HttpDataStream::Configuration() {
    static constexpr mc_configuration_api schema {
        .struct_size = sizeof(mc_configuration_api),
        .integers = Settings,
        .integer_count = std::size(Settings),
    };
    return schema;
}

mc_result HttpDataStream::Create(
    LruDiskCache& cache, const mc_host_api& host, const mc_plugin_services& services, void** instance)
{
    if (!services.preferences || !services.data_directory.length) {
        return MC_ERROR_UNSUPPORTED;
    }
    Options options {};
    mc_result status = MC_OK;
    auto load = [&](const mc_integer_setting& field, int& value) {
        if (status == MC_OK) {
            const auto prefs = services.preferences;
            status = prefs->read_integer(prefs->context, &field, &value);
        }
    };
    load(MaxFiles, options.maxFiles);
    load(Precache, options.precache);
    load(Chunk, options.chunk);
    load(ConnectionTimeout, options.connectionTimeout);
    load(ReadTimeout, options.readTimeout);
    if (status != MC_OK) {
        return status;
    }

    const auto directory = services.data_directory;
    options.directory =
        std::filesystem::u8path(directory.data, directory.data + directory.length) / "cache" / "httpclient";
    *instance = new HttpDataStream(cache, host, std::move(options));
    return MC_OK;
}

HttpDataStream::HttpDataStream(LruDiskCache& cache, const mc_host_api& host, Options options)
: cache(cache)
, host(host)
, options(std::move(options)) {
}

HttpDataStream::~HttpDataStream() {
    Close();
}

mc_result HttpDataStream::Open(mc_string_view uri, mc_stream_open_flags flags) {
    if (flags != MC_STREAM_OPEN_READ) {
        return MC_ERROR_UNSUPPORTED;
    }
    if (cancelled) {
        return MC_ERROR_CANCELLED;
    }
    if (opened || !uri.data || !uri.length || std::memchr(uri.data, 0, uri.length)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    opened = true;
    url.assign(uri.data, uri.length);
    originalUri = url;
    if (url.compare(0, RemoteTrack.size(), RemoteTrack) == 0) {
        try {
            const auto payload = nlohmann::json::parse(url.substr(RemoteTrack.size()));
            url = payload.at("uri").get<std::string>();
            originalUri = payload.at("originalUri").get<std::string>();
            const auto type = payload.value("type", ".mp3");
            if (!type.empty() && type[0] == '.') {
                extension = type.substr(1);
            }
            else {
                contentType = type;
                explicitContentType = !type.empty();
            }
            allowUntrusted = url.compare(0, 8, "https://") == 0 && payload.value("allowUntrustedCertificate", false);
            authorization =
                "Authorization: Basic " + websocketpp::base64_encode("default:" + payload.value("password", ""));
        }
        catch (const nlohmann::json::exception&) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
    }
    if (extension.empty()) {
        const auto resource = originalUri.substr(0, originalUri.find_first_of("?#"));
        const auto dot = resource.rfind('.');
        const auto slash = resource.rfind('/');
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
            extension = resource.substr(dot + 1);
        }
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), lower);

    /* keep the existing verified and unverified cache namespaces separate. */
    cacheId = std::hash<std::string>()(allowUntrusted ? "untrusted:" + url : url);
    std::string storedType;
    uint64_t cachedBytes = 0;
    if (!cache.Initialize(options.directory, static_cast<size_t>(std::max(options.maxFiles, 0)), cancelled)) {
        return MC_ERROR_CANCELLED;
    }
    reader = cache.OpenCached(cacheId, storedType, cachedBytes);
    if (reader) {
        if (contentType.empty() && storedType.find('/') != std::string::npos) {
            contentType = storedType;
        }
        else if (extension.empty() && !storedType.empty() && storedType.front() == '.') {
            extension = storedType.substr(1);
        }
        length = available = cachedBytes;
        finished = MC_OK;
        return cancelled ? MC_ERROR_CANCELLED : MC_OK;
    }

    if (!cache.CreatePartial(partial, writer, reader)) {
        return MC_ERROR_IO;
    }
    worker = std::thread(&HttpDataStream::Download, this);
    std::unique_lock lock(mutex);
    changed.wait(lock, [&] {
        return cancelled || finished || available >= static_cast<size_t>(options.precache);
    });
    return cancelled ? MC_ERROR_CANCELLED : finished.value_or(MC_OK);
}

void HttpDataStream::Interrupt() {
    {
        std::lock_guard lock(mutex);
        cancelled = true;
    }
    changed.notify_all();
}

mc_result HttpDataStream::Close() {
    Interrupt();
    if (worker.joinable()) {
        worker.join();
    }
    bool closed = true;
    if (reader) {
        closed = fclose(reader) == 0;
        reader = nullptr;
    }
    if (writer) {
        closed = fclose(writer) == 0 && closed;
        writer = nullptr;
    }
    if (!partial.empty()) {
        /* only a complete, flushed response may replace the cached track.
        cleanup after a failed cache rename leaves playback itself usable. */
        try {
            const auto type = contentType.empty() && !extension.empty() ? "." + extension : contentType;
            if (closed && finished == MC_OK) {
                cache.Finish(cacheId, partial, type);
            }
        }
        catch (const std::exception&) {
            closed = false;
        }
        std::error_code ignored;
        std::filesystem::remove(partial, ignored);
        partial.clear();
    }
    return closed ? MC_OK : MC_ERROR_IO;
}

mc_result HttpDataStream::Info(mc_stream_info& info) {
    std::lock_guard lock(mutex);
    if (cancelled || !reader) {
        return cancelled ? MC_ERROR_CANCELLED : MC_ERROR_INVALID_ARGUMENT;
    }
    info = MC_STREAM_INFO_DEFAULT;
    info.flags = MC_STREAM_READABLE | MC_STREAM_SEEKABLE | MC_STREAM_CAN_PREFETCH;
    if (length) {
        info.flags |= MC_STREAM_LENGTH_KNOWN;
        info.length_bytes = *length;
    }
    info.uri = view(originalUri);
    info.content_type = view(contentType);
    info.extension = view(extension);
    return MC_OK;
}

mc_result HttpDataStream::Read(void* dst, size_t capacity, size_t& bytes, mc_read_state& state) {
    bytes = 0;
    state = MC_READ_MORE;
    std::unique_lock lock(mutex);
    if (!reader) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    /* reaching the writer's current position is ordinary underflow. wait for
    the next flushed chunk, transfer completion, or Stop before answering. */
    if (capacity) {
        changed.wait(lock, [&] {
            return cancelled || finished || position < available;
        });
    }
    if (cancelled) {
        return MC_ERROR_CANCELLED;
    }
    if (position < available && capacity) {
        const auto n = static_cast<size_t>(std::min<uint64_t>(available - position, capacity));
        clearerr(reader);
        bytes = fread(dst, 1, n, reader);
        position += bytes;
        if (bytes != n) {
            return MC_ERROR_IO;
        }
    }
    if (finished && position >= available) {
        if (*finished == MC_OK) {
            state = MC_READ_END;
        }
        return *finished;
    }
    return MC_OK;
}

mc_result HttpDataStream::Seek(uint64_t offset) {
    std::unique_lock lock(mutex);
    if (cancelled) {
        return MC_ERROR_CANCELLED;
    }
    if (!reader || offset > INT64_MAX || (length && offset > *length)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    /* a demuxer can probe the alleged end in the file header before playback.
    a live response has no promised tail. it can seek within the published
    prefix and read forward as data arrives. waiting here could stall open forever. */
    if (!length && offset > available) {
        return MC_ERROR_UNSUPPORTED;
    }
    changed.wait(lock, [&] {
        return cancelled || finished || offset <= available;
    });
    if (cancelled) {
        return MC_ERROR_CANCELLED;
    }
    if (offset > available || !seekFile(reader, offset)) {
        return MC_ERROR_IO;
    }
    position = offset;
    return MC_OK;
}

mc_result HttpDataStream::Tell(uint64_t& offset) {
    std::lock_guard lock(mutex);
    offset = position;
    return cancelled ? MC_ERROR_CANCELLED : MC_OK;
}

void HttpDataStream::Download() noexcept {
    mc_result result = MC_ERROR_IO;
    try {
        result = Transfer();
    }
    catch (...) {
        /* the waiting decoder still needs a terminal status if a download
        cannot allocate its request or open its cache file. */
    }
    {
        std::lock_guard lock(mutex);
        if (fflush(writer) != 0) {
            result = MC_ERROR_IO;
        }
        available = received;
        if (result == MC_OK) {
            length = received;
        }
        finished = result;
    }
    changed.notify_all();
}

mc_result HttpDataStream::Transfer() {
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) {
        return MC_ERROR_IO;
    }
    auto* handle = curl.get();
    char curlError[CURL_ERROR_SIZE] {};
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> headers(
        authorization.empty() ? nullptr : curl_slist_append(nullptr, authorization.c_str()), curl_slist_free_all);
    if (!authorization.empty() && !headers) {
        return MC_ERROR_IO;
    }
    curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(handle, CURLOPT_ERRORBUFFER, curlError);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers.get());
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, allowUntrusted ? 0L : 1L);
    if (allowUntrusted) {
        curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    }
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
    curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(handle, CURLOPT_USERAGENT, "musikcube HttpDataStream");
    curl_easy_setopt(handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, static_cast<long>(options.connectionTimeout));
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_TIME, static_cast<long>(options.readTimeout));
    curl_easy_setopt(handle, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &CurlWrite);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, this);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, &CurlHeader);
    curl_easy_setopt(handle, CURLOPT_HEADERDATA, this);
    curl_easy_setopt(handle, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(handle, CURLOPT_XFERINFOFUNCTION, &CurlProgress);
    curl_easy_setopt(handle, CURLOPT_XFERINFODATA, this);

    int retryWeight = 0;
    while (!cancelled) {
        const auto status = curl_easy_perform(handle);
        long response = 0;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response);
        if (status == CURLE_OK && response == 200) {
            return MC_OK;
        }
        /* retry transient failures only before consuming body bytes. a second
        response appended to a partial track would corrupt the decoder's input. */
        std::unique_lock lock(mutex);
        if (received || retryWeight >= 10 || (response >= 400 && response < 500 && response != 429) ||
            status == CURLE_PEER_FAILED_VERIFICATION)
        {
            if (!cancelled && this->host.log) {
                std::string message = "HTTP transfer failed: ";
                message += curl_easy_strerror(status);
                if (status == CURLE_PEER_FAILED_VERIFICATION && std::strstr(curlError, "certificate has expired")) {
                    message += " (certificate has expired)";
                }
                if (response) {
                    message += " (HTTP " + std::to_string(response) + ")";
                }
                constexpr mc_string_view component { "HttpDataStream", sizeof("HttpDataStream") - 1 };
                this->host.log(this->host.host_context, MC_LOG_ERROR, component, view(message));
            }
            break;
        }
        retryWeight += response == 429 ? 1 : 2;
        changed.wait_for(lock, std::chrono::seconds(response == 429 ? 5 : 2), [&] {
            return cancelled.load();
        });
    }
    return cancelled ? MC_ERROR_CANCELLED : MC_ERROR_IO;
}

size_t HttpDataStream::WriteBytes(const char* data, size_t size) {
    if (cancelled || !size) {
        return 0;
    }
    const size_t written = fwrite(data, 1, size, writer);
    std::lock_guard lock(mutex);
    received += written;
    if (received - available >= static_cast<size_t>(options.chunk) ||
        (available < static_cast<size_t>(options.precache) && received >= static_cast<size_t>(options.precache)))
    {
        if (fflush(writer) != 0) {
            return 0;
        }
        available = received;
        changed.notify_all();
    }
    return written;
}

void HttpDataStream::ReadHeader(std::string_view header) {
    if (header.compare(0, 5, "HTTP/") == 0) {
        std::lock_guard lock(mutex);
        /* redirects and proxy handshakes each have their own headers. only
        the final response describes the bytes passed to the decoder. */
        if (!received) {
            length.reset();
            if (!explicitContentType) {
                contentType.clear();
            }
        }
        return;
    }
    const auto separator = header.find(':');
    if (separator == std::string_view::npos) {
        return;
    }
    auto key = std::string(header.substr(0, separator));
    std::transform(key.begin(), key.end(), key.begin(), lower);
    auto value = musik::core::sdk::str::Trim(std::string(header.substr(separator + 1)));
    std::lock_guard lock(mutex);
    if (received) {
        return;
    }
    if (key == "content-length") {
        uint64_t n = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), n);
        if (parsed.ec == std::errc() && parsed.ptr == value.data() + value.size()) {
            length = n;
        }
    }
    else if (key == "content-type" && contentType.empty()) {
        contentType = value.substr(0, value.find(';'));
    }
}

size_t HttpDataStream::CurlWrite(char* data, size_t size, size_t count, void* context) {
    return static_cast<HttpDataStream*>(context)->WriteBytes(data, size * count);
}

size_t HttpDataStream::CurlHeader(char* data, size_t size, size_t count, void* context) {
    try {
        static_cast<HttpDataStream*>(context)->ReadHeader({ data, size * count });
        return size * count;
    }
    catch (...) {
        return 0;
    }
}

int HttpDataStream::CurlProgress(void* context, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    return static_cast<HttpDataStream*>(context)->cancelled.load() ? 1 : 0;
}
