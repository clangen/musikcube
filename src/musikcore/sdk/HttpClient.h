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

/** @file HttpClient.h @brief Defines the HttpClient template, a curl-based HTTP client for SDK use. */
#pragma once

#pragma warning(push, 0)
#include <curl/curl.h>
#pragma warning(pop)

#include <functional>
#include <algorithm>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <set>
#include "constants.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {
    /** @brief A fluent, curl-based HTTP client that streams response data into a
     *  caller-provided sink, optionally running asynchronously on a background thread.
     *  @tparam T The sink type that received response bytes are written to. */
    template <typename T>
    class HttpClient: public std::enable_shared_from_this<HttpClient<T>> {
        public:
            /** @brief Whether the request runs on the current thread or a background thread. */
            enum class Thread { Current, Background };
            /** @brief The HTTP verb to use for the request. */
            enum class HttpMethod { Get, Post };

            /** @brief The set of request or response headers. */
            using HttpHeaders = std::unordered_map<std::string, std::string>;
            /** @brief The callback invoked when the request completes.
             *  @param caller The client that performed the request.
             *  @param httpStatus The HTTP status code.
             *  @param curlCode The libcurl result code. */
            using Callback = std::function<void(HttpClient<T>* caller, int, CURLcode)>;
            /** @brief The callback invoked to customize the underlying CURL handle. */
            using DecoratorCallback = std::function<void(CURL*)>;
            /** @brief The callback invoked for each response header received.
             *  @param key The header name.
             *  @param value The header value. */
            using HeaderCallback = std::function<void(std::string, std::string)>;
            /** @brief The callback invoked when a request is canceled. */
            using CanceledCallback = std::function<void(HttpClient<T>* caller)>;

            /** @brief Creates a new client that streams into the given sink.
             *  @param stream The sink to write response data into.
             *  @return A shared client instance. */
            static std::shared_ptr<HttpClient<T>> Create(T&& stream) {
               return std::shared_ptr<HttpClient<T>>(new HttpClient<T>(std::move(stream)));
            }

            /** @brief Cleans up the underlying CURL handle and background thread. */
            ~HttpClient();

            /** @brief Sets the request URL.
             *  @param url The URL to request.
             *  @return This client, for chaining. */
            HttpClient<T>& Url(const std::string& url);
            /** @brief Adds a request header.
             *  @param key The header name.
             *  @param value The header value.
             *  @return This client, for chaining. */
            HttpClient<T>& Header(const std::string& key, const std::string& value);
            /** @brief Installs a callback for receiving response headers.
             *  @param headersCb The callback to invoke for each response header.
             *  @return This client, for chaining. */
            HttpClient<T>& Headers(HeaderCallback headersCb);
            /** @brief Installs a callback to customize the underlying CURL handle.
             *  @param decoratorCb The callback invoked with the CURL handle.
             *  @return This client, for chaining. */
            HttpClient<T>& Decorator(DecoratorCallback decoratorCb);
            /** @brief Installs a callback invoked when the request is canceled.
             *  @param canceledCb The callback to invoke on cancellation.
             *  @return This client, for chaining. */
            HttpClient<T>& Canceled(CanceledCallback canceledCb);
            /** @brief Sets the threading mode of the request.
             *  @param mode Whether to run on the current thread or a background thread.
             *  @return This client, for chaining. */
            HttpClient<T>& Mode(Thread mode);
            /** @brief Sets the request body to send with a POST request.
             *  @param postBody The body text.
             *  @return This client, for chaining. */
            HttpClient<T>& PostBody(const std::string& postBody);
            /** @brief Sets the HTTP verb to use.
             *  @param mode The HTTP method.
             *  @return This client, for chaining. */
            HttpClient<T>& Method(HttpMethod mode);
            /** @brief Sets the user agent header value.
             *  @param userAgent The user agent string.
             *  @return This client, for chaining. */
            HttpClient<T>& UserAgent(const std::string& userAgent);

            /** @brief Returns the sink that response data was written into.
             *  @return The response sink. */
            const T& Stream() const { return this->ostream; }
            /** @brief Returns the headers received in the response.
             *  @return The response headers. */
            const HttpHeaders& ResponseHeaders() const { return this->responseHeaders; }
            /** @brief Returns the headers sent with the request.
             *  @return The request headers. */
            const HttpHeaders& RequestHeaders() const { return this->requestHeaders; }
            /** @brief Returns the request URL.
             *  @return The URL. */
            const std::string& Url() const { return this->url; }

            /** @brief Performs the HTTP request.
             *  @param callback The callback invoked when the request completes.
             *  @return This client, for chaining. */
            HttpClient<T>& Run(Callback callback = Callback());
            /** @brief Blocks until an in-flight background request completes. */
            void Wait();
            /** @brief Requests that the in-flight request be canceled. */
            void Cancel();

        private:
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

            std::recursive_mutex mutex;
            std::shared_ptr<std::thread> thread;

            T ostream;
            std::string url;
            std::string postBody;
            std::string userAgent;
            HttpHeaders requestHeaders, responseHeaders;
            HeaderCallback headersCb;
            DecoratorCallback decoratorCb;
            CanceledCallback canceledCb;
            bool cancel;
            Thread mode{ Thread::Background };
            HttpMethod method{ HttpMethod::Get };
            CURL* curl;
    };

    /** @brief Returns a default user agent string identifying the SDK and platform. */
    template <typename T>
    std::string HttpClient<T>::DefaultUserAgent() {
#ifdef _WIN64
        static const std::string PLATFORM = "win64";
#elif WIN32
        static const std::string PLATFORM = "win32";
#elif defined __APPLE__
        static const std::string PLATFORM = "macos";
#elif defined __FreeBSD__
        static const std::string PLATFORM = "freebsd";
#elif defined __OpenBSD__
        static const std::string PLATFORM = "openbsd";
#else
        static const std::string PLATFORM = "linux";
#endif

        return "musikcore sdk " +
            std::to_string(SdkVersion) + "." +
            "(" + PLATFORM + ")";
    }

    /** @brief libcurl callback that writes received body bytes into the response sink. */
    template <typename T>
    size_t HttpClient<T>::CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata) {
        if (ptr && userdata) {
            HttpClient* context = static_cast<HttpClient*>(userdata);
            if (context->cancel) {
                return 0; /* aborts */
            }
            context->ostream.write(ptr, size * nmemb);
        }
        return size * nmemb;
    }

    /** @brief libcurl transfer progress callback used to abort canceled transfers. */
    template <typename T>
    int HttpClient<T>::CurlTransferCallback(
        void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow)
    {
        HttpClient* context = static_cast<HttpClient*>(ptr);
        if (context->cancel) {
            return -1; /* kill the stream */
        }
        return 0; /* ok! */
    }

