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

#include <stdafx.h>
#include "LastFmOverlay.h"
#include <app/util/Messages.h>
#include <core/support/LastFm.h>
#include <core/support/Common.h>
#include <cursespp/App.h>
#include <map>
#include <vector>

using namespace musik::cube;
using namespace musik::core;
using namespace musik;
using namespace cursespp;

static std::map<LastFmOverlay::State, std::string> stateToText = {
    { LastFmOverlay::State::Unregistered, "settings_last_fm_dialog_message_unregistered" },
    { LastFmOverlay::State::ObtainingToken, "settings_last_fm_dialog_message_obtaining_token" },
    { LastFmOverlay::State::WaitingForUser, "settings_last_fm_dialog_message_waiting_for_user" },
    { LastFmOverlay::State::RegisteringSession, "settings_last_fm_dialog_message_registering_session" },
    { LastFmOverlay::State::Registered, "settings_last_fm_dialog_message_registered" },
    { LastFmOverlay::State::LinkError, "settings_last_fm_dialog_message_link_error" },
    { LastFmOverlay::State::RegisterError, "settings_last_fm_dialog_message_register_error" }
};

void LastFmOverlay::Start() {
    std::shared_ptr<LastFmOverlay> overlay(new LastFmOverlay());
    App::Overlays().Push(overlay);
}

static lastfm::Session session() {
    return lastfm::LoadSession();
}

LastFmOverlay::LastFmOverlay()
: DialogOverlay() {
    this->SetTitle(_TSTR("settings_last_fm_dialog_title"));
    this->SetAutoDismiss(false);
    this->LoadDefaultState();
}

LastFmOverlay::~LastFmOverlay() {

}

void LastFmOverlay::LoadDefaultState() {
    this->SetState(session().valid ? State::Registered : State::Unregistered);
}

void LastFmOverlay::GetLinkToken() {
    this->SetState(State::ObtainingToken);
    lastfm::CreateAccountLinkToken([this](std::string token) {
        this->linkToken = token;
        if (token.size()) {
            this->PostState(State::WaitingForUser);
        }
        else {
            this->PostState(State::LinkError);
        }
    });
}

void LastFmOverlay::CreateSession() {
    this->SetState(State::RegisteringSession);
    lastfm::CreateSession(this->linkToken, [this](lastfm::Session session) {
        if (session.valid) {
            lastfm::SaveSession(session);
            this->PostState(State::Registered);
        }
        else {
            this->PostState(State::RegisterError);
        }
    });
}

void LastFmOverlay::PostState(State state) {
    this->Post(message::SetLastFmState, (int64_t) state);
}

void LastFmOverlay::SetState(State state) {
    this->state = state;
    this->UpdateMessage();
    this->UpdateButtons();
}

void LastFmOverlay::UpdateMessage() {
    std::string message = _TSTR(stateToText[state]);

    switch (this->state) {
        case State::Registered: {
            auto session = lastfm::LoadSession();
            core::ReplaceAll(message, "{{username}}", session.username);
            break;
        }

        case State::WaitingForUser:
        case State::RegisterError: {
            std::string url = lastfm::CreateAccountLinkUrl(this->linkToken);
            core::ReplaceAll(message, "{{link}}", url);
            break;
        }

        default:
            break;
    }

    this->SetMessage(message);
}

void LastFmOverlay::UpdateButtons() {
    this->ClearButtons();

    switch (this->state) {
        case State::Unregistered: {
            this->AddButton(
                "KEY_ENTER", "ENTER", _TSTR("button_start"),
                [this](std::string key) {
                    this->GetLinkToken();
                });

            this->AddButton(
                "^[", "ESC", _TSTR("button_cancel"),
                [this](std::string key) {
                    this->Dismiss();
                });
            break;
        }

        case State::Registered: {
            this->AddButton(
                "u", "u", _TSTR("button_unregister"),
                [this](std::string key) {
                    lastfm::ClearSession();
                    this->LoadDefaultState();
                });

            this->AddButton(
                "KEY_ENTER", "ENTER", _TSTR("button_close"),
                [this](std::string key) {
                    this->Dismiss();
                });
            break;
        }

        case State::WaitingForUser:
        case State::RegisterError: {
            this->AddButton(
                "o", "o", _TSTR("button_open_url"),
                [this](std::string key) {
                    core::OpenFile(lastfm::CreateAccountLinkUrl(this->linkToken));
                });


            std::string continueText = _TSTR(
                (state == State::WaitingForUser)
                    ? "button_continue"
                    : "button_retry");

            this->AddButton(
                "KEY_ENTER", "ENTER", continueText,
                [this](std::string key) {
                    this->CreateSession();
                });

            this->AddButton(
                "^[", "ESC", _TSTR("button_cancel"),
                [this](std::string key) {
                    this->Dismiss();
                });
            break;
        }

        case State::LinkError: {
            this->AddButton(
                "KEY_ENTER", "ENTER", _TSTR("button_ok"),
                [this](std::string key) {
                    this->Dismiss();
                });
            break;
        }

        default:
            break;
    }
}

void LastFmOverlay::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == message::SetLastFmState) {
        this->SetState((State) message.UserData1());
    }
    else {
        DialogOverlay::ProcessMessage(message);
    }
}
