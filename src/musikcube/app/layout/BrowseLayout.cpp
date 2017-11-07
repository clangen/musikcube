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

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/library/query/local/CategoryTrackListQuery.h>
#include <core/library/query/local/SavePlaylistQuery.h>
#include <core/i18n/Locale.h>
#include <app/util/Hotkeys.h>
#include <app/util/Playback.h>
#include <app/util/Messages.h>
#include <app/overlay/PlayQueueOverlays.h>

#include "BrowseLayout.h"

using namespace musik::core::library::constants;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db::local;
using namespace musik::core::library;
using namespace musik::cube;
using namespace cursespp;

static size_t MAX_CATEGORY_WIDTH = 40;
static int MIN_LIST_TITLE_HEIGHT = 26;

#define DEFAULT_CATEGORY constants::Track::ARTIST
#define DEFAULT_CATEGORY_NAME FIELD_TO_TITLE[DEFAULT_CATEGORY]

static std::set<std::string> EDIT_KEYS;

static std::map <std::string, std::string> FIELD_TO_TITLE{
    std::make_pair(constants::Track::ARTIST, "browse_title_artists"),
    std::make_pair(constants::Track::ALBUM, "browse_title_albums"),
    std::make_pair(constants::Track::GENRE, "browse_title_genres"),
    std::make_pair(constants::Track::ALBUM_ARTIST, "browse_title_album_artists"),
    std::make_pair(constants::Playlists::TABLE_NAME, "browse_title_playlists")
};

static inline std::string getModifiedText() {
    try {
        return boost::str(
            boost::format(_TSTR("browse_playlist_modified"))
            % Hotkeys::Get(Hotkeys::PlayQueuePlaylistSave));
    }
    catch (...) {
        return _TSTR("browse_playlist_modified");
    }
}

static inline std::string getTitleForCategory(const std::string& fieldName) {
    return FIELD_TO_TITLE.find(fieldName) == FIELD_TO_TITLE.end()
        ? _TSTR("browse_title_category") : _TSTR(FIELD_TO_TITLE[fieldName]);
}

BrowseLayout::BrowseLayout(
    musik::core::audio::PlaybackService& playback,
    ILibraryPtr library)
: LayoutBase()
, playlistModified(false)
, playback(playback) {
    EDIT_KEYS = {
        Hotkeys::Get(Hotkeys::PlayQueueMoveUp),
        Hotkeys::Get(Hotkeys::PlayQueueMoveDown),
        Hotkeys::Get(Hotkeys::PlayQueueDelete)
    };

    this->library = library;
    this->library->Indexer()->Progress.connect(this, &BrowseLayout::OnIndexerProgress);
    this->library->Indexer()->Finished.connect(this, &BrowseLayout::OnIndexerProgress);
    this->InitializeWindows();
}

BrowseLayout::~BrowseLayout() {
}

void BrowseLayout::OnLayout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();
    size_t x = 0, y = 0;

    size_t categoryWidth = std::min(MAX_CATEGORY_WIDTH, cx / 4);

    this->categoryList->MoveAndResize(x, y, categoryWidth, cy);

    if (this->playlistModified) {
        this->modifiedLabel->Show();
        this->modifiedLabel->MoveAndResize(x + categoryWidth, y, cx - categoryWidth, 1);
        ++y;
        --cy;
    }
    else {
        this->modifiedLabel->Hide();
    }

    this->trackList->MoveAndResize(x + categoryWidth, y, cx - categoryWidth, cy);

    this->categoryList->SetFocusOrder(0);
    this->trackList->SetFocusOrder(1);
}

