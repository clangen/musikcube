//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/CategoryTrackListQuery.h>
#include <musikcore/library/query/SavePlaylistQuery.h>
#include <musikcore/library/query/util/TrackSort.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Messages.h>
#include <musikcore/i18n/Locale.h>

#include <app/util/Hotkeys.h>
#include <app/util/Playback.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/Messages.h>
#include <app/util/WindowUtil.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/TrackOverlays.h>

#include "BrowseLayout.h"

using namespace musik;
using namespace musik::core::library::constants;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library::query;
using namespace musik::core::library;
using namespace musik::cube;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;
namespace components = musik::core::prefs::components;

static int kMaxCategoryWidth = 40;
static int kMinListTitleHeight = 26;

#define DEFAULT_CATEGORY constants::Track::ARTIST
#define DEFAULT_CATEGORY_NAME FIELD_TO_TITLE[DEFAULT_CATEGORY]

static std::set<std::string> kEditKeys;

static std::map <std::string, std::string> FIELD_TO_TITLE {
    std::make_pair(constants::Track::ARTIST, "browse_title_artists"),
    std::make_pair(constants::Track::ALBUM, "browse_title_albums"),
    std::make_pair(constants::Track::GENRE, "browse_title_genres"),
    std::make_pair(constants::Track::ALBUM_ARTIST, "browse_title_album_artists"),
    std::make_pair(constants::Playlists::TABLE_NAME, "browse_title_playlists")
};

static inline std::string getModifiedText() {
    try {
        return u8fmt(
            _TSTR("browse_playlist_modified"),
            Hotkeys::Get(Hotkeys::PlayQueuePlaylistSave).c_str());
    }
    catch (...) {
        return _TSTR("browse_playlist_modified");
    }
}

static inline std::string getTitleForCategory(const std::string& fieldName) {
    return FIELD_TO_TITLE.find(fieldName) == FIELD_TO_TITLE.end()
        ? _TSTR(fieldName) : _TSTR(FIELD_TO_TITLE[fieldName]);
}

static TrackSortType getDefaultTrackSort(std::shared_ptr<musik::core::Preferences> prefs) {
    return (TrackSortType)prefs->GetInt(
        keys::CategoryTrackListSortOrder,
        static_cast<int>(TrackSortType::Album));
}

BrowseLayout::BrowseLayout(
    musik::core::audio::PlaybackService& playback,
    ILibraryPtr library)
: LayoutBase()
, playlistModified(false)
, playback(playback) {
    kEditKeys = {
        Hotkeys::Get(Hotkeys::PlayQueueMoveUp),
        Hotkeys::Get(Hotkeys::PlayQueueMoveDown),
        Hotkeys::Get(Hotkeys::PlayQueueDelete)
    };

    this->library = library;
    this->library->Indexer()->Progress.connect(this, &BrowseLayout::OnIndexerProgress);
    this->library->Indexer()->Finished.connect(this, &BrowseLayout::OnIndexerProgress);
    this->prefs = Preferences::ForComponent(components::Settings);
    this->InitializeWindows();
}

BrowseLayout::~BrowseLayout() {
    this->SaveSession();
}

void BrowseLayout::OnLayout() {
    const bool showFilter = this->showCategoryListFilter;
    const int cx = this->GetWidth();
    const int x = 0;
    int cy = this->GetHeight();
    int y = 0;

    const int categoryWidth = std::min(kMaxCategoryWidth, cx / 4);

    const int categoryListBottomMargin = showFilter ? 1 : 0;
    this->categoryList->MoveAndResize(x, y, categoryWidth, cy - categoryListBottomMargin);

    if (showFilter) {
        bool refocusFilter = !this->categoryListFilter->IsVisible();
        this->categoryListFilter->Show();
        this->categoryListFilter->MoveAndResize(x + 1, cy - 1, categoryWidth - 2, 1);
        if (refocusFilter) {
            /* needs to be done during a subsequent tick in the event loop, as the
            widget isn't yet visible so can't receive focus. */
            this->Post(cube::message::FocusBrowseFilter);
        }
    }
    else {
        this->categoryListFilter->Hide();
    }

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
	this->categoryListFilter->SetFocusOrder(1);
    this->trackList->SetFocusOrder(2);
}

