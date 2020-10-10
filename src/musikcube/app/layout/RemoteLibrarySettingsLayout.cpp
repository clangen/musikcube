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

#include "RemoteLibrarySettingsLayout.h"

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <core/support/PreferenceKeys.h>

#include <algorithm>
#include <vector>

using namespace musik;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

static inline size_t longestStringLength(const std::vector<std::string>&& keys) {
    size_t max = 0;
    for (auto& str: keys) {
        size_t len = u8cols(_TSTR(str));
        max = len > max ? len : max;
    }
    return max;
}

RemoteLibrarySettingsLayout::RemoteLibrarySettingsLayout(musik::core::ILibraryPtr library)
: LayoutBase()
, library(library) {
    this->prefs = Preferences::ForComponent(core::prefs::components::Settings);
    this->SetFocusMode(FocusModeTerminating);
    this->InitializeWindows();
}

RemoteLibrarySettingsLayout::~RemoteLibrarySettingsLayout() {
}

void RemoteLibrarySettingsLayout::OnLayout() {
    size_t labelWidth = longestStringLength({
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
    ++y;
    this->ipv6Cb->MoveAndResize(0, y++, cx, 1);
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

    this->ipv6Cb.reset(new Checkbox());
    this->ipv6Cb->SetText(_TSTR("settings_server_use_ipv6"));

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
    this->AddWindow(this->ipv6Cb);

    int order = 0;
    this->hostInput->SetFocusOrder(order++);
    this->wssPortInput->SetFocusOrder(order++);
    this->httpPortInput->SetFocusOrder(order++);
    this->pwInput->SetFocusOrder(order++);
    this->ipv6Cb->SetFocusOrder(order++);
}

void RemoteLibrarySettingsLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);
    if (visible) {
        this->LoadPreferences();
    }
    else {
        this->SavePreferences();
    }
}

void RemoteLibrarySettingsLayout::LoadPreferences() {
}

void RemoteLibrarySettingsLayout::SavePreferences() {
}
