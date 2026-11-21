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

#include <musikcore/library.h>
#include <cursespp/Colors.h>

#include <cursespp/SingleLineEntry.h>
#include <cursespp/MultiLineEntry.h>
#include <cursespp/Text.h>

#include <musikcore/runtime/IMessage.h>

#include <app/util/Hotkeys.h>
#include <app/overlay/PlayQueueOverlays.h>

#include "CategoryListView.h"

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace cursespp;

static std::map<std::string, std::string> FIELD_TO_ID_MAP = {
    { MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_ALBUM_ID },
    { MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_VISUAL_ARTIST_ID },
    { MC_TRACK_FIELD_ALBUM_ARTIST, MC_TRACK_FIELD_ALBUM_ARTIST_ID },
    { MC_TRACK_FIELD_GENRE,  MC_TRACK_FIELD_VISUAL_GENRE_ID }
};

static std::string getFieldIdColumn(const std::string& fieldId) {
    auto id = FIELD_TO_ID_MAP.find(fieldId);
    return id == FIELD_TO_ID_MAP.end() ? "" : id->second;
}

CategoryListView::CategoryListView(
    api::Context& musikcore,
    const std::string& fieldName)
: ListWindow(nullptr)
, musikcore(musikcore)
, playback(musikcore.GetPlayback())
{
    this->selectAfterQuery = -1LL;
    this->fieldName = fieldName;
    this->fieldIdColumn = getFieldIdColumn(fieldName);
    this->adapter = new Adapter(*this);
    this->playback.SetQueueChangedCallback([](void* data) {
        static_cast<CategoryListView*>(data)->RefreshPlayingCategory();
    }, this);
    this->RefreshPlayingCategory();
}

CategoryListView::~CategoryListView() {
    this->playback.SetQueueChangedCallback(nullptr);
    this->CancelQuery();
    this->playingWork.Invalidate();
    if (this->playingQuery) {
        this->musikcore.Cancel(this->playingQuery);
    }
    delete adapter;
}

void CategoryListView::OnVisibilityChanged(bool visible) {
    ListWindow::OnVisibilityChanged(visible);

    this->RefreshPlayingCategory();
    if (visible && !this->activeQuery) {
        this->Requery(this->GetSelectedId());
    }
}

void CategoryListView::OnParentVisibilityChanged(bool visible) {
    ListWindow::OnParentVisibilityChanged(visible);
    this->RefreshPlayingCategory();
}

void CategoryListView::RequeryWithField(
    mc_track_search_mode matchType,
    const std::string& fieldName,
    const std::string& filter,
    const int64_t selectAfterQuery)
{
    this->CancelQuery();

    this->matchType = matchType;
    if (this->fieldName != fieldName) {
        this->fieldName = fieldName;
        this->fieldIdColumn = getFieldIdColumn(fieldName);
        this->RefreshPlayingCategory();
    }
    this->selectAfterQuery = selectAfterQuery;
    this->filter = filter;
    try {
        this->activeQuery = api::ListCategories(this->musikcore, fieldName, filter, matchType,
            this->categoryWork.Wrap([this](mc_result status, std::vector<api::CategoryEntry> categories, std::string_view) {
                this->activeQuery = 0;
                if (status == MC_OK) {
                    this->ApplyCategories(std::move(categories));
                }
            }));
    }
    catch (const api::Error& error) {
        if (error.Result() == MC_ERROR_NOT_FOUND) {
            this->ApplyCategories({});
        }
    }
}

void CategoryListView::RequeryWithField(
    const std::string& fieldName,
    const std::string& filter,
    const int64_t selectAfterQuery)
{
    this->RequeryWithField(this->matchType, fieldName, filter, selectAfterQuery);
}

void CategoryListView::Requery(
    mc_track_search_mode matchType,
    const std::string& filter,
    const int64_t selectAfterQuery)
{
    this->RequeryWithField(matchType, this->fieldName, filter, selectAfterQuery);
}

void CategoryListView::Requery(const std::string& filter, const int64_t selectAfterQuery) {
    this->RequeryWithField(this->matchType, this->fieldName, filter, selectAfterQuery);
}

void CategoryListView::Requery(const int64_t selectAfterQuery) {
    this->RequeryWithField(this->matchType, this->fieldName, this->filter, selectAfterQuery);
}

void CategoryListView::Requery() {
    this->RequeryWithField(this->matchType, this->fieldName, this->filter, -1LL);
}

void CategoryListView::Reset() {
    this->CancelQuery();
    this->metadata.clear();
    this->OnAdapterChanged();
}

int64_t CategoryListView::GetSelectedId() {
    size_t index = this->GetSelectedIndex();
    if (index != NO_SELECTION && index < this->metadata.size()) {
        return this->metadata[index].id;
    }
    return this->metadata.empty() ? this->selectAfterQuery : -1LL;
}

std::string CategoryListView::GetSelectedValue() {
    size_t index = this->GetSelectedIndex();
    if (index != NO_SELECTION && index < this->metadata.size()) {
        return this->metadata[index].value;
    }
    return "";
}

std::string CategoryListView::GetFieldName() {
    return this->fieldName;
}

std::string CategoryListView::GetFilter() {
    return this->filter;
}

