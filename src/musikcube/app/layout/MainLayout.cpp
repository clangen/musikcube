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

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>

#include <core/runtime/Message.h>

#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/UpdateCheck.h>
#include <app/layout/ConsoleLayout.h>
#include <app/layout/LibraryLayout.h>
#include <app/layout/SettingsLayout.h>
#include <app/layout/HotkeysLayout.h>
#include <app/util/Hotkeys.h>

#include <map>

#include "SettingsLayout.h"
#include "MainLayout.h"

using namespace musik;
using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::runtime;
using namespace cursespp;

static UpdateCheck updateCheck;
static std::map<ILayout*, int> lastFocusMap;

#define ENABLE_DEMO_MODE 0

#if ENABLE_DEMO_MODE
static std::string lastKey;
static int lastKeyRepeat = 0;
#endif

static int last(ILayout* layout) {
    auto it = lastFocusMap.find(layout);
    return (it == lastFocusMap.end()) ? 0 : it->second;
}

static void last(ILayout* layout, int last) {
    lastFocusMap[layout] = last;
}

static void updateSyncingText(TextLabel* label, int updates) {
    try {
        if (updates <= 0) {
            label->SetText(
                _TSTR("main_syncing_banner_start"),
                cursespp::text::AlignCenter);
        }
        else {
            label->SetText(boost::str(boost::format(
                _TSTR("main_syncing_banner")) % updates), cursespp::text::AlignCenter);
        }
    }
    catch (...) {
        /* swallow. incomplete locale. don't crash. */
    }
}

MainLayout::MainLayout(
    cursespp::App& app,
    musik::core::audio::PlaybackService& playback,
    ILibraryPtr library)
: shortcutsFocused(false)
, topLevelLayout(nullptr)
, syncUpdateCount(0)
, library(library)
, LayoutBase() {
    this->Initialize();

    this->prefs = Preferences::ForComponent("settings");
    this->ResizeToViewport();

    library->Indexer()->Started.connect(this, &MainLayout::OnIndexerStarted);
    library->Indexer()->Finished.connect(this, &MainLayout::OnIndexerFinished);
    library->Indexer()->Progress.connect(this, &MainLayout::OnIndexerProgress);

    this->libraryLayout = std::make_shared<LibraryLayout>(playback, library);
    this->consoleLayout = std::make_shared<ConsoleLayout>(playback.GetTransport(), library);
    this->settingsLayout = std::make_shared<SettingsLayout>(app, library, playback);
    this->hotkeysLayout = std::make_shared<HotkeysLayout>();

    /* take user to settings if they don't have a valid configuration. otherwise,
    switch to the library view immediately */
    std::vector<std::string> paths;
    library->Indexer()->GetPaths(paths);
    this->SetMainLayout(paths.size() > 0 ? libraryLayout : settingsLayout);

    this->EnableDemoModeIfNecessary();
    this->RunUpdateCheck();
}

MainLayout::~MainLayout() {
    updateCheck.Cancel();
}

void MainLayout::ResizeToViewport() {
    this->MoveAndResize(0, 0, Screen::GetWidth(), Screen::GetHeight());
}

void MainLayout::OnLayout() {
    size_t cx = Screen::GetWidth(), cy = Screen::GetHeight();

#if ENABLE_DEMO_MODE
    this->hotkey->MoveAndResize(0, cy - 1, cx, 1);
    --cy;
#endif

    int yOffset = 0;

    auto state = this->library->Indexer()->GetState();
    if (state == IIndexer::StateIndexing) {
        yOffset = 1;
        this->syncing->MoveAndResize(0, 0, cx, 1);
        this->syncing->Show();

        if (this->syncUpdateCount == 0) {
            updateSyncingText(this->syncing.get(), 0);
        }
    }
    else {
        this->syncing->Hide();
    }

    if (this->layout) {
        this->layout->MoveAndResize(0, yOffset, cx, cy - 1 - yOffset);
        this->layout->Show();
        this->layout->BringToTop();

        if (this->shortcutsFocused) {
            this->layout->SetFocus(IWindowPtr());
        }
    }

    this->shortcuts->MoveAndResize(0, cy - 1, cx, 1);
}

