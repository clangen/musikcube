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
#include "SearchLayout.h"

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

#define SEARCH_HEIGHT 3

SearchLayout::SearchLayout(LibraryPtr library)
: LayoutBase() {
    this->library = library;
    this->InitializeWindows();
}

SearchLayout::~SearchLayout() {

}

void SearchLayout::Layout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();

    if (cx == 0 || cy == 0) {
        return;
    }

    size_t x = this->GetX(), y = this->GetY();

    this->MoveAndResize(x, y, cx, cy);

    size_t inputWidth = cx / 2;
    size_t inputX = x + ((cx - inputWidth) / 2);
    this->input->MoveAndResize(inputX, 0, cx / 2, SEARCH_HEIGHT);

    size_t categoryWidth = cx / 3;
    size_t categoryY = SEARCH_HEIGHT;
    size_t categoryHeight = cy - SEARCH_HEIGHT;
    size_t lastCategoryWidth = cx - (categoryWidth * 2);

    this->albums->MoveAndResize(0, categoryY, categoryWidth, categoryHeight);
    this->artists->MoveAndResize(categoryWidth, categoryY, categoryWidth, categoryHeight);
    this->genres->MoveAndResize(categoryWidth * 2, categoryY, lastCategoryWidth, categoryHeight);
}

void SearchLayout::InitializeWindows() {
    this->input.reset(new cursespp::TextInput());
    this->input->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->input->SetFocusOrder(0);
    this->AddWindow(this->input);

    this->albums.reset(new CategoryListView(this->library, constants::Track::ALBUM));
    this->AddWindow(this->albums);
    this->albums->SetFocusOrder(1);

    this->artists.reset(new CategoryListView(this->library, constants::Track::ARTIST));
    this->AddWindow(this->artists);
    this->artists->SetFocusOrder(2);

    this->genres.reset(new CategoryListView(this->library, constants::Track::GENRE));
    this->AddWindow(this->genres);
    this->genres->SetFocusOrder(3);

    //this->categoryList->SelectionChanged.connect(
    //    this, &BrowseLayout::OnCategoryViewSelectionChanged);

    //this->categoryList->Invalidated.connect(
    //    this, &BrowseLayout::OnCategoryViewInvalidated);
    
    this->Layout();
}

void SearchLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    //if (visible) {
    //    this->categoryList->Requery();
    //}
}

bool SearchLayout::KeyPress(const std::string& key) {
    return LayoutBase::KeyPress(key);
}
