//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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
#include <app/layout/LibraryNotConnectedLayout.h>
#include <musikcore/i18n/Locale.h>
#include <musikcore/library/RemoteLibrary.h>
#include <cursespp/App.h>
#include <cursespp/Screen.h>
#include <cursespp/ToastOverlay.h>
#include <cursespp/Colors.h>
#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <unordered_map>

using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::net;
using namespace cursespp;

using MasterLibraryPtr = LibraryNotConnectedLayout::MasterLibraryPtr;

static inline std::string resolveErrorMessage(MasterLibraryPtr library) {
    static const std::map<WebSocketClient::ConnectionError, std::string> kStateToErrorString = {
        { WebSocketClient::ConnectionError::ClosedByServer, "library_error_closed_by_server" },
        { WebSocketClient::ConnectionError::ConnectionFailed, "library_error_connection_failed" },
        { WebSocketClient::ConnectionError::InvalidPassword, "library_error_invalid_password" },
    };

    auto remoteLibrary = dynamic_cast<const RemoteLibrary*>(library->Wrapped().get());
    if (remoteLibrary) {
        auto error = remoteLibrary->WebSocketClient().LastConnectionError();
        auto it = kStateToErrorString.find(error);
        if (it != kStateToErrorString.end()) {
            return _TSTR(it->second);
        }
        return _TSTR("library_error_connection_failed");
    }

    if (library->GetConnectionState() == ILibrary::ConnectionState::AuthenticationFailure) {
        return _TSTR("library_error_invalid_password");
    }

    return _TSTR("library_error_unknown");
}

static inline std::string resolveMessageText(MasterLibraryPtr library) {
    auto remoteLibrary = dynamic_cast<const RemoteLibrary*>(library->Wrapped().get());
    if (remoteLibrary) {
        auto host = remoteLibrary->WebSocketClient().Uri();
        if (host.find("ws://") == 0) { host = host.substr(5); }
        if (host.size()) {
            return u8fmt(_TSTR("library_not_connected_with_hostname"), host.c_str());
        }
    }
    return _TSTR("library_not_connected");
}

LibraryNotConnectedLayout::LibraryNotConnectedLayout(MasterLibraryPtr library)
: LayoutBase()
, library(library) {
    this->library->ConnectionStateChanged.connect(this, &LibraryNotConnectedLayout::OnLibraryStateChanged);
    this->messageText = std::make_shared<TextLabel>("", text::AlignCenter);
    this->messageText->SetContentColor(Color::TextError);
    this->errorText = std::make_shared<TextLabel>("", text::AlignCenter);
    this->errorText->SetContentColor(Color::TextDisabled);
    this->helpText = std::make_shared<TextLabel>(_TSTR("library_configuration_in_settings"), text::AlignCenter);
    this->AddWindow(this->messageText);
    this->AddWindow(this->errorText);
    this->AddWindow(this->helpText);
    this->UpdateErrorText();
}

void LibraryNotConnectedLayout::OnLayout() {
    LayoutBase::OnLayout();
    int cx = this->GetContentWidth();
    int cy = this->GetContentHeight();
    this->messageText->MoveAndResize(1, (cy / 2) - 2, cx - 2, 1);
    this->errorText->MoveAndResize(1, (cy / 2) - 1, cx - 2, 1);
    this->helpText->MoveAndResize(1, (cy / 2) + 1, cx - 2, 1);
}

void LibraryNotConnectedLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);
}

bool LibraryNotConnectedLayout::KeyPress(const std::string& kn) {
    return LayoutBase::KeyPress(kn);
}

void LibraryNotConnectedLayout::OnLibraryStateChanged(ILibrary::ConnectionState state) {
    this->UpdateErrorText();
}

void LibraryNotConnectedLayout::UpdateErrorText() {
    auto format = this->library->GetConnectionState() == ILibrary::ConnectionState::Connecting
        ? _TSTR("library_error_format_retrying") : _TSTR("library_error_format");
    auto error = u8fmt(format, resolveErrorMessage(this->library).c_str());
    this->errorText->SetText(error);
    this->messageText->SetText(resolveMessageText(this->library));
}

void LibraryNotConnectedLayout::SetShortcutsWindow(cursespp::ShortcutsWindow* shortcuts) {
    this->shortcuts = shortcuts;

    if (this->shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), _TSTR("shortcuts_console"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), _TSTR("shortcuts_library"));
        shortcuts->AddShortcut(App::Instance().GetQuitKey(), _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateConsole, key)) {
                this->Broadcast(message::JumpToConsole);
            }
            if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
                this->Broadcast(message::JumpToSettings);
            }
            else if (key == App::Instance().GetQuitKey()) {
                App::Instance().Quit();
            }
            this->KeyPress(key);
        });

        shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibrary));
    }
}
