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

#include <stdafx.h>

#include "RemoteLibrarySettingsLayout.h"

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/support/PreferenceKeys.h>
#include <app/overlay/SettingsOverlays.h>

#include <algorithm>
#include <vector>

using namespace musik;
using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

#define R(x) (x->GetX() + x->GetWidth())

constexpr const char* kArrowSymbol = "> ";

static int getIntFromTextInput(TextInput& input, int defaultValue = -1) {
    try {
        return std::stoi(input.GetText());
    }
    catch (...) {
        return defaultValue;
    }
}

static inline int longestStringLength(const std::vector<std::string>&& keys) {
    int max = 0;
    for (auto& str: keys) {
        int len = narrow_cast<int>(u8cols(_TSTR(str)));
        max = len > max ? len : max;
    }
    return max;
}

RemoteLibrarySettingsLayout::RemoteLibrarySettingsLayout()
: LayoutBase() {
    this->prefs = Preferences::ForComponent(core::prefs::components::Settings);
    this->SetFocusMode(FocusModeTerminating);
    this->InitializeWindows();
    this->LoadPreferences();
}

RemoteLibrarySettingsLayout::~RemoteLibrarySettingsLayout() {
    this->SavePreferences();
}

void RemoteLibrarySettingsLayout::OnLayout() {
    const int labelWidth = longestStringLength({
        "settings_library_type_remote_hostname",
        "settings_library_type_remote_wss_port",
        "settings_library_type_remote_http_port",
        "settings_library_type_remote_password"
     });

    const int checkboxWidth = narrow_cast<int>(
        u8cols(_TSTR("settings_library_type_remote_use_tls")) + 4);

    int const cx = this->GetWidth();
    int const inputWidth = std::min((int) 32, (int) (cx - labelWidth - 1));
    int y = 0;
    this->hostLabel->MoveAndResize(0, y, labelWidth, 1);
    this->hostInput->MoveAndResize(labelWidth + 1, y++, inputWidth, 1);
    this->wssPortLabel->MoveAndResize(0, y, labelWidth, 1);
    this->wssPortInput->MoveAndResize(labelWidth + 1, y, 5, 1);
    this->wssTlsCheckbox->MoveAndResize(R(this->wssPortInput) + 1, y++, checkboxWidth, 1);
    this->httpPortLabel->MoveAndResize(0, y, labelWidth, 1);
    this->httpPortInput->MoveAndResize(labelWidth + 1, y, 5, 1);
    this->httpTlsCheckbox->MoveAndResize(R(this->wssPortInput) + 1, y++, checkboxWidth, 1);
    this->pwLabel->MoveAndResize(0, y, labelWidth, 1);
    this->pwInput->MoveAndResize(labelWidth + 1, y++, inputWidth, 1);

    this->transcoderCheckbox->MoveAndResize(
        0,
        y,
        narrow_cast<int>(u8cols(this->transcoderCheckbox->GetText())) + 4,
        1);

    this->transcoderFormatDropdown->MoveAndResize(
        R(this->transcoderCheckbox) + 1,
        y,
        narrow_cast<int>(u8cols(this->transcoderFormatDropdown->GetText())),
        1);

    this->transcoderBitrateDropdown->MoveAndResize(
        R(this->transcoderFormatDropdown) + 1,
        y,
        narrow_cast<int>(u8cols(this->transcoderBitrateDropdown->GetText())),
        1);
}

void RemoteLibrarySettingsLayout::OnTlsCheckboxChanged(cursespp::Checkbox* cb, bool checked) {
    if (checked) {
        SettingsOverlays::CheckShowTlsWarningDialog();
    }
}

