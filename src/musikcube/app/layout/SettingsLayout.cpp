//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "stdafx.h"

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>
#include <cursespp/Screen.h>
#include <cursespp/SingleLineEntry.h>

#include <core/library/Indexer.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/support/PreferenceKeys.h>
#include <core/audio/Outputs.h>
#include <core/support/Messages.h>

#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/UpdateCheck.h>
#include <app/overlay/ColorThemeOverlay.h>
#include <app/overlay/LastFmOverlay.h>
#include <app/overlay/LocaleOverlay.h>
#include <app/overlay/PlaybackOverlays.h>
#include <app/overlay/PluginOverlay.h>
#include <app/overlay/ServerOverlay.h>
#include <app/overlay/PreampOverlay.h>

#include <boost/format.hpp>

#include "SettingsLayout.h"

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::constants;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;
using namespace std::placeholders;

#ifndef WIN32
#define ENABLE_256_COLOR_OPTION
#endif

#ifdef WIN32
#define ENABLE_MINIMIZE_TO_TRAY
#endif

#define LABEL_HEIGHT 1
#define INPUT_HEIGHT 3
#define HOTKEY_INPUT_WIDTH 20

#define TOP(w) w->GetY()
#define BOTTOM(w) (w->GetY() + w->GetHeight())
#define LEFT(w) w->GetX()
#define RIGHT(w) (w->GetX() + w->GetWidth())

#define CREATE_CHECKBOX(checkbox, caption) \
    checkbox.reset(new cursespp::Checkbox()); \
    checkbox->SetText(caption); \
    checkbox->CheckChanged.connect(this, &SettingsLayout::OnCheckboxChanged);

using EntryPtr = IScrollAdapter::EntryPtr;

static const std::string arrow = "> ";
static bool showDotfiles = false;

static UpdateCheck updateCheck;

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
    playback->SetInt(core::prefs::keys::Transport, (int) type);
}

static std::string resolveThemeName(const std::string& themePath) {
    const boost::filesystem::path p(themePath);
    if (p.has_extension() && p.extension().string() == ".json") {
        std::string fn = p.filename().string();
        return fn.substr(0, fn.rfind("."));
    }
    return _TSTR("settings_default_theme_name");
}

SettingsLayout::SettingsLayout(
    cursespp::App& app,
    musik::core::ILibraryPtr library,
    musik::core::audio::PlaybackService& playback)
: LayoutBase()
, app(app)
, library(library)
, indexer(library->Indexer())
, playback(playback) {
    this->prefs = Preferences::ForComponent(core::prefs::components::Settings);
    this->browseAdapter.reset(new DirectoryAdapter());
    this->addedPathsAdapter.reset(new SimpleScrollAdapter());
    this->UpdateServerAvailability();
    this->InitializeWindows();
}

SettingsLayout::~SettingsLayout() {
    updateCheck.Cancel();
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
        showDotfiles = !showDotfiles;
        this->browseAdapter->SetDotfilesVisible(showDotfiles);
        this->browseList->OnAdapterChanged();
    }
    else if (cb == seekScrubCheckbox.get()) {
        TimeChangeMode mode = cb->IsChecked() ? TimeChangeSeek : TimeChangeScrub;
        this->prefs->SetInt(core::prefs::keys::TimeChangeMode, (int)mode);
        this->seekScrubCheckbox->SetChecked(this->prefs->GetInt(core::prefs::keys::TimeChangeMode) == (int)TimeChangeSeek);
        this->playback.SetTimeChangeMode(mode);
    }
#ifdef ENABLE_256_COLOR_OPTION
    else if (cb == paletteCheckbox.get()) {
        ColorThemeOverlay::Show256ColorsInfo(
            checked,
            [this]() {
            this->LoadPreferences();
        });
    }
#endif
#ifdef ENABLE_MINIMIZE_TO_TRAY
    else if (cb == minimizeToTrayCheckbox.get()) {
        app.SetMinimizeToTray(checked);
        this->prefs->SetBool(cube::prefs::keys::MinimizeToTray, checked);
    }
    else if (cb == startMinimizedCheckbox.get()) {
        this->prefs->SetBool(cube::prefs::keys::StartMinimized, checked);
    }
#endif
    else if (cb == saveSessionCheckbox.get()) {
        this->prefs->SetBool(core::prefs::keys::SaveSessionOnExit, checked);
    }
    else if (cb == autoUpdateCheckbox.get()) {
        this->prefs->SetBool(cube::prefs::keys::AutoUpdateCheck, checked);
    }
}