void BrowseLayout::InitializeWindows() {
    this->categoryList.reset(new CategoryListView(this->playback, this->library, DEFAULT_CATEGORY));
    this->categoryList->SetFrameTitle(_TSTR(DEFAULT_CATEGORY_NAME));

    this->trackList.reset(new TrackListView(this->playback, this->library));
    this->trackList->SetFrameTitle(_TSTR("browse_title_tracks"));

    this->modifiedLabel.reset(new TextLabel());
    this->modifiedLabel->SetText(getModifiedText(), text::AlignCenter);
    this->modifiedLabel->SetContentColor(CURSESPP_BANNER);
    this->modifiedLabel->Hide();

    this->AddWindow(this->categoryList);
    this->AddWindow(this->trackList);
    this->AddWindow(this->modifiedLabel);

    this->categoryList->SelectionChanged.connect(
        this, &BrowseLayout::OnCategoryViewSelectionChanged);

    this->categoryList->Invalidated.connect(
        this, &BrowseLayout::OnCategoryViewInvalidated);
}

void BrowseLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    if (message.Type() == message::IndexerProgress) {
        int64_t id = this->categoryList->GetSelectedId();
        auto filter = this->categoryList->GetFilter();
        this->categoryList->Requery(filter, id);
    }
    else if (message.Type() == message::TracksAddedToPlaylist) {
        if (this->IsPlaylist() &&
            this->categoryList->GetSelectedId() == message.UserData1())
        {
            this->RequeryTrackList(this->categoryList.get());
        }
    }
    else if (message.Type() == message::PlaylistCreated) {
        if (this->IsPlaylist()) {
            auto lastId = this->categoryList->GetSelectedId();
            this->categoryList->Requery(this->categoryList->GetFilter(), lastId);
        }
    }

    LayoutBase::ProcessMessage(message);
}

void BrowseLayout::ScrollTo(const std::string& fieldType, int64_t fieldId) {
    this->SetFocus(this->trackList);
    this->categoryList->RequeryWithField(fieldType, "", fieldId);
    this->categoryList->SetFrameTitle(getTitleForCategory(fieldType));
}

void BrowseLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->categoryList->Requery();
    }
}

void BrowseLayout::OnIndexerProgress(int count) {
    this->PostMessage(message::IndexerProgress);
}

void BrowseLayout::RequeryTrackList(ListWindow *view) {
    if (view == this->categoryList.get()) {
        int64_t selectedId = this->categoryList->GetSelectedId();
        if (selectedId != -1) {
            this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
                new CategoryTrackListQuery(
                    this->library,
                    this->categoryList->GetFieldName(),
                    selectedId)));
        }
        else {
            this->trackList->Clear();
        }

        this->ShowModifiedLabel(false);
    }
}

void BrowseLayout::OnCategoryViewSelectionChanged(
    ListWindow *view, size_t newIndex, size_t oldIndex)
{
    this->RequeryTrackList(view);
}

void BrowseLayout::OnCategoryViewInvalidated(
    ListWindow *view, size_t selectedIndex)
{
    /* this can happen during sync. if the user managed to modify a
    playlist in this state, just ignore the refresh */
    if (IsPlaylist() && this->playlistModified) {
        return;
    }

    this->RequeryTrackList(view);
}

void BrowseLayout::SwitchCategory(const std::string& fieldName) {
    this->categoryList->SetFieldName(fieldName);
    this->categoryList->SetFrameTitle(getTitleForCategory(fieldName));
}

bool BrowseLayout::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        /* if the tracklist is NOT focused (i.e. the focus is on a
        category window), start playback from the top. */
        if (this->GetFocus() != this->trackList) {
            auto tracks = trackList->GetTrackList();
            if (tracks) {
                playback.Play(*tracks.get(), 0);
                return true;
            }
        }
    }
    else if (Hotkeys::Is(Hotkeys::ViewRefresh, key)) {
        this->categoryList->Requery();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseArtists, key)) {
        this->SwitchCategory(constants::Track::ARTIST);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbums, key)) {
        this->SwitchCategory(constants::Track::ALBUM);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseGenres, key)) {
        this->SwitchCategory(constants::Track::GENRE);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbumArtists, key)) {
        this->SwitchCategory(constants::Track::ALBUM_ARTIST);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowsePlaylists, key)) {
        this->SwitchCategory(constants::Playlists::TABLE_NAME);
        return true;
    }
    else if (ProcessPlaylistOperation(key)) {
        return true;
    }
    else if (this->ProcessEditOperation(key)) {
        return true;
    }

    return LayoutBase::KeyPress(key);
}

