//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <set>
#include "constants.h"

namespace musik { namespace core { namespace sdk {
    template <typename T>
    class HttpClient: public std::enable_shared_from_this<HttpClient<T>> {
        public:
            enum class Thread { Current, Background };
            enum class HttpMethod { Get, Post };

            using HttpHeaders = std::unordered_map<std::string, std::string>;
            using Callback = std::function<void(HttpClient<T>* caller, int, CURLcode)>;
            using DecoratorCallback = std::function<void(CURL*)>;
            using HeaderCallback = std::function<void(std::string, std::string)>;
            using CanceledCallback = std::function<void(HttpClient<T>* caller)>;

            static std::shared_ptr<HttpClient<T>> Create(T&& stream) {
               return std::shared_ptr<HttpClient<T>>(new HttpClient<T>(std::move(stream)));
            }

            ~HttpClient();

            HttpClient<T>& Url(const std::string& url);
            HttpClient<T>& Header(const std::string& key, const std::string& value);
            HttpClient<T>& Headers(HeaderCallback headersCb);
            HttpClient<T>& Decorator(DecoratorCallback decoratorCb);
            HttpClient<T>& Canceled(CanceledCallback canceledCb);
            HttpClient<T>& Mode(Thread mode);
            HttpClient<T>& PostBody(const std::string& postBody);
            HttpClient<T>& Method(HttpMethod mode);
            HttpClient<T>& UserAgent(const std::string& userAgent);

            const T& Stream() const { return this->ostream; }
            const HttpHeaders& ResponseHeaders() const { return this->responseHeaders; }
            const HttpHeaders& RequestHeaders() const { return this->requestHeaders; }
            const std::string& Url() const { return this->url; }

            HttpClient<T>& Run(Callback callback = Callback());
            void Wait();
            void Cancel();

        private:
            HttpClient(T&& stream);

            static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
            static int CurlTransferCallback(void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow);
            static size_t CurlHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata);
            static std::string DefaultUserAgent();

            static void ReplaceAll(std::string& input, const std::string& find, const std::string& replace);
            static std::string Trim(const std::string &str);

            void RunOnCurrentThread(Callback callback);

            std::mutex mutex;
            std::shared_ptr<std::thread> thread;

            T ostream;
            std::string url;
            std::string postBody;
            std::string userAgent;
            HttpHeaders requestHeaders, responseHeaders;
            HeaderCallback headersCb;
            DecoratorCallback decoratorCb;
            CanceledCallback canceledCallback;
            bool cancel;
            Thread mode{ Thread::Background };
            HttpMethod method{ HttpMethod::Get };
            CURL* curl;

            static std::mutex instanceMutex;
            static std::set<std::shared_ptr<HttpClient<T>>> instances;
    };

    template <typename T>
    std::mutex HttpClient<T>::instanceMutex;

    template <typename T>
    std::set<std::shared_ptr<HttpClient<T>>> HttpClient<T>::instances;

    template <typename T>
    std::string HttpClient<T>::DefaultUserAgent() {
#ifdef WIN32
        static const std::string PLATFORM = "win32";
#elif defined __APPLE__
        static const std::string PLATFORM = "macos";
#else
        static const std::string PLATFORM = "linux";
#endif

        return "musikcore sdk " +
            std::to_string(SdkVersion) + "." +
            "(" + PLATFORM + ")";
    }

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

    template <typename T> /* copied from Common.h for SDK usage. */
    void HttpClient<T>::ReplaceAll(std::string& input, const std::string& find, const std::string& replace) {
        size_t pos = input.find(find);
        while (pos != std::string::npos) {
            input.replace(pos, find.size(), replace);
            pos = input.find(find, pos + replace.size());
        }
    }

    template <typename T>
    std::string HttpClient<T>::Trim(const std::string &str) {
        std::string s(str);

        s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            std::not1(std::ptr_fun<int, int>(std::isspace))));

        s.erase(std::find_if(s.rbegin(), s.rend(),
            std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());

        return s;
    }

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

    template <typename T>
    HttpClient<T>::HttpClient(T&& stream) {
        this->curl = nullptr;
        this->cancel = false;
        std::swap(this->ostream, stream);
    }

    template <typename T>
    HttpClient<T>::~HttpClient() {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (this->curl) {
            curl_easy_cleanup(this->curl);
        }

        if (this->thread && this->thread->joinable()) {
            this->cancel = true;
            this->thread->join();
        }
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Run(Callback callback) {
        std::unique_lock<std::mutex> lock(this->mutex);

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
        curl_easy_setopt(curl, CURLOPT_USERAGENT, DefaultUserAgent().c_str());
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
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, &CurlTransferCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &CurlHeaderCallback);

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
            std::unique_lock<std::mutex> lock(instanceMutex);
            instances.insert(this->shared_from_this());

            this->thread.reset(new std::thread([callback, this] {
                this->RunOnCurrentThread(callback);
            }));
        }
        else {
            this->RunOnCurrentThread(callback);
        }

        return *this;
    }

    template <typename T>
    void HttpClient<T>::RunOnCurrentThread(Callback callback) {
        CURLcode curlCode = curl_easy_perform(this->curl);

        if (this->cancel) {
            if (this->canceledCallback) {
                this->canceledCallback(this);
            }
        }

        int httpStatus = 0;
        curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &httpStatus);

        if (callback) {
            callback(this, httpStatus, curlCode);
        }

        if (this->thread) {
            this->thread->detach();
            this->thread.reset();
        }

        {
            std::unique_lock<std::mutex> lock(instanceMutex);
            instances.erase(instances.find(this->shared_from_this()));
        }
    }

    template <typename T>
    void HttpClient<T>::Wait() {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (this->thread && this->thread->joinable()) {
            this->thread->join();
        }
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
    HttpClient<T>& HttpClient<T>::Decorator(DecoratorCallback decoratorCb) {
        this->decoratorCb = decoratorCb;
        return *this;
    }

    template <typename T>
    HttpClient<T>& HttpClient<T>::Canceled(CanceledCallback canceledCb) {
        this->canceledCb = canceledCb;
        return *this;
    }

    template <typename T>
    void HttpClient<T>::Cancel() {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (this->thread) {
            this->cancel = true;
            if (this->thread->joinable()) {
                this->thread->join();
            }
        }
    }

} } }
