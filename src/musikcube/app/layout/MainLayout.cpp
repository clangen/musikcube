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
#include <musikcore/log.hpp>

#include <musikcore/settings.hpp>

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/ToastOverlay.h>

#include <common/runtime/Message.h>
#include <common/version.h>

#include <app/util/Messages.h>
#include <app/util/SettingKeys.h>
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
using namespace musik::core::runtime;
using namespace cursespp;

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

static inline void updateRemoteLibraryConnectedText(TextLabel* label, const api::LibraryStatus& library) {
    auto host = library.endpoint;
    if (!host.empty()) {
        if (host.find("ws://") == 0) { host = host.substr(5); }
        auto value = u8fmt(_TSTR("library_remote_connected_banner"), host.c_str());
        label->SetText(value, cursespp::text::AlignCenter);
    }
}

MainLayout::MainLayout(
    cursespp::App& app,
    musik::cube::ConsoleLogger* logger,
    api::Context& musikcore)
: AppLayout(app)
, playback(musikcore.GetPlayback())
, shortcutsFocused(false)
, syncUpdateCount(0)
, musikcore(musikcore) {

    /* note we don't create `libraryLayout` here; instead we do it lazily once we're sure
    it has been connected. see SwitchToLibraryLayout() */
    this->libraryNotConnectedLayout = std::make_shared<LibraryNotConnectedLayout>(musikcore);
    this->lyricsLayout = std::make_shared<LyricsLayout>(musikcore);
    this->consoleLayout = std::make_shared<ConsoleLayout>(logger);
    this->settingsLayout = std::make_shared<SettingsLayout>(app, musikcore);
    this->hotkeysLayout = std::make_shared<HotkeysLayout>();

    this->topBanner = std::make_shared<TextLabel>();
    this->topBanner->SetContentColor(Color::Header);
    this->topBanner->Hide();
    this->AddWindow(this->topBanner);

    /* take user to settings if they don't have a valid configuration. otherwise,
    switch to the library view immediately */
    this->SetInitialLayout();
    this->SetAutoHideCommandBar(this->prefs.GetBool(prefs::keys::AutoHideCommandBar, false));

    this->RunUpdateCheck();
}

MainLayout::~MainLayout() {
    this->Shutdown();
    updateCheck.Cancel();
}

bool MainLayout::ShowTopBanner() {
    const auto status = this->musikcore.GetLibraryStatus();
    if (status.kind == MC_LIBRARY_LOCAL) {
        return api::Indexer(this->musikcore).State() == MC_INDEXER_INDEXING;
    }
    return status.kind == MC_LIBRARY_REMOTE && status.state == MC_LIBRARY_CONNECTED;
}