void RemoteLibrarySettingsLayout::InitializeWindows() {
    this->SetFrameVisible(false);

    this->hostLabel = std::make_shared<TextLabel>();
    this->hostLabel->SetText(_TSTR("settings_library_type_remote_hostname"), text::AlignRight);
    this->hostInput = std::make_shared<TextInput>(TextInput::StyleLine);

    this->wssPortLabel = std::make_shared<TextLabel>();
    this->wssPortLabel->SetText(_TSTR("settings_library_type_remote_wss_port"), text::AlignRight);
    this->wssPortInput = std::make_shared<TextInput>(TextInput::StyleLine);

    this->wssTlsCheckbox = std::make_shared<Checkbox>();
    this->wssTlsCheckbox->SetText(_TSTR("settings_library_type_remote_use_tls"));

    this->httpPortLabel = std::make_shared<TextLabel>();
    this->httpPortLabel->SetText(_TSTR("settings_library_type_remote_http_port"), text::AlignRight);
    this->httpPortInput = std::make_shared<TextInput>(TextInput::StyleLine);

    this->httpTlsCheckbox = std::make_shared<Checkbox>();
    this->httpTlsCheckbox->SetText(_TSTR("settings_library_type_remote_use_tls"));

    this->pwLabel = std::make_shared<TextLabel>();
    this->pwLabel->SetText(_TSTR("settings_library_type_remote_password"), text::AlignRight);
    this->pwInput = std::make_shared<TextInput>(TextInput::StyleLine);
    this->pwInput->SetInputMode(IInput::InputPassword);

    this->transcoderCheckbox = std::make_shared<Checkbox>();
    this->transcoderCheckbox->SetText(_TSTR("settings_library_type_remote_library_transcoder_enable"));
    this->transcoderFormatDropdown = std::make_shared<TextLabel>();
    this->transcoderFormatDropdown->Activated.connect(this, &RemoteLibrarySettingsLayout::OnActivateTranscoderFormat);
    this->transcoderBitrateDropdown = std::make_shared<TextLabel>();
    this->transcoderBitrateDropdown->Activated.connect(this, &RemoteLibrarySettingsLayout::OnActivateTranscoderBitrate);

    this->AddWindow(this->hostLabel);
    this->AddWindow(this->hostInput);
    this->AddWindow(this->hostLabel);
    this->AddWindow(this->hostInput);
    this->AddWindow(this->wssPortLabel);
    this->AddWindow(this->wssPortInput);
    this->AddWindow(this->wssTlsCheckbox);
    this->AddWindow(this->httpPortLabel);
    this->AddWindow(this->httpPortInput);
    this->AddWindow(this->httpTlsCheckbox);
    this->AddWindow(this->pwLabel);
    this->AddWindow(this->pwInput);
    this->AddWindow(this->transcoderCheckbox);
    this->AddWindow(this->transcoderFormatDropdown);
    this->AddWindow(this->transcoderBitrateDropdown);

    int order = 0;
    this->hostInput->SetFocusOrder(order++);
    this->wssPortInput->SetFocusOrder(order++);
    this->wssTlsCheckbox->SetFocusOrder(order++);
    this->httpPortInput->SetFocusOrder(order++);
    this->httpTlsCheckbox->SetFocusOrder(order++);
    this->pwInput->SetFocusOrder(order++);
    this->transcoderCheckbox->SetFocusOrder(order++);
    this->transcoderFormatDropdown->SetFocusOrder(order++);
    this->transcoderBitrateDropdown->SetFocusOrder(order++);
}