void CategoryListView::RefreshPlayingCategory() {
    this->playingWork.Invalidate();
    if (this->playingQuery) {
        this->musikcore.Cancel(std::exchange(this->playingQuery, 0));
    }
    this->playingCategoryId.reset();
    this->playingCategoryName.clear();
    this->OnAdapterChanged();

    /* Read the category from the playing track's library, even if selection changes. */
    try {
        const auto track = this->playback.CurrentTrack();
        if (!track || !this->IsVisible() || !this->IsParentVisible()) {
            return;
        }

        const auto& field = this->fieldIdColumn.empty() ? this->fieldName : this->fieldIdColumn;
        this->playingQuery = api::ReadTrack(this->musikcore, *track, { field },
            this->playingWork.Wrap([this](mc_result status, api::TrackPage page, std::string_view) {
                this->playingQuery = 0;
                if (status != MC_OK || !page.Size()) {
                    return;
                }

                const auto track = page.Track(0);
                if (this->fieldIdColumn.empty()) {
                    this->playingCategoryName = track.Value(this->fieldName);
                }
                else {
                    const auto value = track.Value(this->fieldIdColumn);
                    if (!value.empty()) {
                        int64_t id = 0;
                        const auto end = value.data() + value.size();
                        const auto parsed = std::from_chars(value.data(), end, id);
                        if (parsed.ec == std::errc{} && parsed.ptr == end) {
                            this->playingCategoryId = id;
                        }
                    }
                }
                this->OnAdapterChanged();
            }));
    }
    catch (const api::Error& error) {
        musik::debug::error("CategoryListView", error.what());
    }
}

bool CategoryListView::IsPlayingCategory(const api::CategoryEntry& entry) const {
    return this->fieldIdColumn.empty()
        ? !this->playingCategoryName.empty() && entry.value == this->playingCategoryName
        : this->playingCategoryId == entry.id;
}

void CategoryListView::SetFieldName(const std::string& fieldName) {
    if (this->fieldName != fieldName) {
        this->selectAfterQuery = -1LL;
        this->fieldName = fieldName;
        this->SetSelectedIndex(0);
        this->ScrollToTop();

        this->metadata.clear();

        this->Requery(this->matchType, this->filter);
    }
}

void CategoryListView::ScrollToPlaying() {
    const auto playing = std::find_if(this->metadata.begin(), this->metadata.end(),
        [this](const auto& entry) { return this->IsPlayingCategory(entry); });
    if (playing != this->metadata.end()) {
        const auto selected = static_cast<size_t>(playing - this->metadata.begin());
        this->SetSelectedIndex(selected);
        this->ScrollTo(selected);
    }
}

bool CategoryListView::KeyPress(const std::string& key) {
    if (Hotkeys::Is(Hotkeys::ContextMenu, key)) {
        this->ShowContextMenu();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateJumpToPlaying, key)) {
        this->ScrollToPlaying();
        return true;
    }

    return ListWindow::KeyPress(key);
}

bool CategoryListView::OnEntryContextMenu(size_t index) {
    ListWindow::OnEntryContextMenu(index);
    this->ShowContextMenu();
    return true;
}

void CategoryListView::ShowContextMenu() {
    int64_t id = this->GetSelectedId();

    if (id != -1LL) {
        PlayQueueOverlays::ShowAddCategoryOverlay(
            this->musikcore,
            this->categoryWork.CurrentToken(),
            this->fieldName,
            this->GetSelectedValue(),
            id, this->addToPlaylist);
    }
}

void CategoryListView::CancelQuery() {
    this->categoryWork.Invalidate();
    if (this->activeQuery) {
        this->musikcore.Cancel(std::exchange(this->activeQuery, 0));
    }
}

void CategoryListView::ApplyCategories(std::vector<api::CategoryEntry> categories) {
    this->metadata = std::move(categories);
    if (this->selectAfterQuery != -1LL) {
        const auto selected = std::find_if(this->metadata.begin(), this->metadata.end(), [this](const auto& entry) {
            return entry.id == this->selectAfterQuery;
        });
        const auto index = selected == this->metadata.end() ? 0 : std::distance(this->metadata.begin(), selected);
        this->SetSelectedIndex(index);
        if (!this->IsEntryVisible(index)) {
            this->ScrollTo(index == 0 ? 0 : index - 1);
        }
    }
    this->OnAdapterChanged();
    this->OnInvalidated();
}

IScrollAdapter& CategoryListView::GetScrollAdapter() {
    return *adapter;
}

CategoryListView::Adapter::Adapter(CategoryListView &parent)
    : parent(parent) {
}

size_t CategoryListView::Adapter::GetEntryCount() {
    return parent.metadata.size();
}

IScrollAdapter::EntryPtr CategoryListView::Adapter::GetEntry(cursespp::ScrollableWindow* window, size_t index) {
    std::string value = parent.metadata[index].value;

    const bool playing = parent.IsPlayingCategory(parent.metadata[index]);

    bool selected = (index == parent.GetSelectedIndex());
    Color attrs = selected ? Color::ListItemHighlighted : Color::Default;

    if (playing) {
        if (selected) {
            attrs = Color::ListItemHighlightedSelected;
        }
        else {
            attrs = Color::ListItemSelected;
        }
    }

    value = text::Ellipsize(value, this->GetWidth());

    std::shared_ptr<SingleLineEntry> entry(new SingleLineEntry(value));
    entry->SetAttrs(attrs);
    return entry;
}
