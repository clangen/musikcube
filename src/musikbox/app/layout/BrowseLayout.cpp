//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include <glue/query/CategoryTrackListQuery.h>
#include <app/util/Hotkeys.h>
#include <app/util/Playback.h>
#include <app/overlay/PlayQueueOverlays.h>

#include "BrowseLayout.h"

using namespace musik::core::library::constants;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

static size_t MAX_CATEGORY_WIDTH = 40;
static int MIN_LIST_TITLE_HEIGHT = 26;

#define DEFAULT_CATEGORY constants::Track::ARTIST
#define DEFAULT_CATEGORY_NAME FIELD_TO_TITLE[DEFAULT_CATEGORY]

static std::map <std::string, std::string> FIELD_TO_TITLE{
    std::make_pair(constants::Track::ARTIST, "artists"),
    std::make_pair(constants::Track::ALBUM, "albums"),
    std::make_pair(constants::Track::GENRE, "genres"),
    std::make_pair(constants::Track::ALBUM_ARTIST, "album artists")
};

#define CATEGORY_TITLE(key) \
    FIELD_TO_TITLE.find(key) == FIELD_TO_TITLE.end() ? "category" : FIELD_TO_TITLE[key]

#define SET_CATEGORY(key) { \
    std::string title = CATEGORY_TITLE(key); \
    this->categoryList->SetFieldName(key); \
    this->categoryTitle->SetText(title, text::AlignCenter); \
}

#define SET_CATEGORY_TITLE(key) \
    this->categoryTitle->SetText(CATEGORY_TITLE(key), text::AlignCenter);

BrowseLayout::BrowseLayout(
    musik::core::audio::PlaybackService& playback,
    ILibraryPtr library)
: LayoutBase()
, playback(playback) {
    this->library = library;
    this->library->Indexer()->TrackRefreshed.connect(this, &BrowseLayout::OnIndexerProgress);
    this->library->Indexer()->SynchronizeEnd.connect(this, &BrowseLayout::OnIndexerProgress);
    this->InitializeWindows();
}

BrowseLayout::~BrowseLayout() {
}

void BrowseLayout::OnLayout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();
    size_t x = this->GetX(), y = this->GetY();

    size_t categoryWidth = std::min(MAX_CATEGORY_WIDTH, cx / 4);

    if (Screen::GetHeight() > MIN_LIST_TITLE_HEIGHT) {
        ++y;

        this->categoryTitle->MoveAndResize(x, y, categoryWidth, 1);
        this->categoryTitle->Show();

        this->tracksTitle->MoveAndResize(x + categoryWidth, y, cx - categoryWidth, 1);
        this->tracksTitle->Show();

        ++y;
        cy -= 2;
    }
    else {
        this->categoryTitle->Hide();
        this->tracksTitle->Hide();
    }

    this->categoryList->MoveAndResize(x, y, categoryWidth, cy);
    this->trackList->MoveAndResize(x + categoryWidth, y, cx - categoryWidth, cy);

    this->categoryList->SetFocusOrder(0);
    this->trackList->SetFocusOrder(1);
}

void BrowseLayout::InitializeWindows() {
    this->categoryTitle.reset(new TextLabel());
    this->categoryTitle->SetText(DEFAULT_CATEGORY_NAME, text::AlignCenter);
    this->categoryTitle->Hide();
    this->categoryList.reset(new CategoryListView(this->playback, this->library, DEFAULT_CATEGORY));

    this->tracksTitle.reset(new TextLabel());
    this->tracksTitle->SetText("tracks", text::AlignCenter);
    this->trackList.reset(new TrackListView(this->playback, this->library));

    this->AddWindow(this->categoryTitle);
    this->AddWindow(this->tracksTitle);
    this->AddWindow(this->categoryList);
    this->AddWindow(this->trackList);

    this->categoryList->SelectionChanged.connect(
        this, &BrowseLayout::OnCategoryViewSelectionChanged);

    this->categoryList->Invalidated.connect(
        this, &BrowseLayout::OnCategoryViewInvalidated);
}

void BrowseLayout::ScrollTo(const std::string& fieldType, DBID fieldId) {
    this->SetFocus(this->trackList);
    this->categoryList->RequeryWithField(fieldType, "", fieldId);
    SET_CATEGORY_TITLE(fieldType);
}

IWindowPtr BrowseLayout::GetFocus() {
    return this->focused ? this->focused : LayoutBase::GetFocus();
}

void BrowseLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->categoryList->Requery();
    }
}

void BrowseLayout::OnIndexerProgress() {
    this->categoryList->Requery();
}

void BrowseLayout::RequeryTrackList(ListWindow *view) {
    if (view == this->categoryList.get()) {
        DBID selectedId = this->categoryList->GetSelectedId();
        if (selectedId != -1) {
            this->trackList->Requery(std::shared_ptr<musik::glue::TrackListQueryBase>(
                new musik::glue::CategoryTrackListQuery(
                    this->library,
                    this->categoryList->GetFieldName(),
                    selectedId)));
        }
        else {
            this->trackList->Clear();
        }
    }
}

void BrowseLayout::OnCategoryViewSelectionChanged(
    ListWindow *view, size_t newIndex, size_t oldIndex) {
    this->RequeryTrackList(view);
}

void BrowseLayout::OnCategoryViewInvalidated(
    ListWindow *view, size_t selectedIndex) {
    this->RequeryTrackList(view);
}

bool BrowseLayout::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        playback::Play(this->trackList, this->playback, this->GetFocus());
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::ContextMenu, key)) {
        if (this->GetFocus() == this->trackList) {
            PlayQueueOverlays::ShowAddTrackOverlay(this->playback, *this->trackList);
            return true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::ViewRefresh, key)) {
        this->categoryList->Requery();
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseArtists, key)) {
        SET_CATEGORY(constants::Track::ARTIST);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbums, key)) {
        SET_CATEGORY(constants::Track::ALBUM);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseGenres, key)) {
        SET_CATEGORY(constants::Track::GENRE);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateLibraryBrowseAlbumArtists, key)) {
        SET_CATEGORY(constants::Track::ALBUM_ARTIST);
        return true;
    }

    return LayoutBase::KeyPress(key);
}
