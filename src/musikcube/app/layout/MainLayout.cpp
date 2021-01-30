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

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/ToastOverlay.h>

#include <musikcore/runtime/Message.h>
#include <musikcore/support/Auddio.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/version.h>

#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/layout/ConsoleLayout.h>
#include <app/layout/LyricsLayout.h>
#include <app/layout/LibraryLayout.h>
#include <app/layout/LibraryNotConnectedLayout.h>
#include <app/layout/SettingsLayout.h>
#include <app/layout/HotkeysLayout.h>
#include <app/util/Hotkeys.h>

#include <map>

#include "SettingsLayout.h"
#include "MainLayout.h"

using namespace musik;
using namespace musik::cube;
using namespace musik::core;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace cursespp;

using MasterLibraryPtr = MainLayout::MasterLibraryPtr;

static inline void updateSyncingText(TextLabel* label, int updates) {
    try {
        if (updates <= 0) {
            label->SetText(
                _TSTR("main_syncing_banner_start"),
                cursespp::text::AlignCenter);
        }
        else {
            label->SetText(u8fmt(
                _TSTR("main_syncing_banner"), updates),
                cursespp::text::AlignCenter);
        }
    }
    catch (...) {
        /* swallow. incomplete locale. don't crash. */
    }
}

static inline void updateRemoteLibraryConnectedText(TextLabel* label, MasterLibraryPtr library) {
    RemoteLibrary* remoteLibrary = dynamic_cast<RemoteLibrary*>(library->Wrapped().get());
    if (remoteLibrary) {
        std::string host = remoteLibrary->WebSocketClient().Uri();
        if (host.find("ws://") == 0) { host = host.substr(5); }
        auto value = u8fmt(_TSTR("library_remote_connected_banner"), host.c_str());
        label->SetText(value, cursespp::text::AlignCenter);
    }
}

MainLayout::MainLayout(
    cursespp::App& app,
    musik::cube::ConsoleLogger* logger,
    musik::core::audio::PlaybackService& playback,
    MasterLibraryPtr library)
: shortcutsFocused(false)
, syncUpdateCount(0)
, library(library)
, playback(playback)
, AppLayout(app) {
    this->prefs = Preferences::ForComponent("settings");

    library->ConnectionStateChanged.connect(this, &MainLayout::OnLibraryConnectionStateChanged);
    library->LibraryChanged.connect(this, &MainLayout::OnLibraryChanged);
    playback.TrackChanged.connect(this, &MainLayout::OnTrackChanged);

    this->RebindIndexerEventHandlers(ILibraryPtr(), this->library);

    /* note we don't create `libraryLayout` here; instead we do it lazily once we're sure
    it has been connected. see SwitchToLibraryLayout() */
    this->libraryNotConnectedLayout = std::make_shared<LibraryNotConnectedLayout>(library);
    this->lyricsLayout = std::make_shared<LyricsLayout>(playback, library);
    this->consoleLayout = std::make_shared<ConsoleLayout>(logger);
    this->settingsLayout = std::make_shared<SettingsLayout>(app, library, playback);
    this->hotkeysLayout = std::make_shared<HotkeysLayout>();

    this->topBanner = std::make_shared<TextLabel>();
    this->topBanner->SetContentColor(Color::Header);
    this->topBanner->Hide();
    this->AddWindow(this->topBanner);

    /* take user to settings if they don't have a valid configuration. otherwise,
    switch to the library view immediately */
    this->SetInitialLayout();
    this->SetAutoHideCommandBar(this->prefs->GetBool(prefs::keys::AutoHideCommandBar, false));

    this->RunUpdateCheck();
}

MainLayout::~MainLayout() {
    updateCheck.Cancel();
}

bool MainLayout::ShowTopBanner() {
    const auto libraryType = this->library->GetType();
    if (libraryType == ILibrary::Type::Local) {
        return this->library->Indexer()->GetState() == IIndexer::StateIndexing;
    }
    else if (libraryType == ILibrary::Type::Remote) {
        return this->library->GetConnectionState() == ILibrary::ConnectionState::Connected;
    }
    return false;
}

void MainLayout::UpdateTopBannerText() {
    const auto libraryType = this->library->GetType();
    if (libraryType == ILibrary::Type::Local) {
        updateSyncingText(this->topBanner.get(), this->syncUpdateCount);
    }
    else if (libraryType == ILibrary::Type::Remote) {
        updateRemoteLibraryConnectedText(this->topBanner.get(), this->library);
    }
}

void MainLayout::OnLayout() {
    if (this->ShowTopBanner()) {
        const int cx = this->GetContentWidth();
        this->SetPadding(1, 0, 0, 0);
        this->topBanner->MoveAndResize(0, 0, cx, 1);
        this->topBanner->Show();
        this->UpdateTopBannerText();
    }
    else {
        this->SetPadding(0, 0, 0, 0);
        this->topBanner->Hide();
    }

    AppLayout::OnLayout();
}

