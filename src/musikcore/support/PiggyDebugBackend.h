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

/** @file PiggyDebugBackend.h
 *  @brief Debug backend that forwards log output to the Piggy service.
 *  @details Implements musik::debug::IBackend so that log messages are relayed
 *      over the Piggy WebSocket connection. */

#include <memory>
#include <musikcore/debug.h>
#include <musikcore/net/PiggyWebSocketClient.h>

namespace musik {

    /** @brief Forwards application log output to the Piggy service.
     *  @details Each log call wraps the message and sends it through the given
     *      PiggyWebSocketClient. */
    class PiggyDebugBackend : public musik::debug::IBackend {
        public:
            using Client = std::shared_ptr<musik::core::net::PiggyWebSocketClient>; /**< Client alias. */

            /** @brief Creates a debug backend bound to a Piggy client.
             *  @param client The client used to send log messages. */
            PiggyDebugBackend(Client client);
            virtual ~PiggyDebugBackend() override;
            /** @brief Forwards a verbose-level log message.
             *  @param tag The logging tag.
             *  @param string The log text. */
            virtual void verbose(const std::string& tag, const std::string& string) override;
            /** @brief Forwards an info-level log message.
             *  @param tag The logging tag.
             *  @param string The log text. */
            virtual void info(const std::string& tag, const std::string& string) override;
            /** @brief Forwards a warning-level log message.
             *  @param tag The logging tag.
             *  @param string The log text. */
            virtual void warning(const std::string& tag, const std::string& string) override;
            /** @brief Forwards an error-level log message.
             *  @param tag The logging tag.
             *  @param string The log text. */
            virtual void error(const std::string& tag, const std::string& string) override;

        private:
            Client client; /**< Piggy client used to relay messages. */
    };

}
