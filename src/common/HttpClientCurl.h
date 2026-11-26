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

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <curl/curl.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <stop_token>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include "version.h"
#include "HttpClientError.h"

namespace musik { namespace core { namespace sdk {
    template <typename T>
    class HttpClient: public std::enable_shared_from_this<HttpClient<T>> {
        public:
            enum class Thread { Current, Background };
            enum class HttpMethod { Get, Post };

            using HttpHeaders = std::unordered_map<std::string, std::string>;
            using Callback = std::function<void(HttpClient<T>* caller, int, bool)>;
            using HeaderCallback = std::function<void(std::string, std::string)>;
            using ResponseCallback = std::function<void()>;
            using CanceledCallback = std::function<void(HttpClient<T>* caller)>;

            static std::shared_ptr<HttpClient<T>> Create(T&& stream) {
               return std::shared_ptr<HttpClient<T>>(new HttpClient<T>(std::move(stream)));
            }

            ~HttpClient();

            HttpClient<T>& Url(const std::string& url);
            HttpClient<T>& Header(const std::string& key, const std::string& value);
            HttpClient<T>& Headers(HeaderCallback headersCb);
            HttpClient<T>& Response(ResponseCallback responseCb);
            HttpClient<T>& VerifyTls(bool enabled);
            HttpClient<T>& AllowUntrustedAuthority(bool enabled);
            HttpClient<T>& FollowRedirects(bool enabled);
            HttpClient<T>& ConnectTimeout(long milliseconds);
            HttpClient<T>& ReadTimeout(long milliseconds);
            HttpClient<T>& Timeout(long milliseconds);
            HttpClient<T>& Cancellation(std::stop_token token);
            HttpClient<T>& Canceled(CanceledCallback canceledCb);
            HttpClient<T>& Mode(Thread mode);
            HttpClient<T>& PostBody(const std::string& postBody);
            HttpClient<T>& Method(HttpMethod mode);
            HttpClient<T>& UserAgent(const std::string& userAgent);

            const T& Stream() const { return this->ostream; }
            const HttpHeaders& ResponseHeaders() const { return this->responseHeaders; }
            const HttpHeaders& RequestHeaders() const { return this->requestHeaders; }
            const std::string& Url() const { return this->url; }
            HttpError Error() const { return this->error; }
            const std::string& ErrorMessage() const { return this->errorMessage; }

            HttpClient<T>& Run(Callback callback = Callback());
            void Wait();
            void Cancel();

        private:
            static constexpr long DefaultTimeoutMs = 15000;

            HttpClient(T&& stream);

            static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
            static int CurlTransferCallback(void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow);
            static size_t CurlHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata);
#if LIBCURL_VERSION_NUM < 0x072000
            static int LegacyCurlTransferCallback(void* ptr, double downTotal, double downNow, double upTotal, double upNow);
#endif

            static std::string DefaultUserAgent();

            static void ReplaceAll(std::string& input, const std::string& find, const std::string& replace);
            static std::string Trim(const std::string &str);

            void RunOnCurrentThread(Callback callback);

            std::mutex mutex;
            std::condition_variable done;
            bool started { false };
            bool finished { false };

            T ostream;
            std::string url;
            std::string postBody;
            std::string userAgent;
            HttpHeaders requestHeaders, responseHeaders;
            HeaderCallback headersCb;
            ResponseCallback responseCb;
            std::stop_token cancellation;
            CanceledCallback canceledCb;
            std::atomic_bool cancel { false };
            long timeoutMs { DefaultTimeoutMs };
            long connectTimeoutMs { 0 };
            long readTimeoutMs { 0 };
            bool verifyTls { false };
            bool allowUntrustedAuthority { false };
            bool followRedirects { true };
            HttpError error { HttpError::None };
            std::string errorMessage;
            Thread mode{ Thread::Background };
            HttpMethod method{ HttpMethod::Get };
            CURL* curl;
            curl_slist* headerList { nullptr };
    };

    template <typename T>
    std::string HttpClient<T>::DefaultUserAgent() {
        return musik::cube::userAgent();
    }

