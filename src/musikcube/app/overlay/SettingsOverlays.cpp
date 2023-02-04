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

#include "SettingsOverlays.h"

#include <musikcore/i18n/Locale.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/sdk/String.h>
#include <musikcore/library/ILibrary.h>

#include <musikcube/app/util/PreferenceKeys.h>
#include <musikcube/app/util/Hotkeys.h>

#include <cursespp/App.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>

#include <chrono>

using namespace musik;
using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace musik::cube::SettingsOverlays;
using namespace cursespp;
using namespace std::chrono;

using Callback = std::function<void()>;

static const std::string kTlsInfoUrl = "https://github.com/clangen/musikcube/wiki/ssl-server-setup";
constexpr int64_t kTlsWarningCooldownMs = 20000;
int64_t lastTlsWarningTime = 0;

static const std::vector<std::string> kTranscoderFormats = { "mp3", "opus", "ogg", "flac", "aac" };
static const std::string kDefaultTranscoderFormat = "ogg";
constexpr int kDefaultTranscoderFormatOverlayWidth = 24;

static const std::vector<int> kTranscoderBitrates = { 64, 96, 128, 192, 256, 320 };
constexpr int kDefaultTranscoderBitrate = 192;
constexpr int kDefaultTranscoderBitrateOverlayWidth = 24;

static void showNeedsRestart(Callback cb = Callback()) {
    auto dialog = std::make_shared<DialogOverlay>();

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(_TSTR("settings_needs_restart"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"), [cb](std::string key) {
            if (cb) {
                cb();
            }
        });

    App::Overlays().Push(dialog);
}

static std::vector<std::string> allLocales;

void musik::cube::SettingsOverlays::ShowLocaleOverlay(std::function<void()> callback) {
    auto& locale = i18n::Locale::Instance();

    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    std::string currentLocale = locale.GetSelectedLocale();
    allLocales = locale.GetLocales();

    auto adapter = std::make_shared<Adapter>();
    adapter->SetSelectable(true);

    int selectedIndex = 0;
    for (size_t i = 0; i < allLocales.size(); i++) {
        adapter->AddEntry(allLocales[i]);
        if (allLocales[i] == currentLocale) {
            selectedIndex = narrow_cast<int>(i);
        }
    }

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("locale_overlay_select_title"))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [callback, currentLocale]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (allLocales[index] != currentLocale) {
                    i18n::Locale::Instance().SetSelectedLocale(allLocales[index]);
                    showNeedsRestart(callback);
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void musik::cube::SettingsOverlays::ShowLibraryTypeOverlay(std::function<void()> callback) {
    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);

    const auto libraryType = static_cast<ILibrary::Type>(prefs->GetInt(
        core::prefs::keys::LibraryType, (int) ILibrary::Type::Local));

    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    auto adapter = std::make_shared<Adapter>();
    adapter->SetSelectable(true);
    adapter->AddEntry(_TSTR("settings_library_type_local"));
    adapter->AddEntry(_TSTR("settings_library_type_remote"));

    const int selectedIndex = libraryType == ILibrary::Type::Local ? 0 : 1;

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("settings_library_type_overlay_title"))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [prefs, callback]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                const auto updatedType = index == 0 ? ILibrary::Type::Local : ILibrary::Type::Remote;
                prefs->SetInt(core::prefs::keys::LibraryType, narrow_cast<int>(updatedType));
                callback();
            });

    cursespp::App::Overlays().Push(dialog);
}

void musik::cube::SettingsOverlays::CheckShowFirstRunDialog() {
    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);

    if (!prefs->GetBool(cube::prefs::keys::FirstRunSettingsDisplayed)) {
        auto dialog = std::make_shared<DialogOverlay>();

        std::string message = u8fmt(
            _TSTR("settings_first_run_dialog_body"),
            Hotkeys::Get(Hotkeys::NavigateLibrary).c_str(),
            Hotkeys::Get(Hotkeys::NavigateConsole).c_str());

        (*dialog)
            .SetTitle(_TSTR("settings_first_run_dialog_title"))
            .SetMessage(message)
            .AddButton(
                "KEY_ENTER",
                "ENTER",
                _TSTR("button_ok"),
                [prefs](std::string key) {
                    prefs->SetBool(cube::prefs::keys::FirstRunSettingsDisplayed, true);
                });

        App::Overlays().Push(dialog);
    }
}

