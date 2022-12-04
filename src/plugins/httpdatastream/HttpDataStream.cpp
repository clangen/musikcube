//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include "config.h"

#include "HttpDataStream.h"
#include "LruDiskCache.h"

#include <musikcore/sdk/IEnvironment.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/ISchema.h>

#include <iostream>
#include <algorithm>
#include <string>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <atomic>

#pragma warning(push, 0)
#include <boost/algorithm/string.hpp>

/* meh... */
#include <../../3rdparty/include/nlohmann/json.hpp>
#include <../../3rdparty/include/websocketpp/base64/base64.hpp>
#pragma warning(pop)

#ifdef WIN32
#include <Windows.h>
#define sleepMs(x) Sleep(x);
#define DLLEXPORT __declspec(dllexport)
#else
#include <unistd.h>
#define sleepMs(x) usleep(x * 1000)
#define DLLEXPORT
#endif

using namespace std::chrono;
using namespace musik::core::sdk;
namespace al = boost::algorithm;

static std::mutex globalMutex;
static IEnvironment* environment;
static LruDiskCache diskCache;
static std::string cachePath;
static IPreferences* prefs;
static std::atomic<int64_t> nextInstanceId(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());

static const int kDefaultMaxCacheFiles = 35;
static const int kDefaultPreCacheSizeBytes = 524288; /*2^19 */
static const int kDefaultChunkSizeBytes = 131072; /* 2^17 */
static const int kDefaultConnectionTimeoutSeconds = 15;
static const int kDefaultReadTimeoutSeconds = 30;

static const std::string kMaxCacheFiles = "max_cache_files";
static const std::string kPreCacheBufferSizeBytesKey = "precache_buffer_size_bytes";
static const std::string kChunkSizeBytesKey = "chunk_size_bytes";
static const std::string kConnectionTimeoutSecondsKey = "connection_timeout_seconds";
static const std::string kReadTimeoutSecondsKey = "read_timeout_seconds";

const std::string HttpDataStream::kRemoteTrackHost = "musikcore://remote-track/";

extern "C" DLLEXPORT void SetEnvironment(IEnvironment* environment) {
    std::unique_lock<std::mutex> lock(globalMutex);
    ::environment = environment;

    if (environment) {
        static char buffer[2046];
        environment->GetPath(PathType::Data, buffer, sizeof(buffer));
        cachePath = std::string(buffer) + "/cache/httpclient/";
    }
}

extern "C" DLLEXPORT void SetPreferences(IPreferences * prefs) {
    ::prefs = prefs;
}

extern "C" DLLEXPORT musik::core::sdk::ISchema * GetSchema() {
    auto schema = new TSchema<>();
    schema->AddInt(kMaxCacheFiles, kDefaultMaxCacheFiles);
    schema->AddInt(kPreCacheBufferSizeBytesKey, kDefaultPreCacheSizeBytes, 32768);
    schema->AddInt(kChunkSizeBytesKey, kDefaultChunkSizeBytes, 32768);
    schema->AddInt(kConnectionTimeoutSecondsKey, kDefaultConnectionTimeoutSeconds, 1);
    schema->AddInt(kReadTimeoutSecondsKey, kDefaultReadTimeoutSeconds, 1);
    return schema;
}

static bool parseHeader(std::string raw, std::string& key, std::string& value) {
    al::replace_all(raw, "\r\n", "");

    size_t splitAt = raw.find_first_of(":");
    if (splitAt != std::string::npos) {
        key = boost::trim_copy(raw.substr(0, splitAt));
        value = boost::trim_copy(raw.substr(splitAt + 1));
        return true;
    }

    return false;
}

static size_t cacheId(const std::string& uri) {
    return std::hash<std::string>()(uri);
}

class FileReadStream {
    public:
        FileReadStream(FILE* file, long maxLength) {
            this->file = file;
            this->maxLength = maxLength;
            this->Reset();
        }

        FileReadStream(const std::string& fn, int64_t instanceId) {
            this->file = diskCache.Open(cacheId(fn), instanceId, "rb");
            this->maxLength = -1;
            Reset();
        }

        ~FileReadStream() {
            if (this->file) {
                fclose(this->file);
                this->file = nullptr;
            }
        }

        void Reset() {
            this->interrupted = false;
            this->length = 0;

            if (this->file) {
                fseek(this->file, 0, SEEK_END);
                this->length = (PositionType) ftell(this->file);
                fseek(this->file, 0, SEEK_SET);
            }
        }

