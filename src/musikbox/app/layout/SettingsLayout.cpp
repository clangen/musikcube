//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <app/util/Hotkeys.h>
#include <app/util/PreferenceKeys.h>
#include <app/overlay/ColorThemeOverlay.h>
#include <app/overlay/PlaybackOverlays.h>
#include <app/overlay/PluginOverlay.h>

#include <boost/format.hpp>

#include "SettingsLayout.h"

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::constants;
using namespace musik::core::sdk;
using namespace musik::box;
using namespace musik::glue::audio;
using namespace cursespp;
using namespace std::placeholders;

#ifndef WIN32
#define ENABLE_256_COLOR_OPTION
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

static const std::string arrow = "\xe2\x96\xba";
static bool showDotfiles = false;

SettingsLayout::SettingsLayout(
    musik::core::ILibraryPtr library,
    musik::glue::audio::MasterTransport& transport)
: LayoutBase()
, library(library)
, indexer(library->Indexer())
, transport(transport) {
    this->libraryPrefs = Preferences::ForComponent(core::prefs::components::Settings);
    this->indexer->PathsUpdated.connect(this, &SettingsLayout::RefreshAddedPaths);
    this->browseAdapter.reset(new DirectoryAdapter());
    this->addedPathsAdapter.reset(new SimpleScrollAdapter());
    this->InitializeWindows();
}

SettingsLayout::~SettingsLayout() {
}

