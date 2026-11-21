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

#include <musikcore/library.h>

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <app/util/Locale.h>

#include <app/util/Hotkeys.h>
#include <app/util/PreferenceKeys.h>
#include <app/util/Messages.h>
#include <app/util/WindowUtil.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/TrackOverlays.h>

#include "BrowseLayout.h"

using namespace musik;
using namespace musik::core;
using namespace musik::cube;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;

constexpr int kMaxCategoryWidth = 40;
constexpr int kMinListTitleHeight = 26;
constexpr int kRequeryIntervalMs = 300;

#if 1
constexpr auto kFilterStyle = TextInput::StyleBox;
constexpr int kFilterHeight = 3;
constexpr int kFilterPadding = 0;
#else
constexpr auto kFilterStyle = TextInput::StyleLine;
constexpr int kFilterHeight = 1;
constexpr int kFilterPadding = 1;
#endif


#define DEFAULT_CATEGORY MC_TRACK_FIELD_ARTIST
#define DEFAULT_CATEGORY_NAME FIELD_TO_TITLE[DEFAULT_CATEGORY]

static std::map <std::string, std::string> FIELD_TO_TITLE {
    std::make_pair(MC_TRACK_FIELD_ARTIST, "browse_title_artists"),
    std::make_pair(MC_TRACK_FIELD_ALBUM, "browse_title_albums"),
    std::make_pair(MC_TRACK_FIELD_GENRE, "browse_title_genres"),
    std::make_pair(MC_TRACK_FIELD_ALBUM_ARTIST, "browse_title_album_artists"),
    std::make_pair(MC_LIBRARY_CATEGORY_PLAYLISTS, "browse_title_playlists")
};

static inline std::string getModifiedText() {
    try {
        return u8fmt(
            _TSTR("browse_playlist_modified"),
            Hotkeys::Get(Hotkeys::BrowsePlaylistsSave).c_str());
    }
    catch (...) {
        return _TSTR("browse_playlist_modified");
    }
}

static inline std::string getTitleForCategory(const std::string& fieldName) {
    return FIELD_TO_TITLE.find(fieldName) == FIELD_TO_TITLE.end()
        ? _TSTR(fieldName) : _TSTR(FIELD_TO_TITLE[fieldName]);
}

static mc_track_search_order getDefaultTrackSort(const api::Preferences& prefs) {
    const auto sort = (mc_track_search_order) prefs.GetInt(
        keys::CategoryTrackListSortOrder,
        static_cast<int>(MC_TRACK_SEARCH_ALBUM));
    return sort <= MC_TRACK_SEARCH_GENRE ? sort : MC_TRACK_SEARCH_ALBUM;
}

BrowseLayout::BrowseLayout(
    api::Context& context)