        void Interrupt() {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->interrupted = true;
            this->underflow.notify_all();
        }

        void Add(PositionType length) {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->length += length;
            this->underflow.notify_all();
        }

        void Completed() {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->maxLength = length;
        }

        PositionType Read(void* buffer, PositionType readBytes) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (this->Position() >= this->length && !this->Eof() && !this->interrupted) {
                this->underflow.wait(lock);
            }

            if (this->interrupted || this->Eof()) {
                return 0;
            }

            clearerr(this->file);
            const int available = this->length - this->Position();
            const unsigned actualReadBytes = std::max(0, std::min(available, (int) readBytes));
            return (PositionType) fread(buffer, 1, actualReadBytes, this->file);
        }

        bool SetPosition(PositionType position) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (position > this->length && !this->Eof() && !this->interrupted) {
                this->underflow.wait(lock);
            }

            /* if we've been interrupted, or we know we're at EOF and have been asked
            to read beyond it. */
            if (this->interrupted || (position >= this->Position() && this->Eof())) {
                return false;
            }

            return (fseek(this->file, position, SEEK_SET) == 0);
        }

        PositionType Position() {
            return (this->file) ? ftell(this->file) : 0;
        }

    private:
        bool Eof() {
            return this->maxLength > 0 && this->Position() >= this->maxLength;
        }

        FILE* file;
        PositionType length, maxLength;
        std::condition_variable underflow;
        std::mutex mutex;
        bool interrupted;
};

HttpDataStream::HttpDataStream() {
    this->length = this->totalWritten = this->written = 0;
    this->state = State::NotStarted;
    this->writeFile = nullptr;
    this->interrupted = false;
    this->instanceId = ++nextInstanceId;
}

HttpDataStream::~HttpDataStream() {
    this->Close();
}

void HttpDataStream::Interrupt() {
    std::unique_lock<std::mutex> lock(this->stateMutex);

    auto reader = this->reader;
    auto downloadThread = this->downloadThread;

    if (reader) {
        reader->Interrupt();
    }

    if (downloadThread) {
        this->interrupted = true;
    }
}

bool HttpDataStream::CanPrefetch() {
    return true;
}

