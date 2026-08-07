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

/**
 * @file UpdateCheck.h
 * @brief Background check for application updates.
 * @details Queries the update server on a background thread, compares the
 *          latest version with the installed version and either notifies the
 *          user that an upgrade is available or that no upgrade was found.
 */

#include <thread>
#include <mutex>
#include <memory>
#include <musikcore/runtime/IMessageTarget.h>
#include <musikcore/sdk/HttpClient.h>

namespace musik { namespace cube {
    /**
     * @brief Performs a single update check in the background.
     * @details The check runs on a detached thread and marshals the result
     *          back through the message queue. Results are delivered to the
     *          callback supplied to Run().
     */
    class UpdateCheck: private musik::core::runtime::IMessageTarget {
        public:
            /* args = updateRequired, version, url */
            using Callback = std::function<void(bool, std::string, std::string)>;
            using HttpClient = musik::core::sdk::HttpClient<std::stringstream>;

            /**
             * @brief Shows the "upgrade available" overlay.
             * @param version the latest available version
             * @param url the download or release page url
             * @param silent if true, do not show the overlay if it has already
             *        been acknowledged for this version
             */
            static void ShowUpgradeAvailableOverlay(
                const std::string& version, const std::string& url, bool silent = true);

            /**
             * @brief Shows the "no upgrade found" overlay.
             */
            static void ShowNoUpgradeFoundOverlay();

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(UpdateCheck)

            /**
             * @brief Creates a new update check.
             */
            UpdateCheck();
            /**
             * @brief Destroys the check and cancels any in-flight request.
             */
            ~UpdateCheck();

            /**
             * @brief Starts the update check on a background thread.
             * @param callback invoked with (updateRequired, version, url)
             * @return true if the check was started
             */
            bool Run(Callback callback);
            /**
             * @brief Cancels an in-flight check.
             */
            void Cancel();

        private:
            void Reset();

            /* IMessageHandler */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            std::recursive_mutex mutex;     /**< guards the shared result state */
            Callback callback;              /**< invoked when the check completes */
            std::shared_ptr<HttpClient> httpClient; /**< the HTTP client used to query the server */
            std::string result, latestVersion, updateUrl; /**< last check results */
    };

} }