#pragma once

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <algorithm>
#include <climits>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "version.h"
#include "HttpClientError.h"

namespace musik { namespace core { namespace sdk {
    /* performs HTTP requests with WinHTTP while retaining the shared client API. */
    template<typename T> class HttpClient : public std::enable_shared_from_this<HttpClient<T>> {
    public:
        enum class Thread { Current, Background };
        enum class HttpMethod { Get, Post };
        using HttpHeaders = std::unordered_map<std::string, std::string>;
        using Callback = std::function<void(HttpClient<T>*, int, bool)>;
        using HeaderCallback = std::function<void(std::string, std::string)>;
        using ResponseCallback = std::function<void()>;
        using CanceledCallback = std::function<void(HttpClient<T>*)>;

        static std::shared_ptr<HttpClient<T>> Create(T&& stream) {
            return std::shared_ptr<HttpClient<T>>(new HttpClient<T>(std::move(stream)));
        }

        HttpClient& Url(const std::string& value) {
            this->url = value;
            return *this;
        }
        HttpClient& Header(const std::string& key, const std::string& value) {
            this->requestHeaders[key] = value;
            return *this;
        }
        HttpClient& Headers(HeaderCallback callback) {
            this->headersCb = std::move(callback);
            return *this;
        }
        HttpClient& Response(ResponseCallback callback) {
            this->responseCb = std::move(callback);
            return *this;
        }
        HttpClient& VerifyTls(bool enabled) {
            this->verifyTls = enabled;
            return *this;
        }
        HttpClient& AllowUntrustedAuthority(bool enabled) {
            this->allowUntrustedAuthority = enabled;
            return *this;
        }
        HttpClient& FollowRedirects(bool enabled) {
            this->followRedirects = enabled;
            return *this;
        }
        HttpClient& ConnectTimeout(long milliseconds) {
            this->connectTimeoutMs = milliseconds;
            return *this;
        }
        HttpClient& ReadTimeout(long milliseconds) {
            this->readTimeoutMs = milliseconds;
            return *this;
        }
        HttpClient& Timeout(long milliseconds) {
            this->timeoutMs = milliseconds;
            return *this;
        }
        HttpClient& Cancellation(std::stop_token token) {
            this->cancellation = token;
            return *this;
        }
        HttpClient& Canceled(CanceledCallback callback) {
            this->canceledCb = std::move(callback);
            return *this;
        }
        HttpClient& Mode(Thread value) {
            this->mode = value;
            return *this;
        }
        HttpClient& PostBody(const std::string& value) {
            this->postBody = value;
            return *this;
        }
        HttpClient& Method(HttpMethod value) {
            this->method = value;
            return *this;
        }
        HttpClient& UserAgent(const std::string& value) {
            this->userAgent = value;
            return *this;
        }
        const T& Stream() const { return this->ostream; }
        const HttpHeaders& ResponseHeaders() const { return this->responseHeaders; }
        const HttpHeaders& RequestHeaders() const { return this->requestHeaders; }
        const std::string& Url() const { return this->url; }
        HttpError Error() const { return this->error; }
        const std::string& ErrorMessage() const { return this->errorMessage; }

        HttpClient& Run(Callback callback = Callback()) {
            /* keep a background transfer alive until its callback has returned. */
            {
                std::lock_guard lock(this->mutex);
                if (this->started) {
                    throw std::runtime_error("already started");
                }
                this->started = true;
            }
            if (this->mode == Thread::Background) {
                auto self = this->shared_from_this();
                std::thread([self, callback = std::move(callback)] {
                    self->Execute(callback);
                }).detach();
            }
            else {
                this->Execute(callback);
            }
            return *this;
        }

        void Wait() {
            std::unique_lock lock(this->mutex);
            this->done.wait(lock, [this] {
                return !this->started || this->finished;
            });
        }
        void Cancel() { this->localStop.request_stop(); }

    private:
        static constexpr long DefaultTimeoutMs = 15000;

        /* WinHTTP handles form a session, connection, and request hierarchy.
        their owners are declared in that order so they close in reverse. */
        class Handle {
        public:
            explicit Handle(HINTERNET value = nullptr)
            : value(value) {}
            ~Handle() {
                if (this->value && !WinHttpCloseHandle(this->value)) {
                    std::fputs("WinHTTP: could not close handle\n", stderr);
                    std::abort();
                }
            }
            Handle(const Handle&) = delete;
            Handle& operator=(const Handle&) = delete;
            HINTERNET Get() const { return this->value; }

        private:
            HINTERNET value;
        };

        /* WinHTTP may invoke callbacks after close returns. keep this state
        alive until the final handle-closing notification has arrived. */
        class AsyncRequest {
        public:
            explicit AsyncRequest(HINTERNET request)
            : request(request) {}
            ~AsyncRequest() {
                if (!this->request) {
                    return;
                }
                if (!WinHttpCloseHandle(this->request)) {
                    std::fputs("WinHTTP: could not close request\n", stderr);
                    std::abort();
                }
                if (this->registered) {
                    std::unique_lock lock(this->mutex);
                    this->changed.wait(lock, [this] {
                        return this->closed;
                    });
                }
            }
            AsyncRequest(const AsyncRequest&) = delete;
            AsyncRequest& operator=(const AsyncRequest&) = delete;

            bool Initialize() {
                DWORD_PTR context = reinterpret_cast<DWORD_PTR>(this);
                if (!WinHttpSetOption(
                        this->request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof(context)))
                {
                    return false;
                }
                if (WinHttpSetStatusCallback(this->request, &OnStatus,
                        WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES,
                        0) == WINHTTP_INVALID_STATUS_CALLBACK)
                {
                    return false;
                }
                this->registered = true;
                return true;
            }
            HINTERNET Get() const { return this->request; }
            void Wake() {
                /* synchronize with the wait predicate so a stop request
                cannot arrive between checking it and going to sleep. */
                std::lock_guard lock(this->mutex);
                this->changed.notify_all();
            }
            void Prepare() {
                std::lock_guard lock(this->mutex);
                this->event = 0;
                this->failed = false;
                this->errorCode = 0;
                this->timedOut = false;
            }
            bool Start(BOOL started) {
                if (started) {
                    return true;
                }
                const DWORD error = GetLastError();
                std::lock_guard lock(this->mutex);
                this->errorCode = error;
                this->failed = true;
                return false;
            }
            bool Wait(DWORD expected, std::chrono::steady_clock::time_point deadline, std::stop_token external,
                std::stop_token local) {
                std::unique_lock lock(this->mutex);
                const bool ready = this->changed.wait_until(lock, deadline, [&] {
                    return this->event == expected || this->failed || external.stop_requested() ||
                        local.stop_requested();
                });
                this->timedOut = !ready;
                return !this->failed && !external.stop_requested() && !local.stop_requested() &&
                    this->event == expected;
            }
            DWORD BytesRead() const { return this->bytesRead; }
            DWORD ErrorCode() const { return this->errorCode; }
            bool TimedOut() const { return this->timedOut; }

        private:
            static void CALLBACK OnStatus(HINTERNET, DWORD_PTR context, DWORD status, void* information, DWORD size) {
                auto& self = *reinterpret_cast<AsyncRequest*>(context);
                {
                    std::lock_guard lock(self.mutex);
                    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
                        self.closed = true;
                    }
                    else if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
                        self.failed = true;
                        self.errorCode = static_cast<WINHTTP_ASYNC_RESULT*>(information)->dwError;
                    }
                    else {
                        self.event = status;
                        if (status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
                            self.bytesRead = size;
                        }
                    }
                }
                self.changed.notify_all();
            }

