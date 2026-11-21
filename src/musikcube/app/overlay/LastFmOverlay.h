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

#include <cursespp/DialogOverlay.h>
#include <musikcore/context.hpp>
#include <app/util/AsyncCallbackScope.h>

namespace musik { namespace cube {
    /* Runs the browser authorization flow for a borrowed context. Neither copyable nor movable. */
    class LastFmOverlay : public cursespp::DialogOverlay {
        public:
            enum class State {
                Unregistered = 0,
                ObtainingToken = 1,
                WaitingForUser = 2,
                RegisteringSession = 3,
                Registered = 4,
                LinkError = 5,
                RegisterError = 6
            };

            static void Start(musik::core::api::Context& musikcore);

        private:
            explicit LastFmOverlay(musik::core::api::Context& musikcore);

            void LoadDefaultState();
            void UpdateMessage();
            void UpdateButtons();

            void SetState(State state);

            void GetLinkToken();
            void CreateSession();

            musik::core::api::Context& musikcore;
            AsyncCallbackScope work;
            State state { State::Unregistered };
            std::string linkToken;
            std::string linkUrl;
    };
} }
