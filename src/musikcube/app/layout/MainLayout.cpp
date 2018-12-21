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

#include <stdafx.h>

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
, syncUpdateCount(0)
, library(library)
, AppLayout(app) {
    this->prefs = Preferences::ForComponent("settings");

    library->Indexer()->Started.connect(this, &MainLayout::OnIndexerStarted);
    library->Indexer()->Finished.connect(this, &MainLayout::OnIndexerFinished);
    library->Indexer()->Progress.connect(this, &MainLayout::OnIndexerProgress);

    this->libraryLayout = std::make_shared<LibraryLayout>(playback, library);
    this->consoleLayout = std::make_shared<ConsoleLayout>(playback.GetTransport(), library);
    this->settingsLayout = std::make_shared<SettingsLayout>(app, library, playback);
    this->hotkeysLayout = std::make_shared<HotkeysLayout>();

    this->syncing.reset(new TextLabel());
    this->syncing->SetContentColor(Color::Banner);
    this->syncing->Hide();
    this->AddWindow(this->syncing);

    /* take user to settings if they don't have a valid configuration. otherwise,
    switch to the library view immediately */
    std::vector<std::string> paths;
    library->Indexer()->GetPaths(paths);
    this->SetLayout(paths.size() > 0 ? libraryLayout : settingsLayout);

    this->RunUpdateCheck();
}

MainLayout::~MainLayout() {
    updateCheck.Cancel();
}

void MainLayout::OnLayout() {
    if (this->library->Indexer()->GetState() == IIndexer::StateIndexing) {
        size_t cx = this->GetContentWidth();
        this->SetPadding(1, 0, 0, 0);
        this->syncing->MoveAndResize(0, 0, cx, 1);
        this->syncing->Show();

        if (this->syncUpdateCount == 0) {
            updateSyncingText(this->syncing.get(), 0);
        }
    }
    else {
        this->SetPadding(0, 0, 0, 0);
        this->syncing->Hide();
    }

    AppLayout::OnLayout();
}

bool MainLayout::KeyPress(const std::string& key) {
    /* deal with top-level view switching first. */
    if (Hotkeys::Is(Hotkeys::NavigateConsole, key)) {
        this->SetLayout(consoleLayout);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateHotkeys, key)) {
        this->SetLayout(hotkeysLayout);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibrary, key)) {
        this->SetLayout(libraryLayout);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
        this->SetLayout(settingsLayout);
        return true;
    }

    return AppLayout::KeyPress(key);
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
        this->SetLayout(consoleLayout);
    }
    else if (type == message::JumpToSettings) {
        this->SetLayout(settingsLayout);
    }
    else if (type == message::JumpToHotkeys) {
        this->SetLayout(hotkeysLayout);
    }
    else if (type == message::JumpToLibrary) {
        this->SetLayout(libraryLayout);
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
    this->Post(message::IndexerStarted);
}

void MainLayout::OnIndexerProgress(int count) {
    this->Post(message::IndexerProgress, count);
}

void MainLayout::OnIndexerFinished(int count) {
    this->Post(message::IndexerFinished);
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