bool HttpDataStream::Open(const char *rawUri, OpenFlags flags) {
    if ((flags & OpenFlags::Write) != 0) {
        return false;
    }

    this->precacheSizeBytes = prefs->GetInt(kPreCacheBufferSizeBytesKey.c_str(), kDefaultPreCacheSizeBytes);
    this->chunkSizeBytes = prefs->GetInt(kChunkSizeBytesKey.c_str(), kDefaultChunkSizeBytes);
    this->maxCacheFiles = prefs->GetInt(kMaxCacheFiles.c_str(), kDefaultMaxCacheFiles);

    std::unordered_map<std::string, std::string> requestHeaders;

    {
        std::unique_lock<std::mutex> lock(this->stateMutex);

        diskCache.Init(cachePath, this->maxCacheFiles);

        this->httpUri = rawUri;

        if (this->httpUri.find(kRemoteTrackHost) == 0) {
            try {
                nlohmann::json options = nlohmann::json::parse(
                    this->httpUri.substr(kRemoteTrackHost.size()));
                this->httpUri = options["uri"].get<std::string>();
                this->originalUri = options["originalUri"].get<std::string>();
                this->type = options.value("type", ".mp3");

                std::string password = options.value("password", "");
                std::string headerValue = "Basic " + websocketpp::base64_encode("default:" + password);
                requestHeaders["Authorization"] = headerValue;
            }
            catch (...) {
                /* malformed payload. not much we can do. */
                return false;
            }
        }

        auto const id = cacheId(httpUri);

        if (diskCache.Cached(id)) {
            FILE* file = diskCache.Open(id, this->instanceId, "rb", this->type, this->length);
            if (file) {
                this->reader = std::make_shared<FileReadStream>(file, this->length);
                this->state = State::Cached;
                return true;
            }
            else {
                diskCache.Delete(id, this->instanceId);
            }
        }

        this->ResetFileHandles();
    }

    if (this->writeFile && this->reader) {
        this->curlEasy = curl_easy_init();

        // curl_easy_setopt (this->curlEasy, CURLOPT_VERBOSE, verbose);

        curl_easy_setopt(this->curlEasy, CURLOPT_URL, this->httpUri.c_str());
        curl_easy_setopt(this->curlEasy, CURLOPT_HEADER, 0);
        curl_easy_setopt(this->curlEasy, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_USERAGENT, "musikcube HttpDataStream");
        curl_easy_setopt(this->curlEasy, CURLOPT_NOPROGRESS, 0);

        curl_easy_setopt(this->curlEasy, CURLOPT_WRITEHEADER, this);
        curl_easy_setopt(this->curlEasy, CURLOPT_HEADERFUNCTION, &HttpDataStream::CurlReadHeaderCallback);

        curl_easy_setopt(this->curlEasy, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(this->curlEasy, CURLOPT_WRITEFUNCTION, &HttpDataStream::CurlWriteCallback);

#if LIBCURL_VERSION_NUM < 0x072000
        curl_easy_setopt(this->curlEasy, CURLOPT_PROGRESSDATA, this);
        curl_easy_setopt(this->curlEasy, CURLOPT_PROGRESSFUNCTION, &HttpDataStream::LegacyCurlTransferCallback);
#else
        curl_easy_setopt(this->curlEasy, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(this->curlEasy, CURLOPT_XFERINFOFUNCTION, &HttpDataStream::CurlTransferCallback);
#endif

        const int connectionTimeout = prefs->GetInt(kConnectionTimeoutSecondsKey.c_str(), kDefaultConnectionTimeoutSeconds);
        const int readTimeout = prefs->GetInt(kReadTimeoutSecondsKey.c_str(), kDefaultReadTimeoutSeconds);

        curl_easy_setopt (this->curlEasy, CURLOPT_CONNECTTIMEOUT, connectionTimeout);
        curl_easy_setopt (this->curlEasy, CURLOPT_LOW_SPEED_TIME, readTimeout);
        curl_easy_setopt(this->curlEasy, CURLOPT_LOW_SPEED_LIMIT, 1);

        // if (useproxy == 1) {
        //     curl_easy_setopt (this->curlEasy, CURLOPT_PROXY, proxyaddress);
        //     if (authproxy == 1) {
        //         curl_easy_setopt (this->curlEasy, CURLOPT_PROXYUSERPWD, proxyuserpass);
        //     }
        // }

        curl_easy_setopt(this->curlEasy, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(this->curlEasy, CURLOPT_SSL_VERIFYHOST, 0);

        /* append parsed headers, if any */
        if (requestHeaders.size()) {
            for (auto& kv : requestHeaders) {
                auto header = kv.first + ": " + kv.second;
                this->curlHeaders = curl_slist_append(this->curlHeaders, header.c_str());
            }
            curl_easy_setopt(this->curlEasy, CURLOPT_HTTPHEADER, this->curlHeaders);
        }

        /* start downloading... */
        this->state = State::Downloading;
        downloadThread = std::make_shared<std::thread>(&HttpDataStream::ThreadProc, this);

        /* wait until we have a few hundred k of data */
        std::unique_lock<std::mutex> lock(this->stateMutex);
        startedContition.wait(lock);

        return this->state != State::Error;
    }

    return false;
}

void HttpDataStream::ResetFileHandles() {
    if (this->writeFile) {
        fclose(this->writeFile);
        this->writeFile = nullptr;
    }

    if (this->reader) {
        this->reader->Interrupt();
        this->reader.reset();
    }

    auto const id = cacheId(httpUri);
    diskCache.Delete(id, this->instanceId);
    this->writeFile = diskCache.Open(id, this->instanceId, "wb");
    if (this->writeFile) {
        this->reader = std::make_shared<FileReadStream>(this->httpUri, this->instanceId);
    }
}

void HttpDataStream::ThreadProc() {
    if (this->curlEasy) {
        static const int kMaxRetries = 10;
        int retryCount = 0; /* note: weighted based on failure type */
        while (this->state != State::Downloaded && !this->interrupted) {
            auto const curlCode = curl_easy_perform(this->curlEasy);
            long httpStatusCode = 0;
            curl_easy_getinfo(this->curlEasy, CURLINFO_RESPONSE_CODE, &httpStatusCode);
            if (httpStatusCode == 200) {
                this->state = curlCode != CURLE_OK
                    ? State::Aborted
                    : State::Downloaded;

                if (this->reader) {
                    if (this->written > 0) {
                        this->reader->Add(this->written);
                        this->written = 0;
                    }
                    this->reader->Completed();
                }
            }
            else {
                if (httpStatusCode == 429) { /* too many requests */
                     this->state = State::Retrying;
                    retryCount += 1;
                    sleepMs(5000);
                }
                else if ((httpStatusCode < 400 || httpStatusCode >= 500) && retryCount < kMaxRetries) {
                    {
                        std::unique_lock<std::mutex> lock(this->stateMutex);
                        this->ResetFileHandles();
                    }
                    this->state = State::Retrying;
                    retryCount += 2;
                    sleepMs(2000);
                }
                else {
                    this->state = State::Error;
                    this->interrupted = true;
                }
            }
        }

        startedContition.notify_all(); /* in case the header write function was never called */

        if (this->curlEasy) {
            curl_easy_cleanup(this->curlEasy);
            this->curlEasy = nullptr;
        }

        if (this->curlHeaders) {
            curl_slist_free_all(this->curlHeaders);
            this->curlHeaders = nullptr;
        }

        if (this->writeFile) {
            fclose(this->writeFile);
            this->writeFile = nullptr;
        }
    }
}

bool HttpDataStream::Close() {
    this->Interrupt();

    /* wait for the download thread to stop, this will ensure file writes have
    completed. */
    auto thread = this->downloadThread;
    this->downloadThread.reset();

    if (thread) {
        thread->join();
    }

    /* need to close the reader so we can perform the filesystem operations below */
    this->reader.reset();

    /* if we just downloaded the file let's rename it to its final name; if the
    download failed, let's delete the temp file. */
    auto id = cacheId(this->httpUri);
    if (this->state == State::Downloaded) {
        diskCache.Finalize(id, this->instanceId, this->Type());
    }
    else if (this->state != State::Cached) {
        diskCache.Delete(id, this->instanceId);
    }

    return true;
}

void HttpDataStream::Release() {
    this->Close();
    delete this;
}

PositionType HttpDataStream::Read(void* buffer, PositionType readBytes) {
    auto reader = this->reader;
    return reader ? reader->Read(buffer, readBytes) : 0;
}

bool HttpDataStream::SetPosition(PositionType position) {
    auto reader = this->reader;
    return reader ? reader->SetPosition(position) : 0;
}

bool HttpDataStream::Seekable() {
    return true;
}

PositionType HttpDataStream::Position() {
    auto reader = this->reader;
    return reader ? reader->Position() : 0;
}

bool HttpDataStream::Eof() {
    auto reader = this->reader;
    return !reader || reader->Position() >= (long) this->length;
}

long HttpDataStream::Length() {
    return (long) this->length;
}

const char* HttpDataStream::Type() {
    return this->type.c_str();
}

const char* HttpDataStream::Uri() {
    return this->originalUri.c_str();
}

size_t HttpDataStream::CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    HttpDataStream* stream = static_cast<HttpDataStream*>(userdata);

    const size_t total = size * nmemb;
    const size_t result = fwrite(ptr, size, nmemb, stream->writeFile);
    fflush(stream->writeFile); /* normally we wouldn't want to do this, but it ensures
     data written is available immediately to any simultaneous readers */
    stream->written += result;

    if (stream->written >= stream->chunkSizeBytes) {
        stream->reader->Add(stream->written);
        stream->written = 0;
    }

    if (stream->totalWritten > -1) {
        stream->totalWritten += result;
        if (stream->totalWritten >= stream->precacheSizeBytes) {
            stream->startedContition.notify_all();
            stream->totalWritten = -1;
        }
    }

    return result;
}

size_t HttpDataStream::CurlReadHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
    HttpDataStream* stream = static_cast<HttpDataStream*>(userdata);

    std::string header(buffer, size * nitems);

    std::string key, value;
    if (parseHeader(header, key, value)) {
        if (key == "Content-Length") {
            stream->length = std::atoi(value.c_str());
        }
        else if (key == "Content-Type") {
            if (!stream->type.size()) {
                stream->type = value;
            }
        }
    }

    return size * nitems;
}

int HttpDataStream::CurlTransferCallback(
    void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow)
{
    HttpDataStream* stream = static_cast<HttpDataStream*>(ptr);
    if (stream->interrupted) {
        return -1; /* kill the stream */
    }
    return 0; /* ok! */
}

#if LIBCURL_VERSION_NUM < 0x072000
int HttpDataStream::LegacyCurlTransferCallback(
    void *ptr, double downTotal, double downNow, double upTotal, double upNow)
{
    return CurlTransferCallback(
        ptr,
        (curl_off_t) downTotal,
        (curl_off_t) downNow,
        (curl_off_t) upTotal,
        (curl_off_t) upNow);
}
#endif