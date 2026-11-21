#pragma once

#include <atomic>
#include <curl/curl.h>

namespace musik { namespace core {
    inline void ConfigureOnlineTransfer(CURL* curl, const std::atomic_bool& cancelled) {
        constexpr long timeoutSeconds = 15;
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
            +[](void* data, curl_off_t, curl_off_t, curl_off_t, curl_off_t) -> int {
                return static_cast<const std::atomic_bool*>(data)->load() ? 1 : 0;
            });
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &cancelled);
    }
} }
