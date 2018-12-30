//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <cursespp/SingleLineEntry.h>
#include <cursespp/MultiLineEntry.h>
#include <cursespp/Text.h>

#include <core/runtime/IMessage.h>
#include <core/library/LocalLibraryConstants.h>

#include <app/util/Hotkeys.h>
#include <app/overlay/PlayQueueOverlays.h>

#include "CategoryListView.h"

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db;
using namespace musik::core::db::local;
using namespace musik::core::library::constants;
using namespace musik::core::runtime;
using namespace musik::cube;
using namespace cursespp;

static std::map<std::string, std::string> FIELD_TO_ID_MAP = {
    { library::constants::Track::ALBUM, library::constants::Track::ALBUM_ID },
    { library::constants::Track::ARTIST, library::constants::Track::ARTIST_ID },
    { library::constants::Track::ALBUM_ARTIST, library::constants::Track::ALBUM_ARTIST_ID },
    { library::constants::Track::GENRE,  library::constants::Track::GENRE_ID }
};

static std::string getFieldIdColumn(const std::string& fieldId) {
    auto id = FIELD_TO_ID_MAP.find(fieldId);
    return id == FIELD_TO_ID_MAP.end() ? "" : id->second;
}

CategoryListView::CategoryListView(
    musik::core::audio::PlaybackService& playback,
    ILibraryPtr library,
    const std::string& fieldName)
: ListWindow(nullptr)
, playback(playback) {
    this->selectAfterQuery = -1LL;
    this->library = library;
    this->library->QueryCompleted.connect(this, &CategoryListView::OnQueryCompleted);
    this->fieldName = fieldName;
    this->fieldIdColumn = getFieldIdColumn(fieldName);
    this->adapter = new Adapter(*this);
    this->playback.TrackChanged.connect(this, &CategoryListView::OnTrackChanged);

    size_t index = playback.GetIndex();
    if (index != ListWindow::NO_SELECTION) {
        this->playing = playback.GetTrackAtIndex(index);
    }
}

CategoryListView::~CategoryListView() {
    delete adapter;
}

void CategoryListView::RequeryWithField(
    const std::string& fieldName,
    const std::string& filter,
    const int64_t selectAfterQuery)
{
    if (this->activeQuery) {
        this->activeQuery->Cancel();
    }

    this->fieldName = fieldName;
    this->fieldIdColumn = getFieldIdColumn(fieldName);
    this->selectAfterQuery = selectAfterQuery;
    this->filter = filter;
    this->activeQuery.reset(new CategoryListQuery(fieldName, filter));
    this->library->Enqueue(activeQuery);
}

void CategoryListView::Requery(const std::string& filter, const int64_t selectAfterQuery) {
    this->RequeryWithField(this->fieldName, filter, selectAfterQuery);
}

void CategoryListView::Requery(const int64_t selectAfterQuery) {
    this->RequeryWithField(this->fieldName, this->filter, selectAfterQuery);
}

void CategoryListView::Reset() {
    this->metadata.reset(new SdkValueList()); /* ugh */
    this->OnAdapterChanged();
}

int64_t CategoryListView::GetSelectedId() {
    size_t index = this->GetSelectedIndex();
    if (index != NO_SELECTION && this->metadata && index < this->metadata->Count()) {
        return this->metadata->At(index)->GetId();
    }
    return this->selectAfterQuery == -1LL ? -1LL : this->selectAfterQuery;
}

std::string CategoryListView::GetSelectedValue() {
    size_t index = this->GetSelectedIndex();
    if (index != NO_SELECTION && this->metadata && index < this->metadata->Count()) {
        return this->metadata->At(index)->ToString();
    }
    return "";
}

std::string CategoryListView::GetFieldName() {
    return this->fieldName;
}

std::string CategoryListView::GetFilter() {
    return this->filter;
}