void musik::cube::SettingsOverlays::CheckShowTlsWarningDialog() {
    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);

    if (!prefs->GetBool(core::prefs::keys::RemoteLibraryTlsWarningSuppressed, false)) {
        const int64_t now = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();

        if (now - lastTlsWarningTime < kTlsWarningCooldownMs) {
            return;
        }

        lastTlsWarningTime = now;

        auto dialog = std::make_shared<DialogOverlay>();

        std::string message = _TSTR("settings_library_type_remote_tls_warning_overlay_message");
        str::ReplaceAll(message, "{{link}}", kTlsInfoUrl.c_str());

        (*dialog)
            .SetTitle(_TSTR("settings_library_type_remote_tls_warning_overlay_title"))
            .SetMessage(message)
            .AddButton(
                "KEY_ENTER",
                "ENTER",
                _TSTR("button_ok"))
            .AddButton(
                "o",
                "o",
                _TSTR("button_open_url"),
                [](std::string key) {
                    musik::core::OpenFile(kTlsInfoUrl);
                })
            .AddButton(
                "d",
                "d",
                _TSTR("button_dont_show_again"),
                [prefs](std::string key) {
                    prefs->SetBool(core::prefs::keys::RemoteLibraryTlsWarningSuppressed, true);
                });

        App::Overlays().Push(dialog);
    }
}

void musik::cube::SettingsOverlays::ShowTranscoderBitrateOverlay(std::function<void()> callback) {
    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);

    const int currentBitrate = prefs->GetInt(
        core::prefs::keys::RemoteLibraryTranscoderBitrate,
        kDefaultTranscoderBitrate);

    int selectedIndex = 0;

    auto adapter = std::make_shared<SimpleScrollAdapter>();
    adapter->SetSelectable(true);
    for (int i = 0; i < kTranscoderBitrates.size(); i++) {
        int const bitrate = kTranscoderBitrates.at(i);
        if (bitrate == currentBitrate) {
            selectedIndex = i;
        }
        adapter->AddEntry(std::to_string(bitrate));
    }

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("settings_library_type_remote_library_transcoder_bitrate_overlay_title"))
        .SetWidth(_DIMEN("transcoder_bitrate_overlay_width", kDefaultTranscoderBitrateOverlayWidth))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [prefs, callback]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                prefs->SetInt(
                    core::prefs::keys::RemoteLibraryTranscoderBitrate,
                    kTranscoderBitrates[index]);
                callback();
            });

    cursespp::App::Overlays().Push(dialog);
}

void musik::cube::SettingsOverlays::ShowTranscoderFormatOverlay(std::function<void()> callback) {
    auto prefs = Preferences::ForComponent(core::prefs::components::Settings);

    std::string const currentFormat = prefs->GetString(
        core::prefs::keys::RemoteLibraryTranscoderFormat,
        kDefaultTranscoderFormat);

    int selectedIndex = 0;

    auto adapter = std::make_shared<SimpleScrollAdapter>();
    adapter->SetSelectable(true);
    for (int i = 0; i < kTranscoderFormats.size(); i++) {
        std::string const format = kTranscoderFormats.at(i);
        if (format == currentFormat) {
            selectedIndex = i;
        }
        adapter->AddEntry(format);
    }

    auto dialog = std::make_shared<ListOverlay>();

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("settings_library_type_remote_library_transcoder_format_overlay_title"))
        .SetWidth(_DIMEN("transcoder_bitrate_overlay_width", kDefaultTranscoderFormatOverlayWidth))
        .SetSelectedIndex(selectedIndex)
        .SetItemSelectedCallback(
            [prefs, callback]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                std::string const updatedFormat = kTranscoderFormats[index];
                prefs->SetString(core::prefs::keys::RemoteLibraryTranscoderFormat, updatedFormat.c_str());
                callback();
            });

    cursespp::App::Overlays().Push(dialog);
}