void MainLayout::Initialize() {
    this->shortcuts.reset(new ShortcutsWindow());
    this->AddWindow(this->shortcuts);

#if ENABLE_DEMO_MODE
    this->hotkey.reset(new TextLabel());
    this->hotkey->SetContentColor(CURSESPP_FOOTER);
    this->hotkey->SetText("keypress: <none>", text::AlignCenter);
    this->AddWindow(this->hotkey);
#endif

    this->syncing.reset(new TextLabel());
    this->syncing->SetContentColor(CURSESPP_BANNER);
    this->syncing->Hide();
    this->AddWindow(this->syncing);
}

cursespp::IWindowPtr MainLayout::GetFocus() {
    if (this->shortcutsFocused) {
        return this->shortcuts;
    }

    if (this->layout) {
        return this->layout->GetFocus();
    }

    return cursespp::IWindowPtr();
}

IWindowPtr MainLayout::FocusNext() {
    return (this->shortcutsFocused)
        ? this->BlurShortcuts() : this->layout->FocusNext();
}
IWindowPtr MainLayout::FocusPrev() {
    return (this->shortcutsFocused)
        ? this->BlurShortcuts() : this->layout->FocusPrev();
}

void MainLayout::SetMainLayout(std::shared_ptr<cursespp::LayoutBase> layout) {
    if (layout != this->layout) {
        if (this->layout) {
            if (this->lastFocus) {
                this->layout->SetFocus(this->lastFocus);
            }

            this->RemoveWindow(this->layout);
            last(this->layout.get(), this->layout->GetFocusIndex());
            this->layout->SetFocusIndex(-1);
            this->layout->Hide();
        }

        this->lastFocus.reset();

        if (this->topLevelLayout) {
            this->topLevelLayout->SetShortcutsWindow(nullptr);
        }

        this->layout = layout;
        this->shortcuts->RemoveAll();

        if (this->layout) {
            this->topLevelLayout = dynamic_cast<ITopLevelLayout*>(layout.get());
            if (this->topLevelLayout) {
                this->topLevelLayout->SetShortcutsWindow(this->shortcuts.get());
            }

            this->AddWindow(this->layout);
            this->layout->SetFocusOrder(0);
            this->layout->SetFocusIndex(last(this->layout.get()));
            this->Layout();
        }
    }
}

cursespp::IWindowPtr MainLayout::BlurShortcuts() {
    this->shortcuts->Blur();
    this->shortcutsFocused = false;

    if (this->layout) {
        bool refocused = false;
        if (this->lastFocus) {
            refocused = this->layout->SetFocus(this->lastFocus);
            this->lastFocus.reset();
        }

        if (!refocused) {
            this->layout->FocusNext();
        }
    }

    return this->layout ? this->layout->GetFocus() : IWindowPtr();
}

void MainLayout::FocusShortcuts() {
    this->shortcuts->Focus();

    if (this->layout) {
        this->lastFocus = this->layout->GetFocus();

        if (this->lastFocus) {
            this->lastFocus->Blur();
        }

        this->layout->SetFocus(IWindowPtr());
    }

    this->shortcuts->Focus();
}

