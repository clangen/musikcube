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

#include <curl/curl.h>
#include <thread>
#include <mutex>
#include <core/runtime/IMessageTarget.h>

namespace musik { namespace cube {
    class UpdateCheck : private musik::core::runtime::IMessageTarget {
        public:
            /* args = updateRequired, version, url */
            using Callback = std::function<void(bool, std::string, std::string)>;

            static void ShowUpgradeAvailableOverlay(
                const std::string& version, const std::string& url, bool silent = true);

            static void ShowNoUpgradeFoundOverlay();

            UpdateCheck();
            bool Run(Callback callback);
            void Cancel();

        private:
            void Reset();

            static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
            static int CurlTransferCallback(void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow);
            virtual void ProcessMessage(musik::core::runtime::IMessage &message);

            std::recursive_mutex mutex;
            std::shared_ptr<std::thread> thread;

            Callback callback;
            std::string result, latestVersion, updateUrl;
            bool cancel;
            CURL* curl;
    };

} }