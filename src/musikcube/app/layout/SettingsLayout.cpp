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

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>
#include <cursespp/Screen.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/SchemaOverlay.h>
#include <cursespp/AppLayout.h>

#include <musikcore/library/Indexer.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/support/Messages.h>
#include <musikcore/sdk/ISchema.h>
#include <musikcore/sdk/version.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/overlay/ColorThemeOverlay.h>
#include <app/overlay/LastFmOverlay.h>
#include <app/overlay/SettingsOverlays.h>
#include <app/overlay/PlaybackOverlays.h>
#include <app/overlay/PluginOverlay.h>
#include <app/overlay/ServerOverlay.h>
#include <app/overlay/PreampOverlay.h>
#include <app/util/Rating.h>

#include "SettingsLayout.h"

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::constants;
using namespace musik::core::sdk;
using namespace musik::core::net;
using namespace musik::cube;
using namespace cursespp;

#define ENABLE_COLOR_THEME_SELECTION

#if !defined(WIN32) || defined(PDCURSES_WINCON)
#define ENABLE_UNIX_TERMINAL_OPTIONS
#endif

#ifdef WIN32
#define ENABLE_MINIMIZE_TO_TRAY
#endif

constexpr int kLabelHeight = 1;
constexpr int kInputHeight = 3;
constexpr int kHotkeyInputWidth = 20;

#define TOP(w) w->GetY()
#define BOTTOM(w) (w->GetY() + w->GetHeight())
#define LEFT(w) w->GetX()
#define RIGHT(w) (w->GetX() + w->GetWidth())

#ifdef __arm__
static const int DEFAULT_MAX_INDEXER_THREADS = 2;
#else
static const int DEFAULT_MAX_INDEXER_THREADS = 4;
#endif

using EntryPtr = IScrollAdapter::EntryPtr;

static const std::string arrow = "> ";

static inline std::shared_ptr<ISchema> AdvancedSettingsSchema() {
    auto schema = std::make_shared<TSchema<>>();
#ifndef DISABLE_UPDATE_CHECK
    schema->AddBool(cube::prefs::keys::AutoUpdateCheck, false);
#endif
#ifdef ENABLE_MINIMIZE_TO_TRAY
    schema->AddBool(cube::prefs::keys::MinimizeToTray, false);
    schema->AddBool(cube::prefs::keys::StartMinimized, false);
#endif
    schema->AddBool(cube::prefs::keys::AutoHideCommandBar, false);
    schema->AddInt(core::prefs::keys::RemoteLibraryLatencyTimeoutMs, 5000);
    schema->AddBool(core::prefs::keys::RemoteLibraryIgnoreVersionMismatch, false);
    schema->AddInt(core::prefs::keys::PlaybackTrackQueryTimeoutMs, 5000);
    schema->AddBool(core::prefs::keys::AsyncTrackListQueries, true);
    schema->AddBool(cube::prefs::keys::DisableRatingColumn, false);
    schema->AddBool(cube::prefs::keys::DisableWindowTitleUpdates, cube::prefs::defaults::DisableWindowTitleUpdates);
    schema->AddString(cube::prefs::keys::RatingPositiveChar, kFilledStar.c_str());
    schema->AddString(cube::prefs::keys::RatingNegativeChar, kEmptyStar.c_str());
    schema->AddBool(core::prefs::keys::IndexerLogEnabled, false);
    schema->AddInt(core::prefs::keys::IndexerThreadCount, DEFAULT_MAX_INDEXER_THREADS);
    schema->AddInt(core::prefs::keys::IndexerTransactionInterval, 300);
    schema->AddString(core::prefs::keys::AuddioApiToken, "");
    schema->AddBool(core::prefs::keys::ResumePlaybackOnStartup, false);
    schema->AddBool(core::prefs::keys::PiggyEnabled, false);
    schema->AddString(core::prefs::keys::PiggyHostname, "localhost");
    return schema;
}