void RemoteLibrarySettingsLayout::LoadPreferences() {
    this->wssTlsCheckbox->CheckChanged.disconnect(this);
    this->httpTlsCheckbox->CheckChanged.disconnect(this);

    auto host = prefs->GetString(core::prefs::keys::RemoteLibraryHostname, "127.0.0.1");
    auto const wssPort = narrow_cast<unsigned short>(prefs->GetInt(core::prefs::keys::RemoteLibraryWssPort, 7905));
    auto const httpPort = narrow_cast<unsigned short>(prefs->GetInt(core::prefs::keys::RemoteLibraryHttpPort, 7906));
    auto const password = prefs->GetString(core::prefs::keys::RemoteLibraryPassword, "");
    auto const wssTls = prefs->GetBool(core::prefs::keys::RemoteLibraryWssTls, false);
    auto const httpTls = prefs->GetBool(core::prefs::keys::RemoteLibraryHttpTls, false);
    this->hostInput->SetText(host);
    this->wssPortInput->SetText(std::to_string(wssPort));
    this->wssTlsCheckbox->SetChecked(wssTls);
    this->httpPortInput->SetText(std::to_string(httpPort));
    this->httpTlsCheckbox->SetChecked(httpTls);
    this->pwInput->SetText(password);

    this->transcoderCheckbox->SetChecked(prefs->GetBool(core::prefs::keys::RemoteLibraryTranscoderEnabled, false));

    const std::string transcodeFormat = prefs->GetString(core::prefs::keys::RemoteLibraryTranscoderFormat, "ogg");
    this->transcoderFormatDropdown->SetText(
        kArrowSymbol +
        u8fmt(_TSTR(
            "settings_library_type_remote_library_transcoder_format"),
            transcodeFormat.c_str()),
        text::AlignRight);

    const int transcodeBitrate = prefs->GetInt(core::prefs::keys::RemoteLibraryTranscoderBitrate, 192);
    this->transcoderBitrateDropdown->SetText(
        kArrowSymbol +
        u8fmt(_TSTR(
            "settings_library_type_remote_library_transcoder_bitrate"),
            transcodeBitrate),
        text::AlignRight);

    this->wssTlsCheckbox->CheckChanged.connect(this, &RemoteLibrarySettingsLayout::OnTlsCheckboxChanged);
    this->httpTlsCheckbox->CheckChanged.connect(this, &RemoteLibrarySettingsLayout::OnTlsCheckboxChanged);
}

void RemoteLibrarySettingsLayout::SyncPreferencesAndLayout() {
    this->SavePreferences();
    this->LoadPreferences();
    this->Layout();
}

void RemoteLibrarySettingsLayout::SavePreferences() {
    auto host = this->hostInput->GetText();
    auto wssPort = getIntFromTextInput(*this->wssPortInput, 7905);
    auto httpPort = getIntFromTextInput(*this->httpPortInput, 7906);
    auto password = this->pwInput->GetText();
    auto const wssTls = this->wssTlsCheckbox->IsChecked();
    auto const httpTls = this->httpTlsCheckbox->IsChecked();
    auto const transcoderEnabled = this->transcoderCheckbox->IsChecked();

    if (wssPort > 65535 || wssPort < 0) { wssPort = 7905; }
    if (httpPort > 65535 || httpPort < 0) { httpPort = 7905; }

    prefs->SetString(core::prefs::keys::RemoteLibraryHostname, host.size() ? host.c_str() : "127.0.0.1");
    prefs->SetInt(core::prefs::keys::RemoteLibraryWssPort, wssPort);
    prefs->SetInt(core::prefs::keys::RemoteLibraryHttpPort, httpPort);
    prefs->SetString(core::prefs::keys::RemoteLibraryPassword, password.c_str());
    prefs->SetBool(core::prefs::keys::RemoteLibraryWssTls, wssTls);
    prefs->SetBool(core::prefs::keys::RemoteLibraryHttpTls, httpTls);
    prefs->SetBool(core::prefs::keys::RemoteLibraryTranscoderEnabled, transcoderEnabled);

    /* note: transcoder format and bitrate settings are saved via SettingsOverlays calls */

    auto library = LibraryFactory::Instance().DefaultRemoteLibrary();
    auto remoteLibrary = std::dynamic_pointer_cast<RemoteLibrary>(library);
    if (remoteLibrary) {
        remoteLibrary->ReloadConnectionFromPreferences();
    }
}

void RemoteLibrarySettingsLayout::OnActivateTranscoderFormat(cursespp::TextLabel* tl) {
    SettingsOverlays::ShowTranscoderFormatOverlay([this]() {
        this->SyncPreferencesAndLayout();
    });
}

void RemoteLibrarySettingsLayout::OnActivateTranscoderBitrate(cursespp::TextLabel* tl) {
    SettingsOverlays::ShowTranscoderBitrateOverlay([this]() {
        this->SyncPreferencesAndLayout();
    });
}