bool MainLayout::KeyPress(const std::string& key) {
    /* deal with top-level view switching first. */
    if (Hotkeys::Is(Hotkeys::NavigateConsole, key)) {
        this->SetLayout(consoleLayout);
        return true;
    }
    else if (auddio::Available() && Hotkeys::Is(Hotkeys::NavigateLyrics, key)) {
        this->Broadcast(message::JumpToLyrics);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateHotkeys, key)) {
        this->SetLayout(hotkeysLayout);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibrary, key)) {
        this->SwitchToLibraryLayout();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
        this->SetLayout(settingsLayout);
        return true;
    }
    else if (key == "M-`") {
        std::string version = u8fmt("%s %s", VERSION, VERSION_COMMIT_HASH);
        ToastOverlay::Show(u8fmt(_TSTR("console_version"), version.c_str()), -1);
        return true;
    }
    else if (this->GetLayout()->KeyPress(key)) {
        return true;
    }
    return AppLayout::KeyPress(key);
}

void MainLayout::Start() {
    MessageQueue().RegisterForBroadcasts(this->shared_from_this());
}

void MainLayout::Stop() {
    MessageQueue().UnregisterForBroadcasts(this);
}

void MainLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    const int type = message.Type();

    if (type == message::JumpToConsole) {
        this->SetLayout(consoleLayout);
    }
    else if (type == message::JumpToSettings) {
        this->SetLayout(settingsLayout);
    }
    else if (type == message::JumpToLyrics) {
        this->SetLayout(lyricsLayout);
    }
    else if (type == message::JumpToHotkeys) {
        this->SetLayout(hotkeysLayout);
    }
    else if (type == message::JumpToLibrary) {
        this->SwitchToLibraryLayout();
    }
    else if (type == message::JumpToPlayQueue) {
        this->SwitchToPlayQueue();
    }
    else if (type == message::IndexerStarted) {
        this->syncUpdateCount = 0;
        this->Layout();
    }
    else if (type == message::IndexerFinished) {
        this->Layout();
    }
    else if (type == message::IndexerProgress) {
        this->syncUpdateCount = narrow_cast<int>(message.UserData1());
        this->UpdateTopBannerText();
        if (!topBanner->IsVisible()) {
            this->Layout();
        }
    }
    else {
        LayoutBase::ProcessMessage(message);
    }
}

bool MainLayout::IsLibraryConnected() {
    using State = ILibrary::ConnectionState;
    return library->GetConnectionState()  == State::Connected;
}

void MainLayout::SetInitialLayout() {
    if (library->IsConfigured()) {
        this->SwitchToLibraryLayout();
    }
    else {
        this->SetLayout(settingsLayout);
    }
}

void MainLayout::SwitchToPlayQueue() {
    if (IsLibraryConnected()) {
        this->SetLayout(libraryLayout);
        libraryLayout->KeyPress(Hotkeys::Get(Hotkeys::NavigateLibraryPlayQueue));
    }
}

void MainLayout::SwitchToLibraryLayout() {
    if (IsLibraryConnected()) {
        if (!this->libraryLayout) {
            this->libraryLayout = std::make_shared<LibraryLayout>(playback, library);
        }
        this->SetLayout(libraryLayout);
    }
    else {
        this->libraryLayout.reset();
        this->SetLayout(this->libraryNotConnectedLayout);
    }
}

void MainLayout::OnLibraryConnectionStateChanged(ILibrary::ConnectionState state) {
    auto currentLayout = this->GetLayout();
    if (currentLayout == this->libraryLayout ||
        currentLayout == this->libraryNotConnectedLayout)
    {
        this->SwitchToLibraryLayout();
    }
}

void MainLayout::OnLibraryChanged(musik::core::ILibraryPtr prev, musik::core::ILibraryPtr curr) {
    this->playback.Stop();
    this->libraryLayout = std::make_shared<LibraryLayout>(playback, library);
    this->RebindIndexerEventHandlers(prev, curr);
    this->Layout();
}

void MainLayout::RebindIndexerEventHandlers(musik::core::ILibraryPtr prev, musik::core::ILibraryPtr curr) {
    if (prev) {
        prev->Indexer()->Started.disconnect(this);
        prev->Indexer()->Finished.disconnect(this);
        prev->Indexer()->Progress.disconnect(this);
    }
    if (curr) {
        curr->Indexer()->Started.connect(this, &MainLayout::OnIndexerStarted);
        curr->Indexer()->Finished.connect(this, &MainLayout::OnIndexerFinished);
        curr->Indexer()->Progress.connect(this, &MainLayout::OnIndexerProgress);
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

void MainLayout::OnTrackChanged(size_t index, musik::core::TrackPtr track) {
    if (prefs->GetBool(
            cube::prefs::keys::DisableWindowTitleUpdates,
            cube::prefs::defaults::DisableWindowTitleUpdates))
    {
        return;
    }

    if (!track) {
        App::Instance().SetTitle("musikcube");
    }
    else {
        App::Instance().SetTitle(u8fmt(
            "musikcube [%s - %s]",
            track->GetString("artist").c_str(),
            track->GetString("title").c_str()));
    }
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
