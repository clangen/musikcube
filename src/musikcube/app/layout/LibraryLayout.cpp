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

#include <musikcore/preferences.hpp>

#include <musikcore/library.h>

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <common/runtime/Message.h>

#include <app/overlay/BrowseOverlays.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/util/Hotkeys.h>
#include <app/util/Messages.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/MagicConstants.h>
#include <musikcore/support/Playback.h>

#include "LibraryLayout.h"

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::cube;
using namespace musik::core::runtime;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;

#define SHOULD_REFOCUS(target)         \
    (this->visibleLayout == target) && \
        (this->shortcuts && !this->shortcuts->IsFocused())

static void rememberSessionValue(std::string_view key, std::string_view value) {
    api::Preferences session { MC_PREFERENCE_SESSION };
    session.SetString(key, value);
    session.Save();
}

namespace type {
    const std::string CategoryFilter = "CategoryFilter";
    const std::string TrackFilter = "TrackFilter";
    const std::string Browse = "Browse";
    const std::string Directory = "Directory";
    const std::string NowPlaying = "NowPlaying";
};

LibraryLayout::LibraryLayout(musik::core::api::Context& musikcore)
: LayoutBase()
, playback(musikcore.GetPlayback())
, musikcore(musikcore)
, shortcuts(nullptr)
{
    this->InitializeWindows(musikcore);
}

void LibraryLayout::OnLayout() {
    const bool autoHideCommandBar = this->prefs.GetBool(keys::AutoHideCommandBar, false);
    const int x = 0, y = 0;
    const int cx = this->GetWidth(), cy = this->GetHeight();
#ifdef WIN32
    const int transportCy = 3;
#else
    const int transportCy = (autoHideCommandBar ? 2 : 3);
#endif
    const int mainHeight = cy - transportCy;
    this->transportView->MoveAndResize(1, mainHeight, cx - 2, transportCy);
    if (this->visibleLayout) {
        this->visibleLayout->MoveAndResize(x, y, cx, mainHeight);
        this->visibleLayout->Show();
    }
}

void LibraryLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    const auto status = this->musikcore.GetLibraryStatus();
    if (visible && status.kind == MC_LIBRARY_REMOTE && status.state == MC_LIBRARY_CONNECTED) {
        prefs.SetBool(MC_PREFERENCE_KEY_REMOTE_LIBRARY_VIEWED, true);
    }
}

void LibraryLayout::ChangeMainLayout(std::shared_ptr<cursespp::LayoutBase> newLayout) {
    if (this->visibleLayout != newLayout) {
        this->transportView->SetFocus(TransportWindow::FocusNone);

        if (this->visibleLayout) {
            this->RemoveWindow(this->visibleLayout);
            this->visibleLayout->FocusTerminated.disconnect(this);
            this->visibleLayout->Hide();
        }

        this->visibleLayout = newLayout;

        this->visibleLayout->FocusTerminated.connect(
            this, &LibraryLayout::OnMainLayoutFocusTerminated);

        this->AddWindow(this->visibleLayout);
        this->Layout();

        /* ask the visible layout to terminate focusing, not do it
        in a circular fashion. when we hit the end, we'll focus
        the transport! see FocusNext() and FocusPrev(). */
        this->visibleLayout->SetFocusMode(ILayout::FocusModeTerminating);

        if (this->IsVisible()) {
            this->visibleLayout->BringToTop();
        }

        this->OnLayoutChanged();
    }
}

void LibraryLayout::OnLayoutChanged() {
    this->UpdateShortcutsWindow();
}

void LibraryLayout::ShowNowPlaying() {
    this->ChangeMainLayout(this->nowPlayingLayout);
    rememberSessionValue(keys::LastLibraryView, type::NowPlaying);
}

void LibraryLayout::ShowBrowse(const std::string& category) {
    this->ChangeMainLayout(this->browseLayout);
    if (category.size()) {
        this->browseLayout->SwitchCategory(category);
    }
    this->lastBrowseCategoryType = category;
    rememberSessionValue(keys::LastLibraryView, type::Browse);
}

void LibraryLayout::ShowCategorySearch() {
    SHOULD_REFOCUS(this->categorySearchLayout)
    ? this->categorySearchLayout->FocusInput()
    : this->ChangeMainLayout(this->categorySearchLayout);
    rememberSessionValue(keys::LastLibraryView, type::CategoryFilter);
}