void CategoryListView::OnTrackChanged(size_t index, musik::core::TrackPtr track) {
    this->playing = track;
    this->OnAdapterChanged();
}

void CategoryListView::SetFieldName(const std::string& fieldName) {
    if (this->fieldName != fieldName) {
        this->selectAfterQuery = -1LL;
        this->fieldName = fieldName;
        this->SetSelectedIndex(0);
        this->ScrollToTop();

        if (this->metadata) {
            this->metadata.reset();
        }

        this->Requery();
    }
}

void CategoryListView::ScrollToPlaying() {
    if (this->playing) {
        size_t selected = this->GetSelectedIndex();

        /* by ID: preferred. */
        if (fieldIdColumn.size()) {
            int64_t id = this->playing->GetInt64(fieldIdColumn.c_str(), 0);
            for (size_t i = 0; i < this->metadata->Count(); i++) {
                if (this->metadata->At(i)->GetId() == id) {
                    selected = i;
                    break;
                }
            }
        }
        /* by name: slower. also, not currently used, but will be in the near
        future when we open up category browsing to other types of metadata */
        else {
            std::string value = this->playing->GetString(this->fieldName.c_str());
            if (value.size()) {
                /* binary search would be better, but need to research if sqlite
                properly sorts utf8 strings. */
                for (size_t i = 0; i < this->metadata->Count(); i++) {
                    if (this->metadata->At(i)->ToString() == value) {
                        selected = i;
                        break;
                    }
                }
            }
        }

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

void CategoryListView::OnEntryContextMenu(size_t index) {
    ListWindow::OnEntryContextMenu(index);
    this->ShowContextMenu();
}

void CategoryListView::ShowContextMenu() {
    int64_t id = this->GetSelectedId();

    if (id != -1LL) {
        PlayQueueOverlays::ShowAddCategoryOverlay(
            this->MessageQueue(),
            this->playback,
            this->library,
            this->fieldName,
            this->GetSelectedValue(),
            id);
    }
}

void CategoryListView::OnQueryCompleted(IQuery* query) {
    auto active = this->activeQuery;
    if (active && query == active.get()) {
        int selectedIndex = -1;
        if (this->selectAfterQuery != -1LL) {
            selectedIndex = active->GetIndexOf(this->selectAfterQuery);
        }

        if (this->activeQuery &&
            this->activeQuery->GetId() == query->GetId() &&
            this->activeQuery->GetStatus() == IQuery::Finished)
        {
            this->metadata = activeQuery->GetResult();
            activeQuery.reset();

            if (selectedIndex >= 0) {
                this->SetSelectedIndex(selectedIndex);

                /* scroll down just a bit more to reveal the item above so
                there's indication the user can scroll. */
                if (!this->IsEntryVisible(selectedIndex)) {
                    this->ScrollTo(selectedIndex == 0 ? selectedIndex : selectedIndex - 1);
                }
            }

            this->OnAdapterChanged();
            this->OnInvalidated();
        }
    }
}

IScrollAdapter& CategoryListView::GetScrollAdapter() {
    return *adapter;
}

CategoryListView::Adapter::Adapter(CategoryListView &parent)
    : parent(parent) {
}

size_t CategoryListView::Adapter::GetEntryCount() {
    return parent.metadata ? parent.metadata->Count() : 0;
}

IScrollAdapter::EntryPtr CategoryListView::Adapter::GetEntry(cursespp::ScrollableWindow* window, size_t index) {
    std::string value = parent.metadata->At(index)->ToString();

    bool playing = false;

    if (parent.playing) {
        /* we should generally be able to match by ID; if not, fall back to by name */
        if (parent.fieldIdColumn.size()) {
            auto playingId = parent.playing->GetInt64(parent.fieldIdColumn.c_str(), 0);
            playing = (playingId == parent.metadata->At(index)->GetId());
        }
        else {
            playing = parent.playing->GetString(parent.fieldName.c_str()) == value;
        }
    }

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