static std::string getOutputDeviceName() {
    std::string deviceName = _TSTR("settings_output_device_default");

    std::shared_ptr<IOutput> output = outputs::SelectedOutput();
    if (output) {
        IDevice* device = output->GetDefaultDevice();
        if (device) {
            deviceName = device->Name();
            device->Release();
        }
    }

    return deviceName;
}

static TransportType getTransportType() {
    auto playback = Preferences::ForComponent(core::prefs::components::Playback);

    return (TransportType) playback->GetInt(
        core::prefs::keys::Transport, (int)TransportType::Gapless);
}

static void setTransportType(TransportType type) {
    auto playback = Preferences::ForComponent(core::prefs::components::Playback);
    playback->SetInt(core::prefs::keys::Transport, static_cast<int>(type));
}

SettingsLayout::SettingsLayout(
    cursespp::App& app,
    MasterLibraryPtr library,
    musik::core::audio::PlaybackService& playback)
: LayoutBase()
, app(app)
, library(library)
, indexer(library->Indexer())
, playback(playback) {
    this->prefs = Preferences::ForComponent(core::prefs::components::Settings);
    this->piggyClient = PiggyWebSocketClient::Instance(&MessageQueue());
    this->piggyClient->StateChanged.connect(this, &SettingsLayout::OnPiggyClientStateChange);
    this->UpdateServerAvailability();
    this->InitializeWindows();
}

SettingsLayout::~SettingsLayout() {
    updateCheck.Cancel();
}

void SettingsLayout::OnPiggyClientStateChange(
    PiggyWebSocketClient* client,
    PiggyWebSocketClient::State newState,
    PiggyWebSocketClient::State oldState)
{
    /* trigger a redraw on the main thread */
    using State = PiggyWebSocketClient::State;
    if (newState == State::Connected || newState == State::Disconnected) {
        this->Post(core::message::EnvironmentUpdated);
    }
}

void SettingsLayout::OnCheckboxChanged(cursespp::Checkbox* cb, bool checked) {
    if (cb == syncOnStartupCheckbox.get()) {
        this->prefs->SetBool(core::prefs::keys::SyncOnStartup, checked);
        this->prefs->Save();
    }
    else if (cb == removeCheckbox.get()) {
        this->prefs->SetBool(core::prefs::keys::RemoveMissingFiles, checked);
        this->prefs->Save();
    }
    else if (cb == dotfileCheckbox.get()) {
        this->localLibraryLayout->ToggleShowDotFiles();
    }
#ifdef ENABLE_UNIX_TERMINAL_OPTIONS
    else if (cb == paletteCheckbox.get()) {
        ColorThemeOverlay::Show256ColorsInfo(
            checked,
            [this]() {
                this->LoadPreferences();
            });
    }
    else if (cb == enableTransparencyCheckbox.get()) {
        auto bgType = checked ? Colors::Inherit : Colors::Theme;
        prefs->SetBool(cube::prefs::keys::InheritBackgroundColor, checked);
        app.SetColorBackgroundType(bgType);
    }
#endif
    else if (cb == saveSessionCheckbox.get()) {
        this->prefs->SetBool(core::prefs::keys::SaveSessionOnExit, checked);
    }
}

void SettingsLayout::OnLibraryTypeDropdownActivated(cursespp::TextLabel* label) {
    SettingsOverlays::ShowLibraryTypeOverlay([this]() {
        this->LoadPreferences();
        this->library->LoadDefaultLibrary();
    });
}

void SettingsLayout::OnLocaleDropdownActivate(cursespp::TextLabel* label) {
    SettingsOverlays::ShowLocaleOverlay([this](){ this->LoadPreferences(); });
}