void LibraryLayout::ShowTrackSearch() {
    SHOULD_REFOCUS(this->trackSearchLayout)
    ? this->trackSearchLayout->FocusInput()
    : this->ChangeMainLayout(this->trackSearchLayout);

    rememberSessionValue(keys::LastLibraryView, type::TrackFilter);
}

void LibraryLayout::ShowDirectories(const std::string& directory) {
    if (directory.size()) {
        this->directoryLayout->SetDirectory(directory);
        rememberSessionValue(keys::LastBrowseDirectoryRoot, directory);
    }
    this->ChangeMainLayout(this->directoryLayout);
    this->lastBrowseCategoryType = MagicConstants::DirectoryCategoryType;
    rememberSessionValue(keys::LastLibraryView, type::Directory);
}

void LibraryLayout::CancelLibraryRequests() {
    this->playlistWork.Invalidate();
    this->trackSearchLayout->CancelSearchRequests();
    this->categorySearchLayout->CancelSearchRequests();
    this->browseLayout->CancelTrackRequests();
    this->directoryLayout->CancelTrackRequests();
    this->nowPlayingLayout->CancelPlaylistLoad();
}

void LibraryLayout::InitializeWindows(musik::core::api::Context& musikcore) {
    this->browseLayout = std::make_shared<BrowseLayout>(musikcore);
    this->browseLayout->SetOnHeaderClicked([this]() { this->ShowCategoryChooser(); });

    this->directoryLayout = std::make_shared<DirectoryLayout>(musikcore);

    /* Legacy indexer signals stay at the layout boundary until the SDK exposes them. */
    this->nowPlayingLayout = std::make_shared<NowPlayingLayout>(musikcore);
    this->categorySearchLayout = std::make_shared<CategorySearchLayout>(musikcore);
    this->categorySearchLayout->SearchResultSelected.connect(this, &LibraryLayout::OnCategorySearchResultSelected);
    /* Bind legacy row actions and index notifications at the layout boundary. */
    auto searchRows = std::make_shared<TrackListView>(musikcore);
    this->trackSearchLayout = std::make_shared<TrackSearchLayout>(
        std::move(searchRows), musikcore);
    const auto token = this->playlistWork.CurrentToken();
    const auto addTrack = [&musikcore, token](int64_t id) {
        PlayQueueOverlays::ShowAddToPlaylistPicker(musikcore, token, { id });
    };
    const auto addCategory = [&musikcore, token](const std::string& field, const std::string& value, int64_t id) {
        PlayQueueOverlays::ShowAddCategoryToPlaylistPicker(musikcore, token, field, value, id);
    };
    this->browseLayout->SetAddToPlaylistCallbacks(addTrack, addCategory);
    this->trackSearchLayout->SetAddToPlaylistCallbacks(addTrack, addCategory);

    this->transportView = std::make_shared<TransportWindow>(musikcore);

    this->AddWindow(this->transportView);
    this->LoadLastSession();
}

void LibraryLayout::LoadLastSession() {
    if (this->prefs.GetBool(MC_PREFERENCE_KEY_SAVE_SESSION_ON_EXIT, true)) {
        auto session = api::Preferences { MC_PREFERENCE_SESSION };
        const std::string type = session.GetString(keys::LastLibraryView, type::Browse);
        if (type == type::Directory) {
            const std::string lastDirectoryRoot =
                session.GetString(keys::LastBrowseDirectoryRoot, "");

            const auto paths = this->musikcore.GetLibraryStatus().kind == MC_LIBRARY_LOCAL
                ? api::Indexer(this->musikcore).Paths()
                : std::vector<std::string> {};
            for (auto p : paths) {
                if (p == lastDirectoryRoot) {
                    this->ShowDirectories(p);
                    return;
                }
            }

            this->ShowBrowse();
        }
        else if (type == type::CategoryFilter) {
            this->ShowCategorySearch();
            this->categorySearchLayout->LoadLastSession();
        }
        else if (type == type::TrackFilter) {
            this->ShowTrackSearch();
            this->trackSearchLayout->LoadLastSession();
        }
        else if (type == type::NowPlaying) {
            this->ShowNowPlaying();
        }
        else /*if (type == type::Browse)*/ {
            this->ShowBrowse();
            this->browseLayout->LoadLastSession();
        }
    }
    else {
        this->ShowBrowse();
    }
}

