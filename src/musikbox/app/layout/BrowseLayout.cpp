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
#include <app/query/CategoryTrackListQuery.h>
#include "BrowseLayout.h"

using namespace musik::core::library::constants;

#define CATEGORY_WIDTH 25
#define DEFAULT_CATEGORY constants::Track::ARTIST

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

BrowseLayout::BrowseLayout(
    PlaybackService& playback,
    LibraryPtr library)
: LayoutBase()
, playback(playback) {
    this->library = library;
    this->InitializeWindows();
}

BrowseLayout::~BrowseLayout() {

}

void BrowseLayout::Layout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();

    if (cx == 0 || cy == 0) {
        return;
    }

    size_t x = this->GetX(), y = this->GetY();

    this->MoveAndResize(x, y, cx, cy);

    this->categoryList->MoveAndResize(x, y, CATEGORY_WIDTH, cy);

    this->trackList->MoveAndResize(
        x + CATEGORY_WIDTH, y, cx - CATEGORY_WIDTH, cy);

    this->categoryList->SetFocusOrder(0);
    this->trackList->SetFocusOrder(1);
}

void BrowseLayout::InitializeWindows() {
    this->categoryList.reset(new CategoryListView(this->library, DEFAULT_CATEGORY));
    this->trackList.reset(new TrackListView(this->playback, this->library));

    this->AddWindow(this->categoryList);
    this->AddWindow(this->trackList);

    this->categoryList->SelectionChanged.connect(
        this, &BrowseLayout::OnCategoryViewSelectionChanged);

    this->categoryList->Invalidated.connect(
        this, &BrowseLayout::OnCategoryViewInvalidated);

    this->Layout();
}

void BrowseLayout::ScrollTo(const std::string& fieldType, DBID fieldId) {
    this->categoryList->SetFieldName(fieldType);
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

void BrowseLayout::RequeryTrackList(ListWindow *view) {
    if (view == this->categoryList.get()) {
        DBID selectedId = this->categoryList->GetSelectedId();
        if (selectedId != -1) {
            this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
                new CategoryTrackListQuery(
                    this->library,
                    this->categoryList->GetFieldName(),
                    selectedId)));
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
    if (key == "^M") { /* enter. play the selection */
        auto tracks = this->trackList->GetTrackList();
        auto focus = this->GetFocus();

        size_t index = (focus == this->trackList)
            ? this->trackList->GetSelectedIndex() : 0;

        this->playback.Play(*tracks, index);
        return true;
    }
    if (key == "KEY_F(5)") {
        this->categoryList->Requery();
        return true;
    }
    else if (key == "ALT_1" || key == "M-1") {
        this->categoryList->SetFieldName(constants::Track::ARTIST);
        return true;
    }
    else if (key == "ALT_2" || key == "M-2") {
        this->categoryList->SetFieldName(constants::Track::ALBUM);
        return true;
    }
    else if (key == "ALT_3" || key == "M-3") {
        this->categoryList->SetFieldName(constants::Track::GENRE);
        return true;
    }

    return LayoutBase::KeyPress(key);
}