    template <typename T>
    size_t HttpClient<T>::CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
        if (ptr && userdata) {
            HttpClient* context = static_cast<HttpClient*>(userdata);
            if (context->cancel || context->cancellation.stop_requested()) {
                return 0; /* aborts */
            }
            try {
                context->ostream.write(ptr, size * nmemb);
                if (!context->ostream) { return 0; }
            }
            catch (...) {
                return 0;
            }
        }
        return size * nmemb;
    }

    template <typename T>
    int HttpClient<T>::CurlTransferCallback(
        void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow)
    {
        HttpClient* context = static_cast<HttpClient*>(ptr);
        if (context->cancel || context->cancellation.stop_requested()) {
            return -1; /* kill the stream */
        }
        return 0; /* ok! */
    }

#if LIBCURL_VERSION_NUM < 0x072000
    template <typename T>
    int HttpClient<T>::LegacyCurlTransferCallback(
        void* ptr, double downTotal, double downNow, double upTotal, double upNow)
    {
        return CurlTransferCallback(
            ptr,
            (curl_off_t)downTotal,
            (curl_off_t)downNow,
            (curl_off_t)upTotal,
            (curl_off_t)upNow);
    }
#endif

    template <typename T> /* copied from Common.h for SDK usage. */
    void HttpClient<T>::ReplaceAll(std::string& input, const std::string& find, const std::string& replace) {
        size_t pos = input.find(find);
        while (pos != std::string::npos) {
            input.replace(pos, find.size(), replace);
            pos = input.find(find, pos + replace.size());
        }
    }

    template <typename T>
    std::string HttpClient<T>::Trim(const std::string &s) {
        /* so lazy https://stackoverflow.com/a/17976541 */
        auto front = std::find_if_not(s.begin(), s.end(), isspace);
        auto back = std::find_if_not(s.rbegin(), s.rend(), isspace).base();
        return (back <= front ? std::string() : std::string(front, back));
    }

    template <typename T>
    size_t HttpClient<T>::CurlHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
        HttpClient* stream = static_cast<HttpClient*>(userdata);

        try {
            std::string header(buffer, size * nitems);

            if (header.rfind("HTTP/", 0) == 0) {
                stream->responseHeaders.clear();
                if (stream->responseCb) {
                    stream->responseCb();
                }
            }

            ReplaceAll(header, "\r\n", "");
            size_t splitAt = header.find_first_of(":");
            if (splitAt != std::string::npos) {
                std::string key = Trim(header.substr(0, splitAt));
                std::string value = Trim(header.substr(splitAt + 1));
                stream->responseHeaders[key] = value;

                if (stream->headersCb) {
                    stream->headersCb(key, value);
                }
            }

            return size * nitems;
        }
        catch (...) {
            return 0;
        }
    }

    template <typename T>
    HttpClient<T>::HttpClient(T&& stream) {
        this->curl = nullptr;
        this->cancel = false;
        std::swap(this->ostream, stream);
    }

    template <typename T>
    HttpClient<T>::~HttpClient() {
        if (this->curl) {
            curl_easy_cleanup(this->curl);
        }
        if (this->headerList) {
            curl_slist_free_all(this->headerList);
        }
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Run(Callback callback) {
        {
            std::lock_guard lock(this->mutex);
            if (this->started) {
                throw std::runtime_error("already started");
            }
            this->started = true;
        }

        const std::string userAgent =
            this->userAgent.size() ? this->userAgent : DefaultUserAgent();

        this->curl = curl_easy_init();
        if (!this->curl) {
            throw std::runtime_error("could not create HTTP client");
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
        curl_easy_setopt(curl, CURLOPT_HEADER, 0);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, this->followRedirects ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, this->verifyTls && !this->allowUntrustedAuthority ? 1L : 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, this->verifyTls ? 2L : 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3000);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 7500);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 500);

        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CurlHeaderCallback);

#if LIBCURL_VERSION_NUM < 0x072000
        curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
        curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, &LegacyCurlTransferCallback);
#else
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CurlTransferCallback);
#endif

#if 0
        curl_easy_setopt(curl, CURLOPT_PROXY, "http://localhost");
        curl_easy_setopt(curl, CURLOPT_PROXYPORT, 8080);