void LibraryLayout::SetShortcutsWindow(ShortcutsWindow* shortcuts) {
    this->shortcuts = shortcuts;

    if (this->shortcuts) {
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryBrowse), _TSTR("shortcuts_browse"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryFilter), _TSTR("shortcuts_filter"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryTracks), _TSTR("shortcuts_tracks"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateLibraryPlayQueue), _TSTR("shortcuts_play_queue"));
        this->shortcuts->AddShortcut(Hotkeys::Get(Hotkeys::NavigateSettings), _TSTR("shortcuts_settings"));
        this->shortcuts->AddShortcut(App::Instance().GetQuitKey(), _TSTR("shortcuts_quit"));

        this->shortcuts->SetChangedCallback([this](std::string key) {
            if (Hotkeys::Is(Hotkeys::NavigateSettings, key)) {
                this->Broadcast(message::JumpToSettings);
            }
            else if (key == App::Instance().GetQuitKey()) {
                App::Instance().Quit();
            }
            else {
                this->KeyPress(key);
            }
        });

        this->UpdateShortcutsWindow();
    }
}

void LibraryLayout::UpdateShortcutsWindow() {
    if (this->shortcuts) {
        if (this->shortcuts->IsFocused() && this->visibleLayout) {
            this->visibleLayout->SetFocus(IWindowPtr());
        }
        if (this->visibleLayout == this->browseLayout ||
            this->visibleLayout == this->directoryLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryBrowse));
        }
        else if (this->visibleLayout == nowPlayingLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryPlayQueue));
        }
        else if (this->visibleLayout == categorySearchLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryFilter));
        }
        else if (this->visibleLayout == trackSearchLayout) {
            this->shortcuts->SetActive(Hotkeys::Get(Hotkeys::NavigateLibraryTracks));
        }
    }
}

void LibraryLayout::OnAddedToParent(IWindow* parent) {
    MessageQueue().RegisterForBroadcasts(this->shared_from_this());
}

void LibraryLayout::OnRemovedFromParent(IWindow* parent) {
    MessageQueue().UnregisterForBroadcasts(this);
}

void LibraryLayout::OnCategorySearchResultSelected(
    CategorySearchLayout* layout, std::string fieldType, int64_t fieldId)
{
    this->ShowBrowse();
    this->browseLayout->ScrollTo(fieldType, fieldId);
}

IWindowPtr LibraryLayout::FocusNext() {
    if (this->transportView->IsFocused()) {
        if (this->transportView->FocusNext()) {
            return this->transportView;
        }

        return this->visibleLayout->FocusFirst();
    }

    return this->visibleLayout->FocusNext();
}

IWindowPtr LibraryLayout::FocusPrev() {
    if (this->transportView->IsFocused()) {
        if (this->transportView->FocusPrev()) {
            return this->transportView;
        }

        return this->visibleLayout->FocusLast();
    }

    return this->visibleLayout->FocusPrev();
}

void LibraryLayout::OnMainLayoutFocusTerminated(
    LayoutBase::FocusDirection direction)
{
    if (direction == LayoutBase::FocusForward) {
        this->transportView->FocusFirst();
    }
    else {
        this->transportView->FocusLast();
    }
}

IWindowPtr LibraryLayout::GetFocus() {
    if (this->transportView->IsFocused()) {
        return this->transportView;
    }

    auto result = this->visibleLayout->GetFocus();

    if (!result) {
        this->visibleLayout->SetFocusIndex(0, false);
        result = this->visibleLayout->GetFocus();
    }

    return result;
}

bool LibraryLayout::SetFocus(cursespp::IWindowPtr window) {
    if (window == this->transportView) {
        this->transportView->RestoreFocus();
        return true;
    }

    return this->visibleLayout->SetFocus(window);
}