#if LIBCURL_VERSION_NUM < 0x072000
    /** @brief Legacy libcurl progress callback for older libcurl versions. */
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

    /** @brief Replaces all occurrences of a substring in place.
     *  @param input The string to modify.
     *  @param find The substring to search for.
     *  @param replace The replacement substring. */
    template <typename T> /* copied from Common.h for SDK usage. */
    void HttpClient<T>::ReplaceAll(std::string& input, const std::string& find, const std::string& replace) {
        size_t pos = input.find(find);
        while (pos != std::string::npos) {
            input.replace(pos, find.size(), replace);
            pos = input.find(find, pos + replace.size());
        }
    }

    /** @brief Trims leading and trailing whitespace from a string.
     *  @param s The string to trim.
     *  @return The trimmed string. */
    template <typename T>
    std::string HttpClient<T>::Trim(const std::string &s) {
        /* so lazy https://stackoverflow.com/a/17976541 */
        auto front = std::find_if_not(s.begin(), s.end(), isspace);
        auto back = std::find_if_not(s.rbegin(), s.rend(), isspace).base();
        return (back <= front ? std::string() : std::string(front, back));
    }

    /** @brief libcurl callback that parses response headers into the header map. */
    template <typename T>
    size_t HttpClient<T>::CurlHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata) {
        HttpClient* stream = static_cast<HttpClient*>(userdata);

        std::string header(buffer, size * nitems);

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

    /** @brief Constructs a client, taking ownership of the given sink.
     *  @param stream The sink that response data will be written into. */
    template <typename T>
    HttpClient<T>::HttpClient(T&& stream) {
        this->curl = nullptr;
        this->cancel = false;
        std::swap(this->ostream, stream);
    }

    /** @brief Destroys the client, canceling any in-flight request and freeing CURL resources. */
    template <typename T>
    HttpClient<T>::~HttpClient() {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);

        if (this->curl) {
            curl_easy_cleanup(this->curl);
        }

        if (this->thread && this->thread->joinable()) {
            this->cancel = true;
            this->thread->join();
        }
    }

    /** @brief Performs the HTTP request, either inline or on a background thread.
     *  @param callback The callback invoked when the request completes.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Run(Callback callback) {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);

        const std::string userAgent =
            this->userAgent.size() ? this->userAgent : DefaultUserAgent();

        if (this->thread) {
            throw std::runtime_error("already started");
        }

        this->curl = curl_easy_init();

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HEADER, 0);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3000);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 7500);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 500);

        if (this->decoratorCb) {
            this->decoratorCb(this->curl);
        }

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
            struct curl_slist* slist = nullptr;
            for (auto it : this->requestHeaders) {
                std::string header = it.first + ": " + it.second;
                slist = curl_slist_append(slist, header.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
        }

        if (this->method == HttpMethod::Post) {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);

            if (this->postBody.size()) {
                curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, this->postBody.c_str());
            }
        }

        if (mode == Thread::Background) {
            auto instance = this->shared_from_this(); /* hold a reference so we don't dealloc */
            this->thread.reset(new std::thread([callback, instance, this] {
                this->RunOnCurrentThread(callback);
            }));
        }
        else {
            this->RunOnCurrentThread(callback);
        }

        return *this;
    }

    /** @brief Executes the request on the current thread and dispatches the completion callback. */
    template <typename T>
    void HttpClient<T>::RunOnCurrentThread(Callback callback) {
        long httpStatus = 0;
        CURLcode curlCode = curl_easy_perform(this->curl);
        curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &httpStatus);

        if (this->cancel) {
            if (this->canceledCb) {
                this->canceledCb(this);
            }
        }

        if (callback) {
            callback(this, httpStatus, curlCode);
        }

        std::shared_ptr<std::thread> oldThread = this->thread;

        {
            std::unique_lock<std::recursive_mutex> lock(this->mutex);
            this->thread.reset();
        }

        if (oldThread) {
            oldThread->detach();
        }
    }

    /** @brief Blocks until the in-flight background request completes. */
    template <typename T>
    void HttpClient<T>::Wait() {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);

        if (this->thread && this->thread->joinable()) {
            this->thread->join();
        }
    }

    /** @brief Sets the request URL.
     *  @param url The URL to request.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Url(const std::string& url) {
        this->url = url;
        return *this;
    }

    /** @brief Sets the request body for a POST request.
     *  @param postBody The body text.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::PostBody(const std::string& postBody) {
        this->postBody = postBody;
        return *this;
    }

    /** @brief Sets the threading mode of the request.
     *  @param mode Whether to run on the current thread or a background thread.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Mode(Thread mode) {
        this->mode = mode;
        return *this;
    }

    /** @brief Sets the HTTP verb to use.
     *  @param method The HTTP method.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Method(HttpMethod method) {
        this->method = method;
        return *this;
    }

    /** @brief Sets the user agent header value.
     *  @param userAgent The user agent string.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::UserAgent(const std::string& userAgent) {
        this->userAgent = userAgent;
        return *this;
    }

    /** @brief Adds a request header.
     *  @param key The header name.
     *  @param value The header value.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Header(const std::string& key, const std::string& value) {
        this->requestHeaders[key] = value;
        return *this;
    }

    /** @brief Installs a callback for receiving response headers.
     *  @param headersCb The callback to invoke for each response header.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Headers(HeaderCallback headersCb) {
        this->headersCb = headersCb;
        return *this;
    }

    /** @brief Installs a callback to customize the underlying CURL handle.
     *  @param decoratorCb The callback invoked with the CURL handle.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Decorator(DecoratorCallback decoratorCb) {
        this->decoratorCb = decoratorCb;
        return *this;
    }

    /** @brief Installs a callback invoked when the request is canceled.
     *  @param canceledCb The callback to invoke on cancellation.
     *  @return This client, for chaining. */
    template <typename T>
    HttpClient<T>& HttpClient<T>::Canceled(CanceledCallback canceledCb) {
        this->canceledCb = canceledCb;
        return *this;
    }

    /** @brief Requests that the in-flight request be canceled. */
    template <typename T>
    void HttpClient<T>::Cancel() {
        std::unique_lock<std::recursive_mutex> lock(this->mutex);
        this->cancel = true;
    }

} } }