void SettingsLayout::OnOutputDriverDropdownActivated(cursespp::TextLabel* label) {
    std::string currentName;
    std::shared_ptr<IOutput> currentPlugin = outputs::SelectedOutput();
    currentName = currentPlugin ? currentPlugin->Name() : currentName;

    PlaybackOverlays::ShowOutputDriverOverlay(
        getTransportType(),
        [this, currentName] {
            std::string newName;
            std::shared_ptr<IOutput> newPlugin = outputs::SelectedOutput();
            newName = newPlugin ? newPlugin->Name() : newName;

            if (currentName != newName) {
                this->LoadPreferences();
                this->playback.ReloadOutput();
            }
        });
}

void SettingsLayout::OnOutputDeviceDropdownActivated(cursespp::TextLabel* label) {
    std::string currentName = getOutputDeviceName();
    PlaybackOverlays::ShowOutputDeviceOverlay([this, currentName] {
        std::string newName = getOutputDeviceName();
        if (currentName != newName) {
            this->LoadPreferences();
            this->playback.ReloadOutput();
        }
    });
}

void SettingsLayout::OnReplayGainDropdownActivated(cursespp::TextLabel* label) {
    PreampOverlay::Show(this->playback, [this]() { this->LoadPreferences(); });
}

void SettingsLayout::OnTransportDropdownActivate(cursespp::TextLabel* label) {
    const MasterTransport::Type current = getTransportType();

    PlaybackOverlays::ShowTransportOverlay(
        current,
        [this, current](MasterTransport::Type selected) {
            if (selected != current) {
                setTransportType(selected);
                this->playback.ReloadOutput();
                this->LoadPreferences();
            }
        });
}

void SettingsLayout::OnLastFmDropdownActivate(cursespp::TextLabel* label) {
    LastFmOverlay::Start();
}

void SettingsLayout::OnPluginsDropdownActivate(cursespp::TextLabel* label) {
    PluginOverlay::Show();
}

void SettingsLayout::OnHotkeyDropdownActivate(cursespp::TextLabel* label) {
    this->Broadcast(message::JumpToHotkeys);
}

void SettingsLayout::OnServerDropdownActivate(cursespp::TextLabel* label) {
    ServerOverlay::Show([this]() noexcept { });
}

void SettingsLayout::OnAdvancedSettingsActivate(cursespp::TextLabel* label) {
    SchemaOverlay::Show(
        _TSTR("settings_advanced_settings"),
        this->prefs,
        AdvancedSettingsSchema(),
        [this](bool) {
            auto prefs = this->prefs;
            namespace keys = cube::prefs::keys;
            this->app.SetMinimizeToTray(prefs->GetBool(keys::MinimizeToTray, false));
            const bool autoHideCommandBar = prefs->GetBool(keys::AutoHideCommandBar, false);
            dynamic_cast<AppLayout*>(this->app.GetLayout().get())->SetAutoHideCommandBar(autoHideCommandBar);
            updateDefaultRatingSymbols();
        });
}

void SettingsLayout::OnUpdateDropdownActivate(cursespp::TextLabel* label) {
#ifndef DISABLE_UPDATE_CHECK
    updateCheck.Run([this](bool updateRequired, std::string version, std::string url) {
        if (updateRequired) {
            UpdateCheck::ShowUpgradeAvailableOverlay(version, url, false);
        }
        else {
            UpdateCheck::ShowNoUpgradeFoundOverlay();
        }
    });
#endif
}

void SettingsLayout::OnThemeDropdownActivate(cursespp::TextLabel* label) {
    ColorThemeOverlay::Show([this]() { this->LoadPreferences(); });
}