            HINTERNET request;
            std::mutex mutex;
            std::condition_variable changed;
            DWORD event { 0 };
            DWORD bytesRead { 0 };
            DWORD errorCode { 0 };
            bool failed { false };
            bool timedOut { false };
            bool registered { false };
            bool closed { false };
        };

        explicit HttpClient(T&& stream)
        : ostream(std::move(stream)) {}

        static std::wstring Wide(const std::string& value) {
            if (value.empty()) {
                return {};
            }
            const int length = MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
            if (!length) {
                return {};
            }
            std::wstring result(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(
                CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), result.data(), length);
            return result;
        }

        static std::string Utf8(const std::wstring& value) {
            if (value.empty()) {
                return {};
            }
            const int length = WideCharToMultiByte(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
            if (!length) {
                return {};
            }
            std::string result(static_cast<size_t>(length), '\0');
            WideCharToMultiByte(
                CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), length, nullptr, nullptr);
            return result;
        }

        bool IsCanceled() const { return this->localStop.stop_requested() || this->cancellation.stop_requested(); }

        bool SetRemainingTimeout(HINTERNET handle, std::chrono::steady_clock::time_point deadline) const {
            if (this->timeoutMs <= 0) {
                return true;
            }
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now())
                    .count();
            if (remaining <= 0) {
                return false;
            }
            const int timeout = static_cast<int>((std::min) (remaining, static_cast<decltype(remaining)>(INT_MAX)));
            return WinHttpSetTimeouts(handle, timeout, timeout, timeout, timeout) != FALSE;
        }