bool MainLayout::KeyPress(const std::string& key) {
    /* deal with top-level view switching first. */
    if (Hotkeys::Is(Hotkeys::NavigateConsole, key)) {
        this->SetMainLayout(consoleLayout);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateHotkeys, key)) {
        this->SetMainLayout(hotkeysLayout);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibrary, key)) {
        this->SetMainLayout(libraryLayout);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
        this->SetMainLayout(settingsLayout);
        return true;
    }

    /* otherwise, see if the user is monkeying around with the
    shortcut bar focus... */
    if (key == "^["  ||
        (key == "KEY_ENTER" && this->shortcutsFocused) ||
        (Hotkeys::Is(Hotkeys::Up, key) && this->shortcutsFocused))
    {
        this->shortcutsFocused = !this->shortcutsFocused;

        if (this->shortcutsFocused) {
            this->FocusShortcuts();
        }
        else {
            this->BlurShortcuts();
        }

        return true;
    }

    if (this->shortcutsFocused) {
        if (Hotkeys::Is(Hotkeys::Down, key) || Hotkeys::Is(Hotkeys::Left, key) ||
            Hotkeys::Is(Hotkeys::Up, key) || Hotkeys::Is(Hotkeys::Right, key))
        {
            /* layouts allow focusing via TAB and sometimes arrow
            keys. suppress these from bubbling. */
            return true;
        }
    }

    /* otherwise, pass along to our child layout */
    return this->layout ? this->layout->KeyPress(key) : false;
}

void MainLayout::Start() {
#if (__clang_major__ == 7 && __clang_minor__ == 3)
    std::enable_shared_from_this<MainLayout>* receiver =
        (std::enable_shared_from_this<MainLayout>*) this;
#else
    auto receiver = this;
#endif
    MessageQueue().RegisterForBroadcasts(receiver->shared_from_this());
}

void MainLayout::Stop() {
    MessageQueue().UnregisterForBroadcasts(this);
}

void MainLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    int type = message.Type();

    if (type == message::JumpToConsole) {
        this->SetMainLayout(consoleLayout);
    }
    else if (type == message::JumpToSettings) {
        this->SetMainLayout(settingsLayout);
    }
    else if (type == message::JumpToHotkeys) {
        this->SetMainLayout(hotkeysLayout);
    }
    else if (type == message::JumpToLibrary) {
        this->SetMainLayout(libraryLayout);
    }
    if (type == message::IndexerStarted) {
        this->syncUpdateCount = 0;
        this->Layout();
    }
    else if (type == message::IndexerFinished) {
        this->Layout();
    }
    else if (type == message::IndexerProgress) {
        this->syncUpdateCount += (int)message.UserData1();
        updateSyncingText(this->syncing.get(), this->syncUpdateCount);

        if (!syncing->IsVisible()) {
            this->Layout();
        }
    }
}

void MainLayout::OnIndexerStarted() {
    this->PostMessage(message::IndexerStarted);
}

void MainLayout::OnIndexerProgress(int count) {
    this->PostMessage(message::IndexerProgress, count);
}

void MainLayout::OnIndexerFinished(int count) {
    this->PostMessage(message::IndexerFinished);
}

void MainLayout::RunUpdateCheck() {
    if (!prefs->GetBool(cube::prefs::keys::AutoUpdateCheck, true)) {
        return;
    }

    updateCheck.Run([this](bool updateRequired, std::string version, std::string url) {
        if (updateRequired) {
            UpdateCheck::ShowUpgradeAvailableOverlay(version, url);
        }
    });
}

void MainLayout::EnableDemoModeIfNecessary() {
#if ENABLE_DEMO_MODE
    App::Instance().SetKeyHook([this](const std::string& key) -> bool {
        static std::map<std::string, std::string> SANITIZE = {
            { "^I", "TAB" }, { " ", "SPACE" }, { "^[", "ESC" }
        };

        auto it = SANITIZE.find(key);
        std::string normalized = (it == SANITIZE.end()) ? key : it->second;

        if (normalized == lastKey) {
            ++lastKeyRepeat;
            if (lastKeyRepeat >= 2) {
                normalized = normalized + " (x" + std::to_string(lastKeyRepeat) + ")";
            }
        }
        else {
            lastKey = normalized;
            lastKeyRepeat = 1;
        }

        std::string keypress = "keypress: " + (normalized.size() ? normalized : "<none>");
        this->hotkey->SetText(keypress, text::AlignCenter);
        return false;
    });
#endif
}