void BrowseLayout::InitializeWindows() {
    this->categoryList = std::make_shared<CategoryListView>(this->playback, this->library, DEFAULT_CATEGORY);
    this->categoryList->MouseEvent.connect(this, &BrowseLayout::OnWindowMouseEvent);
    this->categoryList->SetFrameTitle(_TSTR(DEFAULT_CATEGORY_NAME));

    this->categoryListFilter = std::make_shared<TextInput>(TextInput::StyleLine);
    this->categoryListFilter->SetHint("filter");

    this->trackList = std::make_shared<TrackListView>(this->playback, this->library);
    this->trackList->MouseEvent.connect(this, &BrowseLayout::OnWindowMouseEvent);
    this->trackList->Requeried.connect(this, &BrowseLayout::OnRequeried);

    this->modifiedLabel = std::make_shared<TextLabel>();
    this->modifiedLabel->SetText(getModifiedText(), text::AlignCenter);
    this->modifiedLabel->SetContentColor(Color::Banner);
    this->modifiedLabel->Hide();

    this->AddWindow(this->categoryList);
    this->AddWindow(this->categoryListFilter);
    this->AddWindow(this->trackList);
    this->AddWindow(this->modifiedLabel);

    this->categoryList->SelectionChanged.connect(
        this, &BrowseLayout::OnCategoryViewSelectionChanged);

    this->categoryList->Invalidated.connect(
        this, &BrowseLayout::OnCategoryViewInvalidated);
}

void BrowseLayout::LoadLastSession() {
    auto session = Preferences::ForComponent(components::Session);
    const std::string field = session->GetString(keys::LastBrowseCategoryType, "");
    const int64_t id = static_cast<int64_t>(session->GetDouble(keys::LastBrowseCategoryId, -1.0));
    if (field.size()) {
        this->SwitchCategory(field);
        this->ScrollTo(field, id);
    }
}

void BrowseLayout::SaveSession() {
    auto session = Preferences::ForComponent(components::Session);
    const std::string type = this->categoryList->GetFieldName().c_str();
    const double id = static_cast<double>(this->categoryList->GetSelectedId());
    session->SetString(keys::LastBrowseCategoryType, type.c_str());
    session->SetDouble(keys::LastBrowseCategoryId, id);
}

void BrowseLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    switch (message.Type()) {
        case cube::message::IndexerProgress: {
            const int64_t id = this->categoryList->GetSelectedId();
            auto filter = this->categoryList->GetFilter();
            this->categoryList->Requery(filter, id);
            break;
        }

        case core::message::PlaylistModified: {
            if (this->IsPlaylist() &&
                this->categoryList->GetSelectedId() == message.UserData1())
            {
                this->RequeryTrackList(this->categoryList.get());
            }
            break;
        }

        case core::message::PlaylistCreated:
        case core::message::PlaylistDeleted:
        case core::message::PlaylistRenamed: {
            if (this->IsPlaylist()) {
                const auto lastId = this->categoryList->GetSelectedId();
                this->categoryList->Requery(this->categoryList->GetFilter(), lastId);
            }
            break;
        }

        case cube::message::FocusBrowseFilter: {
            this->SetFocus(this->categoryListFilter);
            break;
        }

        default:
            break;
    }

    LayoutBase::ProcessMessage(message);
}

void BrowseLayout::ScrollTo(const std::string& fieldType, int64_t fieldId) {
    this->SetFocus(this->trackList);
    this->categoryList->RequeryWithField(fieldType, "", fieldId);
    this->categoryList->SetFrameTitle(getTitleForCategory(fieldType));
}

void BrowseLayout::OnIndexerProgress(int count) {
    this->Post(message::IndexerProgress);
}

void BrowseLayout::OnRequeried(TrackListQueryBase* query) {
    std::string sortByDisplayString = kTrackListOrderByToDisplayKey
        .find(getDefaultTrackSort(this->prefs))->second;

    this->trackList->SetFrameTitle(u8fmt(
        _TSTR("browse_title_tracks"),
        _TSTR(sortByDisplayString).c_str()));
}

