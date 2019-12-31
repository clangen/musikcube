//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include "stdafx.h"

#include "HttpDataStream.h"
#include "LruDiskCache.h"

#include <core/sdk/IEnvironment.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <algorithm>
#include <string>
#include <set>
#include <mutex>

#ifdef WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

using namespace musik::core::sdk;
namespace al = boost::algorithm;

static const int MAX_CACHE_FILES = 35;
static const int NOTIFY_INTERVAL_BYTES = 131072; /* 2^17 */

static std::mutex globalMutex;
static IEnvironment* environment;
static LruDiskCache diskCache;
static std::string cachePath;

extern "C" DLLEXPORT void SetEnvironment(IEnvironment* environment) {
    std::unique_lock<std::mutex> lock(globalMutex);
    ::environment = environment;

    if (environment) {
        static char buffer[2046];
        environment->GetPath(PathData, buffer, sizeof(buffer));
        cachePath = std::string(buffer) + "/cache/httpclient/";

        boost::filesystem::path p(cachePath);
        if (!boost::filesystem::exists(p)) {
            boost::filesystem::create_directories(p);
        }
    }
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
            this->Init();
        }

        FileReadStream(const std::string& fn) {
            this->file = diskCache.Open(cacheId(fn), "rb");
            this->maxLength = -1;
            Init();
        }

        ~FileReadStream() {
            if (this->file) {
                fclose(this->file);
                this->file = nullptr;
            }
        }

        void Init() {
            this->interrupted = false;
            this->length = 0;

            if (this->file) {
                fseek(this->file, 0, SEEK_END);
                this->length = (PositionType)ftell(this->file);
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

            if (this->interrupted && this->Eof()) {
                return 0;
            }

            clearerr(this->file);
            int available = this->length - this->Position();
            unsigned actualReadBytes = std::max(0, std::min(available, (int) readBytes));
            return (PositionType) fread(buffer, 1, actualReadBytes, this->file);
        }

        bool SetPosition(PositionType position) {
            std::unique_lock<std::mutex> lock(this->mutex);
            while (position > this->length && !this->Eof() && !this->interrupted) {
                this->underflow.wait(lock);
            }

            if (this->interrupted || this->Eof()) {
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
    this->length = this->written = 0;
    this->state = Idle;
    this->writeFile = nullptr;
    this->interrupted = false;
}

HttpDataStream::~HttpDataStream() {
    auto id = cacheId(this->uri);
    if (this->state == Finished) {
        diskCache.Finalize(id, this->Type());
    }
    else if (this->state != Cached) {
        diskCache.Delete(id);
    }
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

bool HttpDataStream::Open(const char *uri, OpenFlags flags) {
    if ((flags & OpenFlags::Write) != 0) {
        return false;
    }

    std::unique_lock<std::mutex> lock(this->stateMutex);

    diskCache.Init(cachePath, MAX_CACHE_FILES);

    this->uri = uri;

    auto id = cacheId(uri);

    if (diskCache.Cached(id)) {
        FILE* file = diskCache.Open(id, "rb", this->type, this->length);
        this->reader.reset(new FileReadStream(file, this->length));
        this->state = Cached;
        return true;
    }

    this->writeFile = diskCache.Open(id, "wb");

    if (this->writeFile) {
        this->reader.reset(new FileReadStream(uri));

        this->curlEasy = curl_easy_init();

        // curl_easy_setopt (this->curlEasy, CURLOPT_VERBOSE, verbose);

        curl_easy_setopt(this->curlEasy, CURLOPT_URL, uri);
        curl_easy_setopt(this->curlEasy, CURLOPT_HEADER, 0);
        curl_easy_setopt(this->curlEasy, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_USERAGENT, "musikcube HttpDataStream");
        curl_easy_setopt(this->curlEasy, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt(this->curlEasy, CURLOPT_WRITEHEADER, this);
        curl_easy_setopt(this->curlEasy, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(this->curlEasy, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(this->curlEasy, CURLOPT_WRITEFUNCTION, &HttpDataStream::CurlWriteCallback);
        curl_easy_setopt(this->curlEasy, CURLOPT_HEADERFUNCTION, &HttpDataStream::CurlHeaderCallback);
        curl_easy_setopt(this->curlEasy, CURLOPT_XFERINFOFUNCTION, &HttpDataStream::CurlTransferCallback);

        // curl_easy_setopt (this->curlEasy, CURLOPT_CONNECTTIMEOUT, connecttimeout);
        // curl_easy_setopt (this->curlEasy, CURLOPT_LOW_SPEED_TIME, readtimeout);
        // curl_easy_setopt(this->curlEasy, CURLOPT_LOW_SPEED_LIMIT, 1);

        // if (useproxy == 1) {
        //     curl_easy_setopt (this->curlEasy, CURLOPT_PROXY, proxyaddress);
        //     if (authproxy == 1) {
        //         curl_easy_setopt (this->curlEasy, CURLOPT_PROXYUSERPWD, proxyuserpass);
        //     }
        // }

        curl_easy_setopt(this->curlEasy, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(this->curlEasy, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(this->curlEasy, CURLOPT_SSL_VERIFYHOST, 0);

        /* start downloading... */
        this->state = Loading;
        downloadThread.reset(new std::thread(&HttpDataStream::ThreadProc, this));

        /* wait until headers have finished */
        startedContition.wait(lock);

        return true;
    }

    return false;
}

void HttpDataStream::ThreadProc() {
    if (this->curlEasy) {
        if (curl_easy_perform(this->curlEasy) == CURLE_OK) {
            this->state = Finished;
        }
        else {
            this->state = Error;
        }

        if (this->reader) {
            if (this->written > 0) {
                this->reader->Add(this->written);
                this->written = 0;
            }

            this->reader->Completed();
        }

        startedContition.notify_all();

        if (this->curlEasy) {
            curl_easy_cleanup(this->curlEasy);
            this->curlEasy = nullptr;
        }

        if (this->writeFile) {
            fclose(this->writeFile);
            this->writeFile = nullptr;
        }
    }
}

bool HttpDataStream::Close() {
    this->Interrupt();
    this->reader.reset();

    if (this->downloadThread) {
        downloadThread->join();
        this->downloadThread.reset();
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
    return this->uri.c_str();
}

size_t HttpDataStream::CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
    HttpDataStream* stream = static_cast<HttpDataStream*>(userdata);

    stream->startedContition.notify_all();

    size_t total = size * nmemb;
    size_t result = fwrite(ptr, size, nmemb, stream->writeFile);
    stream->written += result;

    if (stream->written > NOTIFY_INTERVAL_BYTES) {
        fflush(stream->writeFile);
        stream->reader->Add(stream->written);
        stream->written = 0;
    }

    return result;
}

size_t HttpDataStream::CurlHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
    HttpDataStream* stream = static_cast<HttpDataStream*>(userdata);

    std::string header(buffer, size * nitems);

    std::string key, value;
    if (parseHeader(header, key, value)) {
        if (key == "Content-Length") {
            stream->length = std::atoi(value.c_str());
        }
        else if (key == "Content-Type") {
            stream->type = value;
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