: LayoutBase()
, playlistModified(false)
, musikcore(context)
{
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

    const int categoryListBottomMargin = showFilter ? kFilterHeight : 0;
    this->categoryList->MoveAndResize(x, y, categoryWidth, cy - categoryListBottomMargin);

    if (showFilter) {
        const bool refocusFilter = !this->categoryListFilter->IsVisible();
        this->categoryListFilter->Show();
        this->categoryListFilter->MoveAndResize(
            x + kFilterPadding,
            cy - kFilterHeight,
            categoryWidth - (2 * kFilterPadding),
            kFilterHeight);
        if (refocusFilter) {
            /* needs to be done during a subsequent tick in the event loop, as the
            widget isn't yet visible so can't receive focus. */
            this->Post(cube::message::FocusBrowseFilter);
        }
    }
    else {
        this->categoryListFilter->SetText("");
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
    this->categoryList = std::make_shared<CategoryListView>(
        this->musikcore, DEFAULT_CATEGORY);
    this->categoryList->MouseEvent.connect(this, &BrowseLayout::OnWindowMouseEvent);
    this->categoryList->SetFrameTitle(_TSTR(DEFAULT_CATEGORY_NAME));

    this->categoryListFilter = std::make_shared<TextInput>(kFilterStyle);
    this->categoryListFilter->TextChanged.connect(this, &BrowseLayout::OnCategoryFilterChanged);
    this->categoryListFilter->EnterPressed.connect(this, &BrowseLayout::OnCategoryFilterEnterPressed);
    this->categoryListFilter->SetHint(_TSTR("search_filter_hint"));

    this->trackList = std::make_shared<TrackListView>(this->musikcore);
    this->trackList->MouseEvent.connect(this, &BrowseLayout::OnWindowMouseEvent);

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
    api::Preferences session { MC_PREFERENCE_SESSION };
    const std::string field = session.GetString(keys::LastBrowseCategoryType, "");
    const int64_t id = static_cast<int64_t>(session.GetDouble(keys::LastBrowseCategoryId, -1.0));
    if (field.size()) {
        this->SwitchCategory(field);
        this->ScrollTo(field, id);
    }
    this->showCategoryListFilter = session.GetBool(keys::LastBrowseFilterVisible, false);
    if (this->showCategoryListFilter) {
        const std::string filter = session.GetString(keys::LastBrowseFilter);
        if (filter != this->currentFilter) {
            this->categoryListFilter->SetText(filter);
        }
    }
}

void BrowseLayout::SaveSession() {
    api::Preferences session { MC_PREFERENCE_SESSION };
    const std::string type = this->categoryList->GetFieldName().c_str();
    const double id = static_cast<double>(this->categoryList->GetSelectedId());
    session.SetString(keys::LastBrowseCategoryType, type);
    session.SetDouble(keys::LastBrowseCategoryId, id);
    session.SetBool(keys::LastBrowseFilterVisible, this->showCategoryListFilter);
    session.SetString(keys::LastBrowseFilter, this->currentFilter);
}

void BrowseLayout::ProcessMessage(musik::core::runtime::IMessage &message) {
    switch (message.Type()) {
        case cube::message::IndexerProgress: {
            const int64_t id = this->categoryList->GetSelectedId();
            auto filter = this->categoryList->GetFilter();
            this->categoryList->Requery(filter, id);
            break;
        }

        case cube::message::FocusBrowseFilter: {
            this->SetFocus(this->categoryListFilter);
            break;
        }

        case cube::message::RequeryCategoryList: {
            this->categoryList->Requery(this->currentFilter, 0);
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

void BrowseLayout::OnPlaylistChanged(mc_playlist_event event, int64_t playlistId) {
    if (!this->IsPlaylist()) {
        return;
    }

    const auto selectedId = this->categoryList->GetSelectedId();
    if (event != MC_PLAYLIST_CONTENTS_CHANGED) {
        this->categoryList->Requery(this->categoryList->GetFilter(), selectedId);
    }
    else if (selectedId == playlistId) {
        this->RequeryTrackList(this->categoryList.get());
    }
}

void BrowseLayout::UpdateTrackTitle() {
    const auto sortName = TrackOverlays::TrackSortName(getDefaultTrackSort(this->prefs));

    this->trackList->SetFrameTitle(u8fmt(
        _TSTR("browse_title_tracks"),
        sortName.c_str()));
}

void BrowseLayout::RequeryTrackList(ListWindow *view) {
    if (view == this->categoryList.get()) {
        const int64_t selectedId = this->categoryList->GetSelectedId();
        if (selectedId != -1) {
            const mc_track_search_order sortOrder = getDefaultTrackSort(this->prefs);
            const auto column = this->categoryList->GetFieldName();
            const auto hash = std::hash<std::string>{}(column + ":" + std::to_string(selectedId) +
                ":" + std::to_string(static_cast<int>(sortOrder)));
            this->trackList->RequeryTracks(hash, [column, selectedId, sortOrder]
                (api::Context& context, api::TrackSearchCompletion completion) {
                    return api::OpenCategory(context, column, selectedId,
                        sortOrder, std::move(completion));
                });
            this->UpdateTrackTitle();
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

void BrowseLayout::OnCategoryFilterChanged(TextInput* sender, std::string value) {
    this->currentFilter = value;
    this->Debounce(cube::message::RequeryCategoryList, 0, 0, kRequeryIntervalMs);
}

void BrowseLayout::OnCategoryFilterEnterPressed(cursespp::TextInput* sender) {
    this->SetFocus(this->categoryList);
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

    this->trackList->SetTrackNumType(fieldName == MC_LIBRARY_CATEGORY_PLAYLISTS
        ? TrackRowRenderers::TrackNumType::Sequential
        : TrackRowRenderers::TrackNumType::Metadata);
}

void BrowseLayout::PlayFromTop() {
    this->trackList->PlayRow(0);
}

void BrowseLayout::SetOnHeaderClicked(HeaderClickHandler handler) {
    this->categoryListHeaderClickHandler = handler;
}

void BrowseLayout::ShowTrackSortOverlay() {
    TrackOverlays::ShowTrackSearchSortOverlay(
        getDefaultTrackSort(this->prefs),
        [this](mc_track_search_order type) {
            this->prefs.SetInt(keys::CategoryTrackListSortOrder, static_cast<int>(type));
            this->RequeryTrackList(this->categoryList.get());
        });
}

bool BrowseLayout::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        /* if the tracklist is NOT focused (i.e. the focus is on a
        category window), start playback from the top. */
        if (this->GetFocus() != this->trackList) {
            return this->trackList->PlayRow(0);
        }
    }
	else if (Hotkeys::Is(Hotkeys::BrowseCategoryFilter, key)) {
        this->showCategoryListFilter = !this->showCategoryListFilter;
        this->Layout();
        return true;
    }
    else if (
        this->categoryList->GetFieldName() != MC_LIBRARY_CATEGORY_PLAYLISTS &&
        Hotkeys::Is(Hotkeys::TrackListChangeSortOrder, key))
    {
        this->ShowTrackSortOverlay();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::ViewRefresh, key)) {
        this->categoryList->Requery();
        return true;
    }
    else if (this->ProcessPlaylistOperation(key)) {
        return true;
    }
    else if (this->ProcessEditOperation(key)) {
        return true;
    }

    return LayoutBase::KeyPress(key);
}

bool BrowseLayout::IsPlaylist() {
    return this->categoryList->GetFieldName() == MC_LIBRARY_CATEGORY_PLAYLISTS;
}

bool BrowseLayout::ProcessPlaylistOperation(const std::string& key) {
    if (!this->IsPlaylist()) {
        return false;
    }

    const auto selected = this->categoryList->GetSelectedId();
    const auto token = this->playlistWork.CurrentToken();
    const auto refresh = [this](int64_t id) {
        this->ShowModifiedLabel(false);
        this->categoryList->Requery(this->categoryList->GetFilter(), id);
    };

    if (Hotkeys::Is(Hotkeys::BrowsePlaylistsNew, key)) {
        PlayQueueOverlays::ShowSavePlaylistOverlay(this->musikcore, token, {}, {}, refresh);
        return true;
    }
    if (Hotkeys::Is(Hotkeys::BrowsePlaylistsSave, key) && selected > 0) {
        if (!this->trackList->HasTrackResult()) {
            return true;
        }
        try {
            auto ids = this->trackList->TrackIds();
            auto saved = this->playlistWork.Wrap([this, selected, ids](mc_result status, int64_t, std::string_view error) {
                if (status != MC_OK) {
                    musik::core::api::WriteLog(MC_LOG_ERROR, "BrowseLayout", std::string(error));
                }
                else if (this->categoryList->GetSelectedId() == selected && this->trackList->TrackIds() == ids) {
                    this->ShowModifiedLabel(false);
                }
            });
            api::SavePlaylist(this->musikcore, selected, {}, ids, std::move(saved));
        }
        catch (const api::Error& error) {
            musik::core::api::WriteLog(MC_LOG_ERROR, "BrowseLayout", error.what());
        }
        return true;
    }
    if (this->GetFocus() != this->categoryList || selected <= 0) {
        return false;
    }

    const api::PlaylistEntry playlist { selected, this->categoryList->GetSelectedValue() };
    if (Hotkeys::Is(Hotkeys::BrowsePlaylistsRename, key)) {
        PlayQueueOverlays::ShowRenamePlaylistOverlay(this->musikcore, token, playlist, refresh);
        return true;
    }
    if (Hotkeys::Is(Hotkeys::BrowsePlaylistsDelete, key)) {
        PlayQueueOverlays::ShowDeletePlaylistOverlay(this->musikcore, token, playlist,
            [this](int64_t) { this->categoryList->Requery(); });
        return true;
    }

    return false;
}

bool BrowseLayout::ProcessEditOperation(const std::string& key) {
    if (this->GetFocus() != this->trackList || !this->IsPlaylist()) {
        return false;
    }

    const bool up = Hotkeys::Is(Hotkeys::PlayQueueMoveUp, key);
    const bool down = Hotkeys::Is(Hotkeys::PlayQueueMoveDown, key);
    const bool erase = Hotkeys::Is(Hotkeys::PlayQueueDelete, key);
    if (!up && !down && !erase) {
        return false;
    }

    auto ids = this->trackList->TrackIds();
    const auto selected = this->trackList->GetSelectedTrackIndex();
    if (selected >= ids.size()) {
        return false;
    }

    auto to = selected;
    if (erase) {
        ids.erase(ids.begin() + selected);
    }
    else if (up && selected > 0) {
        --to;
        std::swap(ids[selected], ids[to]);
    }
    else if (down && selected + 1 < ids.size()) {
        ++to;
        std::swap(ids[selected], ids[to]);
    }
    else {
        return true;
    }

    this->trackList->SetEditedTrackIds(ids);
    this->ShowModifiedLabel(true);
    if (!ids.empty()) {
        to = this->trackList->TrackIndexToAdapterIndex(std::min(ids.size() - 1, to));
        if (!this->trackList->IsEntryVisible(to)) {
            this->trackList->ScrollTo(to);
        }
        this->trackList->SetSelectedIndex(to);
    }
    return true;
}

void BrowseLayout::ShowModifiedLabel(bool show) {
    if (show != this->playlistModified) {
        this->playlistModified = show;
        this->Layout();
    }
}