void SettingsLayout::OnLayout() {
    const int x = this->GetX();
    int y = this->GetY();
    const int cx = this->GetWidth(), cy = this->GetHeight();

    /* top row (library config) */
    this->libraryTypeDropdown->MoveAndResize(1, 1, cx - 1, kLabelHeight);
    std::shared_ptr<LayoutBase> libraryLayout;
    static const int kLibraryTypePadding = 3;
    if (this->library->GetType() == ILibrary::Type::Local) {
        const int libraryLayoutHeight = std::min(10, cy / 2);
        this->localLibraryLayout->MoveAndResize(
            kLibraryTypePadding,
            2,
            cx - (kLibraryTypePadding * 2),
            libraryLayoutHeight);
        this->remoteLibraryLayout->Hide();
        libraryLayout = this->localLibraryLayout;
    }
    else {
        this->localLibraryLayout->Hide();
        this->remoteLibraryLayout->MoveAndResize(
            kLibraryTypePadding,
            3,
            cx - (kLibraryTypePadding * 2),
            5);
        libraryLayout = this->remoteLibraryLayout;
    }
    libraryLayout->Show();

    /* bottom row (dropdowns, checkboxes) */
    const int columnCx = (cx - 5) / 2; /* 3 = left + right + middle padding */
    const int column1 = 1;
    const int column2 = columnCx + 3;

    y = BOTTOM(libraryLayout) + 1;
    this->localeDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
    this->outputDriverDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
    this->outputDeviceDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
    this->replayGainDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
    this->transportDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
    this->lastFmDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
#ifdef ENABLE_COLOR_THEME_SELECTION
    this->themeDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
#endif
    this->hotkeyDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);

    if (serverAvailable) {
        this->serverDropdown->MoveAndResize(column1, y++, columnCx, kLabelHeight);
    }

    y = BOTTOM(libraryLayout) + 1;
#ifdef ENABLE_UNIX_TERMINAL_OPTIONS
    this->paletteCheckbox->MoveAndResize(column2, y++, columnCx, kLabelHeight);
    this->enableTransparencyCheckbox->MoveAndResize(column2, y++, columnCx, kLabelHeight);
#endif
    this->dotfileCheckbox->MoveAndResize(column2, y++, columnCx, kLabelHeight);
    this->syncOnStartupCheckbox->MoveAndResize(column2, y++, columnCx, kLabelHeight);
    this->removeCheckbox->MoveAndResize(column2, y++, columnCx, kLabelHeight);
    this->saveSessionCheckbox->MoveAndResize(column2, y++, columnCx, kLabelHeight);
    this->pluginsDropdown->MoveAndResize(column2, y++, columnCx, kLabelHeight);
    this->advancedDropdown->MoveAndResize(column2, y++, columnCx, kLabelHeight);
#ifndef DISABLE_UPDATE_CHECK
    this->updateDropdown->MoveAndResize(column2, y++, columnCx, kLabelHeight);
#endif
    this->appVersion->MoveAndResize(0, cy - 1, cx, kLabelHeight);
}

void SettingsLayout::CreateCheckbox(std::shared_ptr<Checkbox>& cb, const std::string& caption) {
    cb = std::make_shared<Checkbox>();
    cb->SetText(caption);
    cb->CheckChanged.connect(this, &SettingsLayout::OnCheckboxChanged);
}