#endif

        if (this->requestHeaders.size()) {
            for (auto it : this->requestHeaders) {
                std::string header = it.first + ": " + it.second;
                auto* next = curl_slist_append(this->headerList, header.c_str());
                if (!next) {
                    throw std::bad_alloc();
                }
                this->headerList = next;
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, this->headerList);
        }

        if (this->method == HttpMethod::Post) {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);

            if (this->postBody.size()) {
                curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, this->postBody.c_str());
            }
        }

        if (this->timeoutMs > 0) {
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, this->timeoutMs);
        }
        if (this->connectTimeoutMs > 0) {
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, this->connectTimeoutMs);
        }
        if (this->readTimeoutMs > 0) {
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,
                this->readTimeoutMs / 1000 + (this->readTimeoutMs % 1000 != 0));
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
        }

        if (mode == Thread::Background) {
            auto instance = this->shared_from_this();
            std::thread([callback, instance] {
                instance->RunOnCurrentThread(callback);
            }).detach();
        }
        else {
            this->RunOnCurrentThread(callback);
        }

        return *this;
    }

    template <typename T>
    void HttpClient<T>::RunOnCurrentThread(Callback callback) {
        long httpStatus = 0;
        CURLcode curlCode = curl_easy_perform(this->curl);
        curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &httpStatus);
        if (this->cancel || this->cancellation.stop_requested()) {
            this->error = HttpError::Cancelled;
        }
        else if (curlCode == CURLE_PEER_FAILED_VERIFICATION) {
            this->error = HttpError::Certificate;
        }
        else if (curlCode == CURLE_OPERATION_TIMEDOUT) {
            this->error = HttpError::Timeout;
        }
        else if (curlCode == CURLE_COULDNT_CONNECT || curlCode == CURLE_COULDNT_RESOLVE_HOST) {
            this->error = HttpError::Connection;
        }
        else if (curlCode == CURLE_PARTIAL_FILE) {
            this->error = HttpError::Partial;
        }
        else if (curlCode != CURLE_OK) {
            this->error = HttpError::Other;
        }
        this->errorMessage = curl_easy_strerror(curlCode);

        if (this->cancel || this->cancellation.stop_requested()) {
            if (this->canceledCb) {
                this->canceledCb(this);
            }
        }

        if (callback) {
            callback(this, httpStatus, curlCode == CURLE_OK);
        }

        {
            std::lock_guard lock(this->mutex);
            this->finished = true;
        }
        this->done.notify_all();
    }

    template <typename T>
    void HttpClient<T>::Wait() {
        std::unique_lock lock(this->mutex);
        this->done.wait(lock, [this] { return !this->started || this->finished; });
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Url(const std::string& url) {
        this->url = url;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::PostBody(const std::string& postBody) {
        this->postBody = postBody;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Mode(Thread mode) {
        this->mode = mode;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Method(HttpMethod method) {
        this->method = method;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::UserAgent(const std::string& userAgent) {
        this->userAgent = userAgent;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Header(const std::string& key, const std::string& value) {
        this->requestHeaders[key] = value;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Headers(HeaderCallback headersCb) {
        this->headersCb = headersCb;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Response(ResponseCallback callback) { this->responseCb = std::move(callback); return *this; }

    template <typename T>
    HttpClient<T>& HttpClient<T>::VerifyTls(bool enabled) { this->verifyTls = enabled; return *this; }

    template <typename T>
    HttpClient<T>& HttpClient<T>::AllowUntrustedAuthority(bool enabled) { this->allowUntrustedAuthority = enabled; return *this; }

    template <typename T>
    HttpClient<T>& HttpClient<T>::FollowRedirects(bool enabled) { this->followRedirects = enabled; return *this; }

    template <typename T>
    HttpClient<T>& HttpClient<T>::ConnectTimeout(long milliseconds) { this->connectTimeoutMs = milliseconds; return *this; }

    template <typename T>
    HttpClient<T>& HttpClient<T>::ReadTimeout(long milliseconds) { this->readTimeoutMs = milliseconds; return *this; }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Timeout(long milliseconds) {
        this->timeoutMs = milliseconds;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Cancellation(std::stop_token token) {
        this->cancellation = token;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Canceled(CanceledCallback canceledCb) {
        this->canceledCb = canceledCb;
        return *this;
    }

    template <typename T>
    void HttpClient<T>::Cancel() {
        this->cancel = true;
    }

} } }