        /* WinHTTP returns all headers as UTF-16 lines; callers use UTF-8. */
        void ReadHeaders(HINTERNET request) {
            DWORD size = 0;
            WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                WINHTTP_NO_HEADER_INDEX);
            if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !size) {
                return;
            }
            std::vector<wchar_t> buffer(size / sizeof(wchar_t));
            if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                    buffer.data(), &size, WINHTTP_NO_HEADER_INDEX))
            {
                return;
            }
            std::wstring raw(buffer.data());
            size_t start = 0;
            while (start < raw.size()) {
                const auto end = raw.find(L"\r\n", start);
                const auto line = raw.substr(start, end == std::wstring::npos ? end : end - start);
                const auto separator = line.find(L':');
                if (separator != std::wstring::npos) {
                    const auto first = line.find_first_not_of(L" \t", separator + 1);
                    auto key = Utf8(line.substr(0, separator));
                    auto value = first == std::wstring::npos ? std::string {} : Utf8(line.substr(first));
                    this->responseHeaders[key] = value;
                    if (this->headersCb) {
                        this->headersCb(key, value);
                    }
                }
                if (end == std::wstring::npos) {
                    break;
                }
                start = end + 2;
            }
        }

        bool Transfer(int& status) {
            const auto deadline = this->timeoutMs > 0
                ? std::chrono::steady_clock::now() + std::chrono::milliseconds(this->timeoutMs)
                : (std::chrono::steady_clock::time_point::max) ();
            if (this->IsCanceled()) {
                return false;
            }

            /* split the URL once so WinHTTP receives the host, port, and path. */
            const auto urlWide = Wide(this->url);
            URL_COMPONENTS parts {};
            parts.dwStructSize = sizeof(parts);
            parts.dwHostNameLength = static_cast<DWORD>(-1);
            parts.dwUrlPathLength = static_cast<DWORD>(-1);
            parts.dwExtraInfoLength = static_cast<DWORD>(-1);
            if (urlWide.empty() || !WinHttpCrackUrl(urlWide.c_str(), 0, 0, &parts) ||
                (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS))
            {
                return false;
            }
            const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
            std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
            if (path.empty()) {
                path = L"/";
            }
            if (parts.dwExtraInfoLength) {
                path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
            }
            const auto agent = Wide(this->userAgent.empty() ? musik::cube::userAgent() : this->userAgent);

            /* open the session and request before applying headers and body. */
            Handle session(WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC));
            if (!session.Get() || !SetRemainingTimeout(session.Get(), deadline)) {
                return false;
            }
            Handle connection(WinHttpConnect(session.Get(), host.c_str(), parts.nPort, 0));
            if (!connection.Get()) {
                return false;
            }
            AsyncRequest request(WinHttpOpenRequest(connection.Get(),
                this->method == HttpMethod::Post ? L"POST" : L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES, parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
            if (!request.Get() || !request.Initialize()) {
                return false;
            }
            std::stop_callback externalStop(this->cancellation, [&] {
                request.Wake();
            });
            std::stop_callback localStopCallback(this->localStop.get_token(), [&] {
                request.Wake();
            });
            if (parts.nScheme == INTERNET_SCHEME_HTTPS && (!this->verifyTls || this->allowUntrustedAuthority)) {
                /* preserve the current curl client's certificate policy. */
                DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                    SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                if (!this->verifyTls) {
                    flags |= SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
                }
                if (!WinHttpSetOption(request.Get(), WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags))) {
                    return false;
                }
            }
            if (!this->followRedirects) {
                DWORD disabled = WINHTTP_DISABLE_REDIRECTS;
                if (!WinHttpSetOption(request.Get(), WINHTTP_OPTION_DISABLE_FEATURE, &disabled, sizeof(disabled)))
                {
                    return false;
                }
            }
            std::wstring headers;
            for (const auto& [key, value] : this->requestHeaders) {
                headers += Wide(key + ": " + value + "\r\n");
            }
            if (this->postBody.size() > MAXDWORD || !SetRemainingTimeout(request.Get(), deadline)) {
                return false;
            }
            auto* body = this->method == HttpMethod::Post && !this->postBody.empty()
                ? const_cast<char*>(this->postBody.data())
                : nullptr;
            const DWORD length = body ? static_cast<DWORD>(this->postBody.size()) : 0;

            const auto fail = [&] {
                const DWORD code = request.TimedOut() ? ERROR_WINHTTP_TIMEOUT : request.ErrorCode();
                if (this->IsCanceled()) {
                    this->error = HttpError::Cancelled;
                }
                else if (code == ERROR_WINHTTP_SECURE_FAILURE || code == ERROR_WINHTTP_SECURE_CERT_DATE_INVALID ||
                    code == ERROR_WINHTTP_SECURE_CERT_CN_INVALID || code == ERROR_WINHTTP_SECURE_INVALID_CA)
                {
                    this->error = HttpError::Certificate;
                }
                else if (code == ERROR_WINHTTP_TIMEOUT || std::chrono::steady_clock::now() >= deadline) {
                    this->error = HttpError::Timeout;
                }
                else if (code == ERROR_WINHTTP_NAME_NOT_RESOLVED || code == ERROR_WINHTTP_CANNOT_CONNECT ||
                    code == ERROR_WINHTTP_CONNECTION_ERROR)
                {
                    this->error = HttpError::Connection;
                }
                else {
                    this->error = HttpError::Other;
                }
                this->errorMessage = "Windows HTTP error " + std::to_string(code);
                return false;
            };
            const auto phaseDeadline = [&](long milliseconds) {
                return milliseconds > 0
                    ? (std::min) (deadline, std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds))
                    : deadline;
            };

            /* report the HTTP status even when the server returns an error. */
            request.Prepare();
            if (!request.Start(WinHttpSendRequest(request.Get(), headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                    static_cast<DWORD>(headers.size()), body, length, length, reinterpret_cast<DWORD_PTR>(&request))) ||
                !request.Wait(WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE, phaseDeadline(this->connectTimeoutMs),
                    this->cancellation, this->localStop.get_token()))
            {
                return fail();
            }
            request.Prepare();
            if (!request.Start(WinHttpReceiveResponse(request.Get(), nullptr)) ||
                !request.Wait(WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE, phaseDeadline(this->readTimeoutMs),
                    this->cancellation, this->localStop.get_token()))
            {
                return fail();
            }
            DWORD code = 0;
            DWORD size = sizeof(code);
            if (!WinHttpQueryHeaders(request.Get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX, &code, &size, WINHTTP_NO_HEADER_INDEX))
            {
                return false;
            }
            status = static_cast<int>(code);
            if (this->responseCb) {
                this->responseCb();
            }
            this->ReadHeaders(request.Get());
            if (status >= 400) {
                this->error = HttpError::Other;
                this->errorMessage = "HTTP error";
                return false;
            }

            /* deliver each received chunk before asking for the next one. */
            char buffer[16384];
            while (!this->IsCanceled() && SetRemainingTimeout(request.Get(), deadline)) {
                request.Prepare();
                if (!request.Start(WinHttpReadData(request.Get(), buffer, sizeof(buffer), nullptr)) ||
                    !request.Wait(WINHTTP_CALLBACK_STATUS_READ_COMPLETE, phaseDeadline(this->readTimeoutMs),
                        this->cancellation, this->localStop.get_token()))
                {
                    return fail();
                }
                const DWORD read = request.BytesRead();
                if (!read) {
                    return true;
                }
                this->ostream.write(buffer, read);
                if (!this->ostream) {
                    return false;
                }
            }
            return false;
        }

        void Execute(const Callback& callback) {
            int status = 0;
            const bool success = this->Transfer(status);
            if (this->IsCanceled() && this->canceledCb) {
                this->canceledCb(this);
            }
            if (callback) {
                callback(this, status, success);
            }
            {
                std::lock_guard lock(this->mutex);
                this->finished = true;
            }
            this->done.notify_all();
        }

        T ostream;
        std::string url, postBody, userAgent;
        HttpHeaders requestHeaders, responseHeaders;
        HeaderCallback headersCb;
        ResponseCallback responseCb;
        std::stop_token cancellation;
        CanceledCallback canceledCb;
        std::stop_source localStop;
        long timeoutMs { DefaultTimeoutMs };
        long connectTimeoutMs { 0 };
        long readTimeoutMs { 0 };
        bool verifyTls { false };
        bool allowUntrustedAuthority { false };
        bool followRedirects { true };
        HttpError error { HttpError::None };
        std::string errorMessage;
        Thread mode { Thread::Background };
        HttpMethod method { HttpMethod::Get };
        std::mutex mutex;
        std::condition_variable done;
        bool started { false };
        bool finished { false };
    };
} } }
