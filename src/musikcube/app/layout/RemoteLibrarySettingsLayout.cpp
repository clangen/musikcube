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

#include "RemoteLibrarySettingsLayout.h"

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/support/PreferenceKeys.h>

#include <algorithm>
#include <vector>

using namespace musik;
using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

static inline int longestStringLength(const std::vector<std::string>&& keys) {
    int max = 0;
    for (auto& str: keys) {
        size_t len = u8cols(_TSTR(str));
        max = len > max ? len : max;
    }
    return max;
}

RemoteLibrarySettingsLayout::RemoteLibrarySettingsLayout()
: LayoutBase() {
    this->prefs = Preferences::ForComponent(core::prefs::components::Settings);
    this->SetFocusMode(FocusModeTerminating);
    this->InitializeWindows();
}

RemoteLibrarySettingsLayout::~RemoteLibrarySettingsLayout() {
}

void RemoteLibrarySettingsLayout::OnLayout() {
    const int labelWidth = longestStringLength({
        "settings_library_type_remote_hostname",
        "settings_library_type_remote_wss_port",
        "settings_library_type_remote_http_port",
        "settings_library_type_remote_password"
     });

    int cx = this->GetWidth();
    int inputWidth = std::min((int) 32, (int) (cx - labelWidth - 1));
    int y = 0;
    this->hostLabel->MoveAndResize(0, y, labelWidth, 1);
    this->hostInput->MoveAndResize(labelWidth + 1, y++, inputWidth, 1);
    this->wssPortLabel->MoveAndResize(0, y, labelWidth, 1);
    this->wssPortInput->MoveAndResize(labelWidth + 1, y++, 5, 1);
    this->httpPortLabel->MoveAndResize(0, y, labelWidth, 1);
    this->httpPortInput->MoveAndResize(labelWidth + 1, y++, 5, 1);
    this->pwLabel->MoveAndResize(0, y, labelWidth, 1);
    this->pwInput->MoveAndResize(labelWidth + 1, y++, inputWidth, 1);
    //this->ipv6Cb->MoveAndResize(labelWidth - 3, y++, cx - labelWidth, 1);
}

void RemoteLibrarySettingsLayout::InitializeWindows() {
    this->SetFrameVisible(false);

    this->hostLabel.reset(new TextLabel());
    this->hostLabel->SetText(_TSTR("settings_library_type_remote_hostname"), text::AlignRight);
    this->hostInput.reset(new TextInput(TextInput::StyleLine));

    this->wssPortLabel.reset(new TextLabel());
    this->wssPortLabel->SetText(_TSTR("settings_library_type_remote_wss_port"), text::AlignRight);
    this->wssPortInput.reset(new TextInput(TextInput::StyleLine));

    this->httpPortLabel.reset(new TextLabel());
    this->httpPortLabel->SetText(_TSTR("settings_library_type_remote_http_port"), text::AlignRight);
    this->httpPortInput.reset(new TextInput(TextInput::StyleLine));

    this->pwLabel.reset(new TextLabel());
    this->pwLabel->SetText(_TSTR("settings_library_type_remote_password"), text::AlignRight);
    this->pwInput.reset(new TextInput(TextInput::StyleLine));
    this->pwInput->SetInputMode(IInput::InputPassword);

    //this->ipv6Cb.reset(new Checkbox());
    //this->ipv6Cb->SetText(_TSTR("settings_server_use_ipv6"));

    this->AddWindow(this->hostLabel);
    this->AddWindow(this->hostInput);
    this->AddWindow(this->hostLabel);
    this->AddWindow(this->hostInput);
    this->AddWindow(this->wssPortLabel);
    this->AddWindow(this->wssPortInput);
    this->AddWindow(this->httpPortLabel);
    this->AddWindow(this->httpPortInput);
    this->AddWindow(this->pwLabel);
    this->AddWindow(this->pwInput);
    //this->AddWindow(this->ipv6Cb);

    int order = 0;
    this->hostInput->SetFocusOrder(order++);
    this->wssPortInput->SetFocusOrder(order++);
    this->httpPortInput->SetFocusOrder(order++);
    this->pwInput->SetFocusOrder(order++);
    //this->ipv6Cb->SetFocusOrder(order++);
}

void RemoteLibrarySettingsLayout::LoadPreferences() {
    auto host = prefs->GetString(core::prefs::keys::RemoteLibraryHostname, "127.0.0.1");
    auto wssPort = (short) prefs->GetInt(core::prefs::keys::RemoteLibraryWssPort, 7905);
    auto httpPort = (short) prefs->GetInt(core::prefs::keys::RemoteLibraryHttpPort, 7906);
    auto password = prefs->GetString(core::prefs::keys::RemoteLibraryPassword, "");
    //auto ipv6 = prefs->GetBool(core::prefs::keys::RemoteLibraryIpv6, false);
    this->hostInput->SetText(host);
    this->wssPortInput->SetText(std::to_string(wssPort));
    this->httpPortInput->SetText(std::to_string(httpPort));
    this->pwInput->SetText(password);
    //this->ipv6Cb->SetChecked(ipv6);
}

void RemoteLibrarySettingsLayout::SavePreferences() {
    auto host = this->hostInput->GetText();
    auto wssPort = std::stoi(this->wssPortInput->GetText());
    auto httpPort = std::stoi(this->httpPortInput->GetText());
    auto password = this->pwInput->GetText();
    //auto ipv6 = this->ipv6Cb->IsChecked();

    if (wssPort > 65535 || wssPort < 0) { wssPort = 7905; }
    if (httpPort > 65535 || httpPort < 0) { httpPort = 7905; }

    prefs->SetString(core::prefs::keys::RemoteLibraryHostname, host.size() ? host.c_str() : "127.0.0.1");
    prefs->SetInt(core::prefs::keys::RemoteLibraryWssPort, wssPort);
    prefs->SetInt(core::prefs::keys::RemoteLibraryHttpPort, httpPort);
    prefs->SetString(core::prefs::keys::RemoteLibraryPassword, password.c_str());
    //prefs->SetBool(core::prefs::keys::RemoteLibraryIpv6, ipv6);

    auto library = LibraryFactory::Instance().DefaultRemoteLibrary();
    auto remoteLibrary = std::dynamic_pointer_cast<RemoteLibrary>(library);
    if (remoteLibrary) {
        remoteLibrary->ReloadConnectionFromPreferences();
    }
}