void BrowseLayout::RequeryTrackList(ListWindow *view) {
    if (view == this->categoryList.get()) {
        const int64_t selectedId = this->categoryList->GetSelectedId();
        if (selectedId != -1) {
            const TrackSortType sortOrder = getDefaultTrackSort(this->prefs);
            auto column = this->categoryList->GetFieldName();
            this->trackList->Requery(
                std::make_shared<CategoryTrackListQuery>(
                    this->library,
                    column,
                    selectedId,
                    "",
                    sortOrder));
        }
        else {
            this->trackList->Reset();
        }

        this->ShowModifiedLabel(false);
    }
}

void BrowseLayout::OnWindowMouseEvent(Window* window, const IMouseHandler::Event* mouseEvent) {
    if (windowutil::WasHeaderClicked(window, mouseEvent)) {
        if (window == this->categoryList.get()) {
            if (this->categoryListHeaderClickHandler) {
                this->categoryListHeaderClickHandler();
            }
        }
        else if (window == this->trackList.get()) {
            this->ShowTrackSortOverlay();
        }
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

    this->trackList->SetTrackNumType(fieldName == constants::Playlists::TABLE_NAME
        ? TrackRowRenderers::TrackNumType::Sequential
        : TrackRowRenderers::TrackNumType::Metadata);
}

void BrowseLayout::PlayFromTop() {
    playback::PlayFromTop(*this->trackList, this->playback);
}

void BrowseLayout::SetOnHeaderClicked(HeaderClickHandler handler) {
    this->categoryListHeaderClickHandler = handler;
}

void BrowseLayout::ShowTrackSortOverlay() {
    TrackOverlays::ShowTrackSearchSortOverlay(
        getDefaultTrackSort(this->prefs),
        kTrackListOrderByToDisplayKey,
        [this](TrackSortType type) {
            this->prefs->SetInt(keys::CategoryTrackListSortOrder, static_cast<int>(type));
            this->RequeryTrackList(this->categoryList.get());
        });
}

bool BrowseLayout::KeyPress(const std::string& key) {
    if (key == "^[" && this->showCategoryListFilter) {
        if (this->GetFocus() == this->categoryList ||
            this->GetFocus() == this->categoryListFilter)
        {
            this->showCategoryListFilter = false;
            this->Layout();
            return true;
        }
    }
    else if (key == "KEY_ENTER") {
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
	else if (Hotkeys::Is(Hotkeys::BrowseCategoryFilter, key)) {
        if (this->GetFocus() == this->categoryList ||
            this->GetFocus() == this->categoryListFilter)
        {
            this->showCategoryListFilter = !this->showCategoryListFilter;
            this->Layout();
            return true;
        }
	}
    else if (Hotkeys::Is(Hotkeys::TrackListChangeSortOrder, key)) {
        this->ShowTrackSortOverlay();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::ViewRefresh, key)) {
        this->categoryList->Requery();
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
            auto tracks = this->trackList->GetTrackList();
            this->library->Enqueue(
                SavePlaylistQuery::Replace(
                    this->library,
                    this->categoryList->GetSelectedId(),
                    std::make_shared<TrackList>(tracks)));
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::BrowsePlaylistsRename, key)) {
            const auto id = this->categoryList->GetSelectedId();
            if (id != -1) {
                PlayQueueOverlays::ShowRenamePlaylistOverlay(
                    library, id, this->categoryList->GetSelectedValue(),
                    [this, id](auto query) {
                        this->categoryList->Requery(this->categoryList->GetFilter(), id);
                    });
            }
            return true;
        }
        else if (Hotkeys::Is(Hotkeys::BrowsePlaylistsDelete, key)) {
            if (this->GetFocus() == this->categoryList) {
                const auto id = this->categoryList->GetSelectedId();
                if (id != -1) {
                    PlayQueueOverlays::ShowConfirmDeletePlaylistOverlay(
                        library, this->categoryList->GetSelectedValue(), id,
                        [this](auto query) {
                            this->categoryList->Requery();
                        });
                }
            }
            return true;
        }
    }

    return false;
}

bool BrowseLayout::ProcessEditOperation(const std::string& key) {
    if (this->GetFocus() != this->trackList || kEditKeys.find(key) == kEditKeys.end()) {
        return false;
    }

    if (!this->IsPlaylist()) {
        return false;
    }

    std::shared_ptr<TrackList> tracks = this->trackList->GetTrackList();
    if (tracks && kEditKeys.find(key) != kEditKeys.end()) {
        const size_t selected = this->trackList->GetSelectedTrackIndex();
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