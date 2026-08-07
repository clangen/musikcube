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
 * @file LastFmOverlay.h
 * @brief Overlay that guides the user through Last.fm account linking.
 * @details Shows a dialog that walks the user through obtaining an auth
 *          token, opening the Last.fm website to approve it, and then
 *          registering the session so scrobbling can start.
 */

#include <cursespp/DialogOverlay.h>

namespace musik { namespace cube {
    /* in general we probably shouldn't subclass DialogOverlay because
    callers can mutate the title and body... but... meh. this is easy. */
    /**
     * @brief Last.fm account linking dialog.
     * @details Displays the state machine of the linking process: unregistered,
     *          obtaining token, waiting for the user to approve the link,
     *          registering the session, and finally registered. On errors a
     *          retry can be initiated from the dialog buttons.
     */
    class LastFmOverlay : public cursespp::DialogOverlay {
        public:
            /**
             * @brief The current state of the linking flow.
             */
            enum class State {
                Unregistered = 0,      /**< the account is not linked */
                ObtainingToken = 1,    /**< requesting an auth token */
                WaitingForUser = 2,    /**< waiting for the user to approve */
                RegisteringSession = 3,/**< exchanging the token for a session */
                Registered = 4,        /**< the account is linked */
                LinkError = 5,         /**< failed to obtain a token */
                RegisterError = 6      /**< failed to register the session */
            };

            /**
             * @brief Starts the linking flow if it is not already in
             *        progress, showing the overlay.
             */
            static void Start();

            /**
             * @brief Destroys the overlay.
             */
            virtual ~LastFmOverlay();

            /**
             * @brief Processes runtime messages that drive the state machine.
             * @param message the message to process
             */
            virtual void ProcessMessage(musik::core::runtime::IMessage &message);

        private:
            /**
             * @brief Creates the overlay in the unregistered state.
             */
            LastFmOverlay();

            void LoadDefaultState();
            void UpdateMessage();
            void UpdateButtons();

            void SetState(State state);
            void PostState(State state);

            void GetLinkToken();
            void CreateSession();

            State state{ State::Unregistered }; /**< the current linking state */
            std::string linkToken;              /**< the token returned by Last.fm */
    };
} }