void SettingsLayout::InitializeWindows() {
    this->SetFrameVisible(false);

    this->libraryTypeDropdown = std::make_shared<TextLabel>();
    this->libraryTypeDropdown->Activated.connect(this, &SettingsLayout::OnLibraryTypeDropdownActivated);

    this->localLibraryLayout = std::make_shared<LocalLibrarySettingsLayout>();
    this->remoteLibraryLayout = std::make_shared<RemoteLibrarySettingsLayout>();

    this->localeDropdown = std::make_shared<TextLabel>();
    this->localeDropdown->Activated.connect(this, &SettingsLayout::OnLocaleDropdownActivate);

    this->outputDriverDropdown = std::make_shared<TextLabel>();
    this->outputDriverDropdown->Activated.connect(this, &SettingsLayout::OnOutputDriverDropdownActivated);

    this->outputDeviceDropdown = std::make_shared<TextLabel>();
    this->outputDeviceDropdown->Activated.connect(this, &SettingsLayout::OnOutputDeviceDropdownActivated);

    this->replayGainDropdown = std::make_shared<TextLabel>();
    this->replayGainDropdown->Activated.connect(this, &SettingsLayout::OnReplayGainDropdownActivated);

    this->transportDropdown = std::make_shared<TextLabel>();
    this->transportDropdown->Activated.connect(this, &SettingsLayout::OnTransportDropdownActivate);

    this->lastFmDropdown = std::make_shared<TextLabel>();
    this->lastFmDropdown->SetText(arrow + _TSTR("settings_last_fm"));
    this->lastFmDropdown->Activated.connect(this, &SettingsLayout::OnLastFmDropdownActivate);

#ifdef ENABLE_COLOR_THEME_SELECTION
    this->themeDropdown = std::make_shared<TextLabel>();
    this->themeDropdown->SetText(arrow + _TSTR("settings_color_theme") + _TSTR("settings_default_theme_name"));
    this->themeDropdown->Activated.connect(this, &SettingsLayout::OnThemeDropdownActivate);
#endif

    this->hotkeyDropdown = std::make_shared<TextLabel>();
    this->hotkeyDropdown->SetText(arrow + _TSTR("settings_hotkey_tester"));
    this->hotkeyDropdown->Activated.connect(this, &SettingsLayout::OnHotkeyDropdownActivate);

    this->pluginsDropdown = std::make_shared<TextLabel>();
    this->pluginsDropdown->SetText(arrow + _TSTR("settings_enable_disable_plugins"));
    this->pluginsDropdown->Activated.connect(this, &SettingsLayout::OnPluginsDropdownActivate);

    if (this->serverAvailable) {
        this->serverDropdown = std::make_shared<TextLabel>();
        this->serverDropdown->SetText(arrow + _TSTR("settings_server_setup"));
        this->serverDropdown->Activated.connect(this, &SettingsLayout::OnServerDropdownActivate);
    }

#ifndef DISABLE_UPDATE_CHECK
    this->updateDropdown = std::make_shared<TextLabel>();
    this->updateDropdown->SetText(arrow + _TSTR("settings_check_for_updates"));
    this->updateDropdown->Activated.connect(this, &SettingsLayout::OnUpdateDropdownActivate);
#endif

    this->advancedDropdown = std::make_shared<TextLabel>();
    this->advancedDropdown->SetText(arrow + _TSTR("settings_advanced_settings"));
    this->advancedDropdown->Activated.connect(this, &SettingsLayout::OnAdvancedSettingsActivate);

    this->CreateCheckbox(this->dotfileCheckbox, _TSTR("settings_show_dotfiles"));
    this->CreateCheckbox(this->syncOnStartupCheckbox, _TSTR("settings_sync_on_startup"));
    this->CreateCheckbox(this->removeCheckbox, _TSTR("settings_remove_missing"));

#ifdef ENABLE_UNIX_TERMINAL_OPTIONS
    this->CreateCheckbox(this->paletteCheckbox, _TSTR("settings_degrade_256"));
    this->CreateCheckbox(this->enableTransparencyCheckbox, _TSTR("settings_enable_transparency"));
#endif
    this->CreateCheckbox(this->saveSessionCheckbox, _TSTR("settings_save_session_on_exit"));

    this->appVersion = std::make_shared<TextLabel>();
    this->appVersion->SetContentColor(Color::TextDisabled);
    this->appVersion->SetAlignment(text::AlignCenter);

    int order = 0;
    this->libraryTypeDropdown->SetFocusOrder(order++);
    this->localLibraryLayout->SetFocusOrder(order++);
    this->remoteLibraryLayout->SetFocusOrder(order++);
    this->localeDropdown->SetFocusOrder(order++);
    this->outputDriverDropdown->SetFocusOrder(order++);
    this->outputDeviceDropdown->SetFocusOrder(order++);
    this->replayGainDropdown->SetFocusOrder(order++);
    this->transportDropdown->SetFocusOrder(order++);
    this->lastFmDropdown->SetFocusOrder(order++);
#ifdef ENABLE_COLOR_THEME_SELECTION
    this->themeDropdown->SetFocusOrder(order++);
#endif
    this->hotkeyDropdown->SetFocusOrder(order++);

    if (this->serverAvailable) {
        this->serverDropdown->SetFocusOrder(order++);
    }

#ifdef ENABLE_UNIX_TERMINAL_OPTIONS
    this->paletteCheckbox->SetFocusOrder(order++);
    this->enableTransparencyCheckbox->SetFocusOrder(order++);
#endif
    this->dotfileCheckbox->SetFocusOrder(order++);
    this->syncOnStartupCheckbox->SetFocusOrder(order++);
    this->removeCheckbox->SetFocusOrder(order++);
    this->saveSessionCheckbox->SetFocusOrder(order++);
    this->pluginsDropdown->SetFocusOrder(order++);
    this->advancedDropdown->SetFocusOrder(order++);
#ifndef DISABLE_UPDATE_CHECK
    this->updateDropdown->SetFocusOrder(order++);
#endif

    this->AddWindow(this->libraryTypeDropdown);
    this->AddWindow(this->localLibraryLayout);
    this->AddWindow(this->remoteLibraryLayout);
    this->AddWindow(this->localeDropdown);
    this->AddWindow(this->outputDriverDropdown);
    this->AddWindow(this->outputDeviceDropdown);
    this->AddWindow(this->replayGainDropdown);
    this->AddWindow(this->transportDropdown);
    this->AddWindow(this->lastFmDropdown);

#ifdef ENABLE_COLOR_THEME_SELECTION
    this->AddWindow(this->themeDropdown);
#endif

    if (this->serverAvailable) {
        this->AddWindow(this->serverDropdown);
    }

#ifdef ENABLE_UNIX_TERMINAL_OPTIONS
    this->AddWindow(this->paletteCheckbox);
    this->AddWindow(this->enableTransparencyCheckbox);
#endif
    this->AddWindow(this->hotkeyDropdown);

    this->AddWindow(this->dotfileCheckbox);
    this->AddWindow(this->syncOnStartupCheckbox);
    this->AddWindow(this->removeCheckbox);
    this->AddWindow(this->saveSessionCheckbox);
    this->AddWindow(this->pluginsDropdown);
    this->AddWindow(this->advancedDropdown);
#ifndef DISABLE_UPDATE_CHECK
    this->AddWindow(this->updateDropdown);
#endif
    this->AddWindow(this->appVersion);
}

void SettingsLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    if (shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), _TSTR("shortcuts_library"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), _TSTR("shortcuts_console"));
        shortcuts->AddShortcut(App::Instance().GetQuitKey(), _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateConsole, key)) {
                this->Broadcast(message::JumpToConsole);
            }
            if (Hotkeys::Is(Hotkeys::NavigateLibrary, key)) {
                this->Broadcast(message::JumpToLibrary);
            }
            else if (key == App::Instance().GetQuitKey()) {
                app.Quit();
            }
            this->KeyPress(key);
        });

        shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateSettings));
    }
}

void SettingsLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->LoadPreferences();
        SettingsOverlays::CheckShowFirstRunDialog();
    }
    else {
        this->remoteLibraryLayout->SavePreferences();
    }
}

void SettingsLayout::OnAddedToParent(IWindow* parent) {
    MessageQueue().RegisterForBroadcasts(this->shared_from_this());
}

void SettingsLayout::OnRemovedFromParent(IWindow* parent) {
    MessageQueue().UnregisterForBroadcasts(this);
}

void SettingsLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    LayoutBase::ProcessMessage(message);
    if (message.Type() == core::message::EnvironmentUpdated) {
        this->LoadPreferences();
    }
}

void SettingsLayout::LoadPreferences() {
    this->syncOnStartupCheckbox->SetChecked(this->prefs->GetBool(core::prefs::keys::SyncOnStartup, true));
    this->removeCheckbox->SetChecked(this->prefs->GetBool(core::prefs::keys::RemoveMissingFiles, true));

    /* locale */
    this->localeDropdown->SetText(arrow + _TSTR("settings_selected_locale") + i18n::Locale::Instance().GetSelectedLocale());

    /* color theme */
    const bool disableCustomColors = this->prefs->GetBool(cube::prefs::keys::DisableCustomColors);
    std::string colorTheme = this->prefs->GetString(cube::prefs::keys::ColorTheme);

    if (colorTheme == "" && !disableCustomColors) {
        colorTheme = _TSTR("settings_default_theme_name");
    }
    else if (disableCustomColors) {
        colorTheme = _TSTR("settings_8color_theme_name");
    }

#ifdef ENABLE_COLOR_THEME_SELECTION
    this->themeDropdown->SetText(arrow + _TSTR("settings_color_theme") + colorTheme);
#endif

#ifdef ENABLE_UNIX_TERMINAL_OPTIONS
    this->paletteCheckbox->CheckChanged.disconnect(this);
    this->paletteCheckbox->SetChecked(this->prefs->GetBool(cube::prefs::keys::UsePaletteColors, true));
    this->paletteCheckbox->CheckChanged.connect(this, &SettingsLayout::OnCheckboxChanged);

    this->enableTransparencyCheckbox->SetChecked(this->prefs->GetBool(cube::prefs::keys::InheritBackgroundColor, false));
    this->enableTransparencyCheckbox->CheckChanged.connect(this, &SettingsLayout::OnCheckboxChanged);
#endif

    this->saveSessionCheckbox->SetChecked(this->prefs->GetBool(core::prefs::keys::SaveSessionOnExit, true));

    /* output driver */
    std::shared_ptr<IOutput> output = outputs::SelectedOutput();
    if (output) {
        this->outputDriverDropdown->SetText(arrow + _TSTR("settings_output_driver") + output->Name());
    }

    /* output device */
    std::string deviceName = getOutputDeviceName();
    this->outputDeviceDropdown->SetText(arrow + _TSTR("settings_output_device") + deviceName);

    /* replay gain */
    this->replayGainDropdown->SetText(arrow + _TSTR("settings_preamp"));

    /* transport type */
    std::string transportName =
        getTransportType() == MasterTransport::Type::Gapless
            ? _TSTR("settings_transport_type_gapless")
            : _TSTR("settings_transport_type_crossfade");

    this->transportDropdown->SetText(arrow + _TSTR("settings_transport_type") + transportName);

    /* library type */
    const auto libraryType = static_cast<ILibrary::Type>(
        prefs->GetInt(
            core::prefs::keys::LibraryType,
            static_cast<int>(ILibrary::Type::Local)));

    std::string libraryTypeLabel = libraryType == ILibrary::Type::Local
        ? _TSTR("settings_library_type_local")
        : _TSTR("settings_library_type_remote");

    this->libraryTypeDropdown->SetText(arrow + _TSTR("settings_library_type") + libraryTypeLabel);

    this->localLibraryLayout->LoadPreferences();
    this->remoteLibraryLayout->LoadPreferences();

    /* version, status */
    std::string piggyStatus = "";
    if (this->piggyAvailable) {
        if (this->piggyClient->ConnectionState() == PiggyWebSocketClient::State::Connected) {
            piggyStatus = " (oo)";
        }
        else {
            piggyStatus = " (..)";
        }
    }
    std::string version = u8fmt("%s %s%s", MUSIKCUBE_VERSION, MUSIKCUBE_VERSION_COMMIT_HASH, piggyStatus.c_str());
    this->appVersion->SetText(u8fmt(_TSTR("console_version"), version.c_str()));

    this->Layout();
}

void SettingsLayout::UpdateServerAvailability() {
    this->serverAvailable = !!ServerOverlay::FindServerPlugin().get();
    this->piggyAvailable = this->prefs->GetBool(core::prefs::keys::PiggyEnabled, false);
}