void SettingsLayout::OnLocaleDropdownActivate(cursespp::TextLabel* label) {
    LocaleOverlay::Show([this](){ this->LoadPreferences(); });
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
    this->BroadcastMessage(message::JumpToHotkeys);
}

void SettingsLayout::OnServerDropdownActivate(cursespp::TextLabel* label) {
    ServerOverlay::Show([this]() { /* nothing, for now */ });
}

void SettingsLayout::OnUpdateDropdownActivate(cursespp::TextLabel* label) {
    updateCheck.Run([this](bool updateRequired, std::string version, std::string url) {
        if (updateRequired) {
            UpdateCheck::ShowUpgradeAvailableOverlay(version, url, false);
        }
        else {
            UpdateCheck::ShowNoUpgradeFoundOverlay();
        }
    });
}

void SettingsLayout::OnThemeDropdownActivate(cursespp::TextLabel* label) {
    ColorThemeOverlay::Show([this]() { this->LoadPreferences(); });
}

void SettingsLayout::OnLayout() {
    int x = this->GetX(), y = this->GetY();
    int cx = this->GetWidth(), cy = this->GetHeight();

    /* top row (directory setup) */
    int startY = 0;
    int leftX = 0;
    int leftWidth = cx / 3; /* 1/3 width */
    int rightX = leftWidth;
    int rightWidth = cx - rightX; /* remainder (~2/3) */

    int pathListsY = startY;
    int pathsHeight = (cy - pathListsY) / 2;

    this->browseList->MoveAndResize(leftX, pathListsY, leftWidth, pathsHeight);
    this->addedPathsList->MoveAndResize(rightX, pathListsY, rightWidth, pathsHeight);

    /* bottom row (dropdowns, checkboxes) */
    int columnCx = (cx - 5) / 2; /* 3 = left + right + middle padding */
    int column1 = 1;
    int column2 = columnCx + 3;

    y = BOTTOM(this->browseList);
    this->localeDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->outputDriverDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->outputDeviceDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->replayGainDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->transportDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->lastFmDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->themeDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->hotkeyDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    this->pluginsDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);

    if (serverAvailable) {
        this->serverDropdown->MoveAndResize(column1, y++, columnCx, LABEL_HEIGHT);
    }

    y = BOTTOM(this->browseList);
#ifdef ENABLE_256_COLOR_OPTION
    this->paletteCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
#endif
    this->dotfileCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
    this->syncOnStartupCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
    this->removeCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
    this->seekScrubCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
#ifdef ENABLE_MINIMIZE_TO_TRAY
    this->minimizeToTrayCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
    this->startMinimizedCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
#endif
    this->saveSessionCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
    this->autoUpdateCheckbox->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);

    y++;
    this->updateDropdown->MoveAndResize(column2, y++, columnCx, LABEL_HEIGHT);
}

void SettingsLayout::RefreshAddedPaths() {
    this->addedPathsAdapter->Clear();

    std::vector<std::string> paths;
    this->indexer->GetPaths(paths);

    for (size_t i = 0; i < paths.size(); i++) {
        auto v = paths.at(i);
        auto e = EntryPtr(new SingleLineEntry(v));
        this->addedPathsAdapter->AddEntry(e);
    }

    this->addedPathsList->OnAdapterChanged();
}

int64_t SettingsLayout::ListItemDecorator(
    ScrollableWindow* scrollable,
    size_t index,
    size_t line,
    IScrollAdapter::EntryPtr entry)
{
    if (scrollable == this->addedPathsList.get() ||
        scrollable == this->browseList.get())
    {
         ListWindow* lw = static_cast<ListWindow*>(scrollable);
         if (lw->GetSelectedIndex() == index) {
             return COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM);
         }
    }
    return -1;
}