void MainLayout::UpdateTopBannerText() {
    const auto status = this->musikcore.GetLibraryStatus();
    if (status.kind == MC_LIBRARY_LOCAL) {
        updateSyncingText(this->topBanner.get(), this->syncUpdateCount);
    }
    else if (status.kind == MC_LIBRARY_REMOTE) {
        updateRemoteLibraryConnectedText(this->topBanner.get(), status);
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
    else if (Hotkeys::Is(Hotkeys::NavigateLyrics, key)) {
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
        std::string version = u8fmt("%s %s", MUSIKCUBE_VERSION, MUSIKCUBE_VERSION_COMMIT_HASH);
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
    this->musikcore.SetLibraryChangedCallback([](void* data, bool selectionChanged) {
        static_cast<MainLayout*>(data)->OnLibraryChanged(selectionChanged);
    }, this);
    api::SetPlaylistChangedCallback(this->musikcore, [](void* data, mc_playlist_event event, int64_t id) {
        auto& layout = *static_cast<MainLayout*>(data);
        if (layout.libraryLayout) {
            layout.libraryLayout->OnPlaylistChanged(event, id);
        }
    }, this);
    this->playback.SetChangedCallback([](void* data, mc_playback_event) {
        static_cast<MainLayout*>(data)->UpdateWindowTitle();
    }, this, { MC_PLAYBACK_CHANGED_TRACK });
    api::Indexer(this->musikcore).SetCallback([](void* data, mc_indexer_event event, int updates) {
        static_cast<MainLayout*>(data)->OnIndexerEvent(event, updates);
    }, this);
    this->UpdateWindowTitle();
    this->musikcore.Dispatch();
}

void MainLayout::Shutdown() {
    this->playback.SetChangedCallback(nullptr);
    api::Indexer(this->musikcore).SetCallback(nullptr);
    this->musikcore.SetLibraryChangedCallback(nullptr);
    api::SetPlaylistChangedCallback(this->musikcore, nullptr);
    if (this->libraryLayout) this->libraryLayout->CancelLibraryRequests();
    this->musikcore.Shutdown();
    MessageQueue().Remove(nullptr, message::DispatchCoreRequests);
    MessageQueue().UnregisterForBroadcasts(this);
}

void MainLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    const int type = message.Type();

    if (type == message::DispatchCoreRequests) {
        this->musikcore.Dispatch();
    }
    else if (type == message::JumpToConsole) {
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
    else {
        LayoutBase::ProcessMessage(message);
    }
}

bool MainLayout::IsLibraryConnected() {
    return this->musikcore.GetLibraryStatus().state == MC_LIBRARY_CONNECTED;
}

void MainLayout::SetInitialLayout() {
    if (this->musikcore.GetLibraryStatus().configured) {
        this->SwitchToLibraryLayout();
    }
    else {
        this->SetLayout(settingsLayout);
    }
}

void MainLayout::SwitchToPlayQueue() {
    if (IsLibraryConnected()) {
        this->SwitchToLibraryLayout();
        if (this->libraryLayout) this->libraryLayout->KeyPress(Hotkeys::Get(Hotkeys::NavigateLibraryPlayQueue));
    }
}

void MainLayout::SwitchToLibraryLayout() {
    if (IsLibraryConnected()) {
        if (!this->libraryLayout) {
            const auto selected = this->musikcore.GetLibrary();
            if (!selected) {
                this->SetLayout(this->libraryNotConnectedLayout);
                return;
            }
            this->libraryLayout = std::make_shared<LibraryLayout>(
                this->musikcore);
        }
        this->SetLayout(libraryLayout);
    }
    else {
        this->libraryLayout.reset();
        this->SetLayout(this->libraryNotConnectedLayout);
    }
}

void MainLayout::OnLibraryChanged(bool) {
    const auto currentLayout = this->GetLayout();
    const bool showingLibrary = currentLayout == this->libraryLayout ||
        currentLayout == this->libraryNotConnectedLayout;

    /* Disconnect and reconnect may arrive together. Retire the old view either way. */
    if (this->libraryLayout) {
        this->libraryLayout->CancelLibraryRequests();
        this->libraryLayout.reset();
    }

    this->libraryNotConnectedLayout->UpdateErrorText();
    if (showingLibrary) {
        this->SwitchToLibraryLayout();
    }
    this->Layout();
}

void MainLayout::OnIndexerEvent(mc_indexer_event event, int updates) {
    if (this->musikcore.GetLibraryStatus().kind != MC_LIBRARY_LOCAL) {
        return;
    }

    if (this->libraryLayout) {
        this->libraryLayout->OnIndexerEvent(event, updates);
    }
    if (event == MC_INDEXER_STARTED) {
        this->syncUpdateCount = 0;
    }
    else if (event == MC_INDEXER_PROGRESS) {
        this->syncUpdateCount = updates;
    }
    this->Layout();
}

void MainLayout::UpdateWindowTitle() {
    if (prefs.GetBool(cube::prefs::keys::DisableWindowTitleUpdates,
        cube::prefs::defaults::DisableWindowTitleUpdates))
    {
        return;
    }

    try {
        const auto track = this->playback.CurrentTrackMetadata();
        if (!track) {
            App::Instance().SetTitle("musikcube");
            return;
        }

        App::Instance().SetTitle("musikcube [" + std::string(track->Artist()) +
            " - " + std::string(track->Title()) + "]");
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "MainLayout", error.what());
    }
}

void MainLayout::RunUpdateCheck() {
#ifndef DISABLE_UPDATE_CHECK
    if (!prefs.GetBool(cube::prefs::keys::AutoUpdateCheck, true)) {
        return;
    }

    updateCheck.Run([](bool updateRequired, std::string version, std::string url) {
        if (updateRequired) {
            UpdateCheck::ShowUpgradeAvailableOverlay(version, url);
        }
    });
#endif
}