void SettingsLayout::OnCheckboxChanged(cursespp::Checkbox* cb, bool checked) {
    if (cb == syncOnStartupCheckbox.get()) {
        this->libraryPrefs->SetBool(core::prefs::keys::SyncOnStartup, checked);
        this->libraryPrefs->Save();
    }
    else if (cb == removeCheckbox.get()) {
        this->libraryPrefs->SetBool(core::prefs::keys::RemoveMissingFiles, checked);
        this->libraryPrefs->Save();
    }
    else if (cb == dotfileCheckbox.get()) {
        showDotfiles = !showDotfiles;
        this->browseAdapter->SetDotfilesVisible(showDotfiles);
        this->browseList->OnAdapterChanged();
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
}

void SettingsLayout::OnOutputDropdownActivated(cursespp::TextLabel* label) {
    std::string currentName;
    std::shared_ptr<IOutput> currentPlugin = outputs::SelectedOutput();
    currentName = currentPlugin ? currentPlugin->Name() : currentName;

    PlaybackOverlays::ShowOutputOverlay(
        this->transport.GetType(),
        [this, currentName] {
            std::string newName;
            std::shared_ptr<IOutput> newPlugin = outputs::SelectedOutput();
            newName = newPlugin ? newPlugin->Name() : newName;

            if (currentName != newName) {
                this->LoadPreferences();
                this->transport.ReloadOutput();
            }
        });
}

void SettingsLayout::OnTransportDropdownActivate(cursespp::TextLabel* label) {
    const MasterTransport::Type current = this->transport.GetType();

    PlaybackOverlays::ShowTransportOverlay(
        this->transport.GetType(),
        [this, current](int selected) {
            if (selected != current) {
                this->transport.SwitchTo((MasterTransport::Type) selected);
                this->LoadPreferences();
            }
        });
}

void SettingsLayout::OnPluginsDropdownActivate(cursespp::TextLabel* label) {
    PluginOverlay::Show();
}

void SettingsLayout::OnHotkeyDropdownActivate(cursespp::TextLabel* label) {
    std::shared_ptr<InputOverlay> overlay(new InputOverlay());
    overlay->SetTitle("hotkey tester").SetInputMode(IInput::InputRaw);
    App::Overlays().Push(overlay);
}

void SettingsLayout::OnThemeDropdownActivate(cursespp::TextLabel* label) {
    ColorThemeOverlay::Show([this]() { this->LoadPreferences(); });
}

void SettingsLayout::OnLayout() {
    int x = this->GetX(), y = this->GetY();
    int cx = this->GetWidth(), cy = this->GetHeight();

    int startY = 1;
    int leftX = 0;
    int leftWidth = cx / 3; /* 1/3 width */
    int rightX = leftWidth;
    int rightWidth = cx - rightX; /* remainder (~2/3) */

    this->browseLabel->MoveAndResize(leftX + 1, startY, leftWidth - 1, LABEL_HEIGHT);
    this->addedPathsLabel->MoveAndResize(rightX + 1, startY, rightWidth - 1, LABEL_HEIGHT);

    int pathListsY = BOTTOM(this->browseLabel);
    int pathsHeight = (cy - pathListsY) / 2;

    this->browseList->MoveAndResize(leftX, pathListsY, leftWidth, pathsHeight);
    this->addedPathsList->MoveAndResize(rightX, pathListsY, rightWidth, pathsHeight);

    y = BOTTOM(this->browseList);
    this->outputDropdown->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
    this->transportDropdown->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
    this->pluginsDropdown->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
    this->themeDropdown->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
#ifdef ENABLE_256_COLOR_OPTION
    this->paletteCheckbox->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
#endif
    this->hotkeyDropdown->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
    this->dotfileCheckbox->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
    this->syncOnStartupCheckbox->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
    this->removeCheckbox->MoveAndResize(1, y++, cx - 1, LABEL_HEIGHT);
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

int64 SettingsLayout::ListItemDecorator(
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

    this->browseLabel.reset(new TextLabel());
    this->browseLabel->SetText("browse (SPACE to add)", text::AlignCenter);

    this->addedPathsLabel.reset(new TextLabel());
    this->addedPathsLabel->SetText("indexed paths (BACKSPACE to remove)", text::AlignCenter);

    this->addedPathsList.reset(new cursespp::ListWindow(this->addedPathsAdapter));
    this->browseList.reset(new cursespp::ListWindow(this->browseAdapter));

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

    this->outputDropdown.reset(new TextLabel());
    this->outputDropdown->Activated.connect(this, &SettingsLayout::OnOutputDropdownActivated);

    this->transportDropdown.reset(new TextLabel());
    this->transportDropdown->Activated.connect(this, &SettingsLayout::OnTransportDropdownActivate);

    this->pluginsDropdown.reset(new TextLabel());
    this->pluginsDropdown->SetText(arrow + " enable/disable plugins");
    this->pluginsDropdown->Activated.connect(this, &SettingsLayout::OnPluginsDropdownActivate);

    this->themeDropdown.reset(new TextLabel());
    this->themeDropdown->SetText(arrow + " color theme: default");
    this->themeDropdown->Activated.connect(this, &SettingsLayout::OnThemeDropdownActivate);
#ifdef ENABLE_256_COLOR_OPTION
    CREATE_CHECKBOX(this->paletteCheckbox, "degrade to 256 color palette");
#endif

    this->hotkeyDropdown.reset(new TextLabel());
    this->hotkeyDropdown->SetText(arrow + " hotkey tester");
    this->hotkeyDropdown->Activated.connect(this, &SettingsLayout::OnHotkeyDropdownActivate);

    CREATE_CHECKBOX(this->dotfileCheckbox, "show dotfiles in directory browser");
    CREATE_CHECKBOX(this->syncOnStartupCheckbox, "sync metadata on startup");
    CREATE_CHECKBOX(this->removeCheckbox, "remove missing files from library");

    int order = 0;
    this->browseList->SetFocusOrder(order++);
    this->addedPathsList->SetFocusOrder(order++);
    this->outputDropdown->SetFocusOrder(order++);
    this->transportDropdown->SetFocusOrder(order++);
    this->pluginsDropdown->SetFocusOrder(order++);
    this->themeDropdown->SetFocusOrder(order++);
#ifdef ENABLE_256_COLOR_OPTION
    this->paletteCheckbox->SetFocusOrder(order++);
#endif
    this->hotkeyDropdown->SetFocusOrder(order++);
    this->dotfileCheckbox->SetFocusOrder(order++);
    this->syncOnStartupCheckbox->SetFocusOrder(order++);
    this->removeCheckbox->SetFocusOrder(order++);

    this->AddWindow(this->browseLabel);
    this->AddWindow(this->addedPathsLabel);
    this->AddWindow(this->browseList);
    this->AddWindow(this->addedPathsList);
    this->AddWindow(this->outputDropdown);
    this->AddWindow(this->transportDropdown);
    this->AddWindow(this->pluginsDropdown);
    this->AddWindow(this->themeDropdown);
#ifdef ENABLE_256_COLOR_OPTION
    this->AddWindow(this->paletteCheckbox);
#endif
    this->AddWindow(this->hotkeyDropdown);
    this->AddWindow(this->dotfileCheckbox);
    this->AddWindow(this->syncOnStartupCheckbox);
    this->AddWindow(this->removeCheckbox);
}

void SettingsLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    if (shortcuts) {
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), "settings");
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibrary), "library");
        shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateConsole), "console");
        shortcuts->AddShortcut("^D", "quit");
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