void SettingsLayout::InitializeWindows() {
    this->SetFrameVisible(false);

    this->addedPathsList.reset(new cursespp::ListWindow(this->addedPathsAdapter));
    this->addedPathsList->SetFrameTitle(_TSTR("settings_backspace_to_remove"));

    this->browseList.reset(new cursespp::ListWindow(this->browseAdapter));
    this->browseList->SetFrameTitle(_TSTR("settings_space_to_add"));

    ScrollAdapterBase::ItemDecorator decorator =
        std::bind(
            &SettingsLayout::ListItemDecorator,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4);

    this->addedPathsAdapter->SetItemDecorator(decorator);
    this->browseAdapter->SetItemDecorator(decorator);

    this->localeDropdown.reset(new TextLabel());
    this->localeDropdown->Activated.connect(this, &SettingsLayout::OnLocaleDropdownActivate);

    this->outputDriverDropdown.reset(new TextLabel());
    this->outputDriverDropdown->Activated.connect(this, &SettingsLayout::OnOutputDriverDropdownActivated);

    this->outputDeviceDropdown.reset(new TextLabel());
    this->outputDeviceDropdown->Activated.connect(this, &SettingsLayout::OnOutputDeviceDropdownActivated);

    this->replayGainDropdown.reset(new TextLabel());
    this->replayGainDropdown->Activated.connect(this, &SettingsLayout::OnReplayGainDropdownActivated);

    this->transportDropdown.reset(new TextLabel());
    this->transportDropdown->Activated.connect(this, &SettingsLayout::OnTransportDropdownActivate);

    this->lastFmDropdown.reset(new TextLabel());
    this->lastFmDropdown->SetText(arrow + _TSTR("settings_last_fm"));
    this->lastFmDropdown->Activated.connect(this, &SettingsLayout::OnLastFmDropdownActivate);

    this->themeDropdown.reset(new TextLabel());
    this->themeDropdown->SetText(arrow + _TSTR("settings_color_theme") + _TSTR("settings_default_theme_name"));
    this->themeDropdown->Activated.connect(this, &SettingsLayout::OnThemeDropdownActivate);

    this->hotkeyDropdown.reset(new TextLabel());
    this->hotkeyDropdown->SetText(arrow + _TSTR("settings_hotkey_tester"));
    this->hotkeyDropdown->Activated.connect(this, &SettingsLayout::OnHotkeyDropdownActivate);

    this->pluginsDropdown.reset(new TextLabel());
    this->pluginsDropdown->SetText(arrow + _TSTR("settings_enable_disable_plugins"));
    this->pluginsDropdown->Activated.connect(this, &SettingsLayout::OnPluginsDropdownActivate);

    if (this->serverAvailable) {
        this->serverDropdown.reset(new TextLabel());
        this->serverDropdown->SetText(arrow + _TSTR("settings_server_setup"));
        this->serverDropdown->Activated.connect(this, &SettingsLayout::OnServerDropdownActivate);
    }

    this->updateDropdown.reset(new TextLabel());
    this->updateDropdown->SetText(arrow + _TSTR("settings_check_for_updates"));
    this->updateDropdown->Activated.connect(this, &SettingsLayout::OnUpdateDropdownActivate);

    CREATE_CHECKBOX(this->dotfileCheckbox, _TSTR("settings_show_dotfiles"));
    CREATE_CHECKBOX(this->syncOnStartupCheckbox, _TSTR("settings_sync_on_startup"));
    CREATE_CHECKBOX(this->removeCheckbox, _TSTR("settings_remove_missing"));
    CREATE_CHECKBOX(this->seekScrubCheckbox, _TSTR("settings_seek_not_scrub"));

#ifdef ENABLE_256_COLOR_OPTION
    CREATE_CHECKBOX(this->paletteCheckbox, _TSTR("settings_degrade_256"));
#endif
#ifdef ENABLE_MINIMIZE_TO_TRAY
    CREATE_CHECKBOX(this->minimizeToTrayCheckbox, _TSTR("settings_minimize_to_tray"));
    CREATE_CHECKBOX(this->startMinimizedCheckbox, _TSTR("settings_start_minimized"));
#endif
    CREATE_CHECKBOX(this->saveSessionCheckbox, _TSTR("settings_save_session_on_exit"));
    CREATE_CHECKBOX(this->autoUpdateCheckbox, _TSTR("settings_auto_update_check"));

    int order = 0;
    this->browseList->SetFocusOrder(order++);
    this->addedPathsList->SetFocusOrder(order++);
    this->localeDropdown->SetFocusOrder(order++);
    this->outputDriverDropdown->SetFocusOrder(order++);
    this->outputDeviceDropdown->SetFocusOrder(order++);
    this->replayGainDropdown->SetFocusOrder(order++);
    this->transportDropdown->SetFocusOrder(order++);
    this->lastFmDropdown->SetFocusOrder(order++);
    this->themeDropdown->SetFocusOrder(order++);
    this->hotkeyDropdown->SetFocusOrder(order++);
    this->pluginsDropdown->SetFocusOrder(order++);

    if (this->serverAvailable) {
        this->serverDropdown->SetFocusOrder(order++);
    }

#ifdef ENABLE_256_COLOR_OPTION
    this->paletteCheckbox->SetFocusOrder(order++);
#endif
    this->dotfileCheckbox->SetFocusOrder(order++);
    this->syncOnStartupCheckbox->SetFocusOrder(order++);
    this->removeCheckbox->SetFocusOrder(order++);
    this->seekScrubCheckbox->SetFocusOrder(order++);
#ifdef ENABLE_MINIMIZE_TO_TRAY
    this->minimizeToTrayCheckbox->SetFocusOrder(order++);
    this->startMinimizedCheckbox->SetFocusOrder(order++);
#endif
    this->saveSessionCheckbox->SetFocusOrder(order++);
    this->autoUpdateCheckbox->SetFocusOrder(order++);
    this->updateDropdown->SetFocusOrder(order++);

    this->AddWindow(this->browseList);
    this->AddWindow(this->addedPathsList);
    this->AddWindow(this->localeDropdown);
    this->AddWindow(this->outputDriverDropdown);
    this->AddWindow(this->outputDeviceDropdown);
    this->AddWindow(this->replayGainDropdown);
    this->AddWindow(this->transportDropdown);
    this->AddWindow(this->lastFmDropdown);
    this->AddWindow(this->themeDropdown);

    if (this->serverAvailable) {
        this->AddWindow(this->serverDropdown);
    }

#ifdef ENABLE_256_COLOR_OPTION
    this->AddWindow(this->paletteCheckbox);
#endif
    this->AddWindow(this->hotkeyDropdown);
    this->AddWindow(this->pluginsDropdown);

    this->AddWindow(this->dotfileCheckbox);
    this->AddWindow(this->syncOnStartupCheckbox);
    this->AddWindow(this->removeCheckbox);
    this->AddWindow(this->seekScrubCheckbox);
#ifdef ENABLE_MINIMIZE_TO_TRAY
    this->AddWindow(this->minimizeToTrayCheckbox);
    this->AddWindow(this->startMinimizedCheckbox);
#endif
    this->AddWindow(this->saveSessionCheckbox);
    this->AddWindow(this->autoUpdateCheckbox);
    this->AddWindow(updateDropdown);
}

void SettingsLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    if (shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), _TSTR("shortcuts_library"));
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), _TSTR("shortcuts_console"));
        shortcuts->AddShortcut("^D", _TSTR("shortcuts_quit"));

        shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateConsole, key)) {
                this->BroadcastMessage(message::JumpToConsole);
            }
            if (Hotkeys::Is(Hotkeys::NavigateLibrary, key)) {
                this->BroadcastMessage(message::JumpToLibrary);
            }
            else if (key == "^D") {
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
        this->FocusFirst();
        this->RefreshAddedPaths();
        this->LoadPreferences();
        this->CheckShowFirstRunDialog();
    }
}

void SettingsLayout::OnAddedToParent(IWindow* parent) {
#if (__clang_major__ == 7 && __clang_minor__ == 3)
    std::enable_shared_from_this<LayoutBase>* receiver =
        (std::enable_shared_from_this<LayoutBase>*) this;
#else
    auto receiver = this;
#endif
    MessageQueue().RegisterForBroadcasts(receiver->shared_from_this());
    MessageQueue().RegisterForBroadcasts(receiver->shared_from_this());
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

void SettingsLayout::CheckShowFirstRunDialog() {
    if (!this->prefs->GetBool(cube::prefs::keys::FirstRunSettingsDisplayed)) {
        if (!this->firstRunDialog) {
            this->firstRunDialog.reset(new DialogOverlay());

            std::string message = _TSTR("settings_first_run_dialog_body");
            try {
                message = boost::str(boost::format(message)
                    % Hotkeys::Get(Hotkeys::NavigateLibrary)
                    % Hotkeys::Get(Hotkeys::NavigateConsole));
            }
            catch (...) {
            }

            (*this->firstRunDialog)
                .SetTitle(_TSTR("settings_first_run_dialog_title"))
                .SetMessage(message)
                .AddButton(
                    "KEY_ENTER",
                    "ENTER",
                    _TSTR("button_ok"),
                    [this](std::string key) {
                        this->prefs->SetBool(cube::prefs::keys::FirstRunSettingsDisplayed, true);
                        this->firstRunDialog.reset();
                    });

            App::Overlays().Push(this->firstRunDialog);
        }
    }
}

void SettingsLayout::LoadPreferences() {
    this->syncOnStartupCheckbox->SetChecked(this->prefs->GetBool(core::prefs::keys::SyncOnStartup, true));
    this->removeCheckbox->SetChecked(this->prefs->GetBool(core::prefs::keys::RemoveMissingFiles, true));
    this->seekScrubCheckbox->SetChecked(this->prefs->GetInt(core::prefs::keys::TimeChangeMode, TimeChangeScrub) == TimeChangeSeek);

    /* locale */
    this->localeDropdown->SetText(arrow + _TSTR("settings_selected_locale") + i18n::Locale::Instance().GetSelectedLocale());

    /* color theme */
    bool disableCustomColors = this->prefs->GetBool(cube::prefs::keys::DisableCustomColors);
    std::string colorTheme = this->prefs->GetString(cube::prefs::keys::ColorTheme);

    if (colorTheme == "" && !disableCustomColors) {
        colorTheme = _TSTR("settings_default_theme_name");
    }
    else if (disableCustomColors) {
        colorTheme = _TSTR("settings_8color_theme_name");
    }

    this->themeDropdown->SetText(
        arrow + _TSTR("settings_color_theme") +
        resolveThemeName(colorTheme));

#ifdef ENABLE_256_COLOR_OPTION
    this->paletteCheckbox->CheckChanged.disconnect(this);
    this->paletteCheckbox->SetChecked(this->prefs->GetBool(cube::prefs::keys::UsePaletteColors, true));
    this->paletteCheckbox->CheckChanged.connect(this, &SettingsLayout::OnCheckboxChanged);
#endif

#ifdef ENABLE_MINIMIZE_TO_TRAY
    this->minimizeToTrayCheckbox->SetChecked(this->prefs->GetBool(cube::prefs::keys::MinimizeToTray, false));
    this->startMinimizedCheckbox->SetChecked(this->prefs->GetBool(cube::prefs::keys::StartMinimized, false));
#endif
    this->saveSessionCheckbox->SetChecked(this->prefs->GetBool(core::prefs::keys::SaveSessionOnExit, true));
    this->autoUpdateCheckbox->SetChecked(this->prefs->GetBool(cube::prefs::keys::AutoUpdateCheck, true));

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
}

void SettingsLayout::AddSelectedDirectory() {
    size_t index = this->browseList->GetSelectedIndex();

    if (index == ListWindow::NO_SELECTION) {
        index = 0;
    }

    std::string path = this->browseAdapter->GetFullPathAt(index);

    if (path.size()) {
        this->indexer->AddPath(path);
        this->RefreshAddedPaths();
        this->library->Indexer()->Schedule(IIndexer::SyncType::Local);
    }
}

void SettingsLayout::RemoveSelectedDirectory() {
    std::vector<std::string> paths;
    this->indexer->GetPaths(paths);

    size_t index = this->addedPathsList->GetSelectedIndex();
    if (index != ListWindow::NO_SELECTION) {
        this->indexer->RemovePath(paths.at(index));
        this->RefreshAddedPaths();
        this->library->Indexer()->Schedule(IIndexer::SyncType::Local);
    }
}

void SettingsLayout::DrillIntoSelectedDirectory() {
    size_t selectIndexAt = this->browseAdapter->Select(this->browseList.get());

    if (selectIndexAt == DirectoryAdapter::NO_INDEX) {
        selectIndexAt = 0;
    }

    this->browseList->SetSelectedIndex(selectIndexAt);
    this->browseList->ScrollTo(selectIndexAt);
}

void SettingsLayout::UpdateServerAvailability() {
    this->serverAvailable = !!ServerOverlay::FindServerPlugin().get();
}

bool SettingsLayout::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        if (this->GetFocus() == this->browseList) {
            this->DrillIntoSelectedDirectory();
            return true;
        }
    }
    else if (key == " ") {
        if (this->GetFocus() == this->browseList) {
            this->AddSelectedDirectory();
            return true;
        }
    }
    else if (key == "KEY_BACKSPACE" || key == "KEY_DC") { /* backspace */
        if (this->GetFocus() == this->addedPathsList) {
            this->RemoveSelectedDirectory();
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}