bool BrowseLayout::IsPlaylist() {
    return this->categoryList->GetFieldName() == Playlists::TABLE_NAME;
}

bool BrowseLayout::ProcessPlaylistOperation(const std::string& key) {
    if (IsPlaylist()) {
        if (Hotkeys::Is(Hotkeys::BrowsePlaylistsNew, key)) {
            /* note: no callback here, will handle this in ProcessMessage so
            we can still create new playlists from the context menu, out of
            band. */
            PlayQueueOverlays::ShowCreatePlaylistOverlay(this->MessageQueue(), library);
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::BrowsePlaylistsSave, key)) {
            this->ShowModifiedLabel(false);
            auto tracks = this->trackList->GetTrackList().get();
            this->library->Enqueue(SavePlaylistQuery::Replace(
                this->library,
                this->categoryList->GetSelectedId(),
                std::shared_ptr<TrackList>(new TrackList(tracks))));
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::BrowsePlaylistsRename, key)) {
            auto id = this->categoryList->GetSelectedId();
            if (id != -1) {
                PlayQueueOverlays::ShowRenamePlaylistOverlay(
                    library, id, this->categoryList->GetSelectedValue(),
                    [this, id](auto query) {
                        this->categoryList->Requery(this->categoryList->GetFilter(), id);
                    });
            }
        }
        else if (Hotkeys::Is(Hotkeys::BrowsePlaylistsDelete, key)) {
            if (this->GetFocus() == this->categoryList) {
                auto id = this->categoryList->GetSelectedId();
                if (id != -1) {
                    PlayQueueOverlays::ShowConfirmDeletePlaylistOverlay(
                        library, this->categoryList->GetSelectedValue(), id,
                        [this](auto query) {
                            this->categoryList->Requery();
                        });
                }
            }
        }
    }

    return false;
}

bool BrowseLayout::ProcessEditOperation(const std::string& key) {
    if (this->GetFocus() != this->trackList || EDIT_KEYS.find(key) == EDIT_KEYS.end()) {
        return false;
    }

    if (!this->IsPlaylist()) {
        return false;
    }

    std::shared_ptr<TrackList> tracks = this->trackList->GetTrackList();
    if (tracks && EDIT_KEYS.find(key) != EDIT_KEYS.end()) {
        size_t selected = this->trackList->GetSelectedTrackIndex();
        size_t to = -1;
        bool modified = this->playlistModified;

        if (selected == ListWindow::NO_SELECTION) {
            return false;
        }

        if (Hotkeys::Is(Hotkeys::PlayQueueMoveUp, key)) {
            if (selected > 0) {
                to = selected - 1;
                modified |= tracks->Move(selected, to);
            }
            else {
                to = selected;
            }
        }
        else if (Hotkeys::Is(Hotkeys::PlayQueueMoveDown, key)) {
            if (selected < tracks->Count() - 1) {
                to = selected + 1;
                modified |= tracks->Move(selected, to);
            }
            else {
                to = selected;
            }
        }
        else if (Hotkeys::Is(Hotkeys::PlayQueueDelete, key)) {
            modified |= tracks->Delete(selected);
            to = selected;
        }

        this->ShowModifiedLabel(modified);

        to = trackList->TrackIndexToAdapterIndex(to);
        trackList->OnAdapterChanged();

        if (!trackList->IsEntryVisible(to)) {
            trackList->ScrollTo(to);
        }

        to = std::min(tracks->Count() - 1, to);
        trackList->SetSelectedIndex(to);

        return true;
    }

    return false;
}

void BrowseLayout::ShowModifiedLabel(bool show) {
    if (show != this->playlistModified) {
        this->playlistModified = show;
        this->Layout();
    }
}