void SettingsLayout::CheckShowFirstRunDialog() {
    if (!this->libraryPrefs->GetBool(box::prefs::keys::FirstRunSettingsDisplayed)) {
        if (!this->firstRunDialog) {
            this->firstRunDialog.reset(new DialogOverlay());

            (*this->firstRunDialog)
                .SetTitle("welcome to musikbox!")
                .SetMessage(boost::str(boost::format(
                    "add some directories that contain music files, "
                    "then press '%s' to show the library view and start listening!\n\n"
                    "for troubleshooting, press '%s' to enter the console view.\n\n"
                    "other keyboard shorcuts are displayed in the command bar at the "
                    "bottom of the screen. toggle command mode by pressing 'ESC'.\n\n"
                    "select 'ok' to get started.")
                    % Hotkeys::Get(Hotkeys::NavigateLibrary)
                    % Hotkeys::Get(Hotkeys::NavigateConsole)))
                .AddButton(
                    "KEY_ENTER",
                    "ENTER",
                    "ok",
                    [this](std::string key) {
                        this->libraryPrefs->SetBool(box::prefs::keys::FirstRunSettingsDisplayed, true);
                        this->firstRunDialog.reset();
                    });

            App::Overlays().Push(this->firstRunDialog);
        }
    }
}

void SettingsLayout::LoadPreferences() {
    this->syncOnStartupCheckbox->SetChecked(this->libraryPrefs->GetBool(core::prefs::keys::SyncOnStartup, true));
    this->removeCheckbox->SetChecked(this->libraryPrefs->GetBool(core::prefs::keys::RemoveMissingFiles, true));

    /* color theme */
    bool disableCustomColors = this->libraryPrefs->GetBool(box::prefs::keys::DisableCustomColors);
    std::string colorTheme = this->libraryPrefs->GetString(box::prefs::keys::ColorTheme);

    if (colorTheme == "" && !disableCustomColors) {
        colorTheme = "default";
    }
    else if (disableCustomColors) {
        colorTheme = "8 colors";
    }

    this->themeDropdown->SetText(arrow + " color theme: " + colorTheme);

#ifdef ENABLE_256_COLOR_OPTION
    this->paletteCheckbox->CheckChanged.disconnect(this);
    this->paletteCheckbox->SetChecked(
        this->libraryPrefs->GetBool(box::prefs::keys::UsePaletteColors, true));
    this->paletteCheckbox->CheckChanged.connect(this, &SettingsLayout::OnCheckboxChanged);
#endif

    /* output plugin */
    std::shared_ptr<IOutput> output = outputs::SelectedOutput();
    if (output) {
        this->outputDropdown->SetText(arrow + " output device: " + output->Name());
    }

    /* transport type */
    std::string transportName =
        this->transport.GetType() == MasterTransport::Gapless
            ? "gapless"
            : "crossfade";

    this->transportDropdown->SetText(arrow + " playback mode: " + transportName);
}

void SettingsLayout::AddSelectedDirectory() {
    size_t index = this->browseList->GetSelectedIndex();

    if (index != ListWindow::NO_SELECTION) {
        std::string path = this->browseAdapter->GetFullPathAt(index);

        if (path.size()) {
            this->indexer->AddPath(path);
        }
    }
}

void SettingsLayout::RemoveSelectedDirectory() {
    std::vector<std::string> paths;
    this->indexer->GetPaths(paths);

    size_t index = this->addedPathsList->GetSelectedIndex();
    if (index != ListWindow::NO_SELECTION) {
        this->indexer->RemovePath(paths.at(index));
    }
}

void SettingsLayout::DrillIntoSelectedDirectory() {
    this->browseAdapter->Select(this->browseList->GetSelectedIndex());
    this->browseList->SetSelectedIndex(0);
    this->browseList->OnAdapterChanged();
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
    else if (key == "KEY_BACKSPACE") { /* backspace */
        if (this->GetFocus() == this->addedPathsList) {
            this->RemoveSelectedDirectory();
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}