void LibraryLayout::ProcessMessage(musik::core::runtime::IMessage& message) {
    switch (message.Type()) {
        case message::JumpToCategory: {
            static std::map<int, const char*> JUMP_TYPE_TO_COLUMN = {
                { cube::message::category::Album, MC_TRACK_FIELD_ALBUM },
                { cube::message::category::Artist, MC_TRACK_FIELD_ARTIST },
                { cube::message::category::AlbumArtist, MC_TRACK_FIELD_ALBUM_ARTIST },
                { cube::message::category::Genre, MC_TRACK_FIELD_GENRE }
            };

            auto type = JUMP_TYPE_TO_COLUMN[(int) message.UserData1()];
            const auto id = message.UserData2();
            this->OnCategorySearchResultSelected(nullptr, type, id);
        }
        break;

        default:
            break;
    }

    LayoutBase::ProcessMessage(message);
}

void LibraryLayout::OnPlaylistChanged(mc_playlist_event event, int64_t playlistId) {
    this->browseLayout->OnPlaylistChanged(event, playlistId);
}

void LibraryLayout::OnIndexerEvent(mc_indexer_event event, int updates) {
    if (event == MC_INDEXER_PROGRESS || event == MC_INDEXER_FINISHED) {
        this->browseLayout->OnIndexerProgress(updates);
        this->directoryLayout->OnIndexerProgress(updates);
    }
    if (event == MC_INDEXER_FINISHED) {
        this->trackSearchLayout->OnIndexerFinished(updates);
    }
}

void LibraryLayout::ShowDirectoryChooser() {
    BrowseOverlays::ShowDirectoryChooser(
        this->musikcore, this->playlistWork.CurrentToken(),
        [this](std::string directory) {
            this->ShowDirectories(directory);
        });
}

void LibraryLayout::ShowCategoryChooser() {
    BrowseOverlays::ShowCategoryChooser(
        this->musikcore, this->playlistWork.CurrentToken(),
        [this](std::string category, std::string type) {
            if (type == MagicConstants::DirectoryCategoryType) {
                this->ShowDirectoryChooser();
            }
            else {
                this->ShowBrowse(category);
            }
        });
}

bool LibraryLayout::KeyPress(const std::string& key) {
    if (this->visibleLayout == this->browseLayout ||
        this->visibleLayout == this->directoryLayout) {
        if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseArtists, key)) {
            this->ShowBrowse(MC_TRACK_FIELD_ARTIST);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbums, key)) {
            this->ShowBrowse(MC_TRACK_FIELD_ALBUM);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseGenres, key)) {
            this->ShowBrowse(MC_TRACK_FIELD_GENRE);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbumArtists, key)) {
            this->ShowBrowse(MC_TRACK_FIELD_ALBUM_ARTIST);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowsePlaylists, key)) {
            this->ShowBrowse(MC_LIBRARY_CATEGORY_PLAYLISTS);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseChooseCategory, key)) {
            this->ShowCategoryChooser();
            return true;
        }
    }

    if (Hotkeys::Is(Hotkeys::TrackListPlayFromTop, key)) {
        /* TODO: maybe have an IPlayFromTopLayout? meh... */
        if (this->visibleLayout == this->browseLayout) {
            this->browseLayout->PlayFromTop();
        }
        else if (this->visibleLayout == this->directoryLayout) {
            this->directoryLayout->PlayFromTop();
        }
        else if (this->visibleLayout == this->trackSearchLayout) {
            this->trackSearchLayout->PlayFromTop();
        }
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryPlayQueue, key)) {
        this->ShowNowPlaying();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowse, key)) {
        if (this->lastBrowseCategoryType == MagicConstants::DirectoryCategoryType) {
            this->ShowDirectories();
        }
        else {
            this->ShowBrowse();
        }
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryFilter, key)) {
        this->ShowCategorySearch();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryTracks, key)) {
        this->ShowTrackSearch();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseDirectories, key)) {
        this->ShowDirectoryChooser();
        return true;
    }
    else if (this->GetFocus() == this->transportView && Hotkeys::Is(Hotkeys::Up, key)) {
        this->visibleLayout->FocusLast();
        return true;
    }
    else if (this->GetFocus() == this->transportView && Hotkeys::Is(Hotkeys::Down, key)) {
        this->visibleLayout->FocusFirst();
        return true;
    }
    /* forward to the visible layout */
    else if (this->visibleLayout && this->visibleLayout->KeyPress(key)) {
        return true;
    }
    else if (key == " " || key == "M- ") {
        this->playback.Control(MC_PLAYBACK_TOGGLE_PAUSE);
        return true;
    }

    return LayoutBase::KeyPress(key);
}
