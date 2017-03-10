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
#include <cursespp/Text.h>
#include <core/library/LocalLibraryConstants.h>
#include "SearchLayout.h"

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

#define SEARCH_HEIGHT 3
#define LABEL_HEIGHT 1

#define IS_CATEGORY(x) \
    x == this->albums || \
    x == this->artists || \
    x == this->genres

SearchLayout::SearchLayout(musik::core::audio::PlaybackService& playback, ILibraryPtr library)
: LayoutBase() {
    this->library = library;
    this->InitializeWindows(playback);
}

SearchLayout::~SearchLayout() {

}

void SearchLayout::OnLayout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();
    size_t x = 0, y = 0;

    size_t inputWidth = cx / 2;
    size_t inputX = x + ((cx - inputWidth) / 2);
    this->input->MoveAndResize(inputX, 0, cx / 2, SEARCH_HEIGHT);

    size_t labelY = SEARCH_HEIGHT;
    size_t categoryWidth = cx / 3;
    size_t categoryY = labelY + LABEL_HEIGHT;
    size_t categoryHeight = cy - SEARCH_HEIGHT - 1;
    size_t lastCategoryWidth = cx - (categoryWidth * 2);

    this->albumsLabel->MoveAndResize(0, labelY, categoryWidth, 1);
    this->albums->MoveAndResize(0, categoryY, categoryWidth, categoryHeight);

    this->artistsLabel->MoveAndResize(categoryWidth, labelY, categoryWidth, 1);
    this->artists->MoveAndResize(categoryWidth, categoryY, categoryWidth, categoryHeight);

    this->genresLabel->MoveAndResize(categoryWidth * 2, labelY, lastCategoryWidth, 1);
    this->genres->MoveAndResize(categoryWidth * 2, categoryY, lastCategoryWidth, categoryHeight);
}

#define CREATE_CATEGORY(view, type, order) \
    view.reset(new CategoryListView(playback, this->library, type)); \
    view->SetAllowArrowKeyPropagation(); \
    this->AddWindow(view); \
    view->SetFocusOrder(order);

#define CREATE_LABEL(view, value) \
    view.reset(new cursespp::TextLabel()); \
    view->SetText(value, cursespp::text::AlignCenter); \
    this->AddWindow(view);

void SearchLayout::InitializeWindows(musik::core::audio::PlaybackService& playback) {
    this->input.reset(new cursespp::TextInput());
    this->input->TextChanged.connect(this, &SearchLayout::OnInputChanged);
    this->input->EnterPressed.connect(this, &SearchLayout::OnEnterPressed);
    this->input->SetFocusOrder(0);

    this->AddWindow(this->input);

    CREATE_CATEGORY(this->albums, constants::Track::ALBUM, 1);
    CREATE_CATEGORY(this->artists, constants::Track::ARTIST, 2);
    CREATE_CATEGORY(this->genres, constants::Track::GENRE, 3);

    CREATE_LABEL(this->albumsLabel, "albums");
    CREATE_LABEL(this->artistsLabel, "artists");
    CREATE_LABEL(this->genresLabel, "genres");
}

void SearchLayout::Requery() {
    const std::string& value = this->input->GetText();
    this->albums->Requery(value);
    this->artists->Requery(value);
    this->genres->Requery(value);
}

void SearchLayout::FocusInput() {
    this->SetFocus(this->input);
}

void SearchLayout::OnInputChanged(cursespp::TextInput* sender, std::string value) {
    if (this->IsVisible()) {
        this->Requery();
    }
}

void SearchLayout::OnEnterPressed(cursespp::TextInput* sender) {
    this->SetFocus(this->albums);
}

void SearchLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->SetFocus(this->input);
        this->Requery();
    }
    else {
        this->input->SetText("");
        this->albums->Reset();
        this->artists->Reset();
        this->genres->Reset();
    }
}

bool SearchLayout::KeyPress(const std::string& key) {
    IWindowPtr focus = this->GetFocus();
    CategoryListView* category = dynamic_cast<CategoryListView*>(focus.get());

    if (category) {
        if (key == "KEY_ENTER") {
            int index = (int) category->GetSelectedIndex();
            if (index >= 0) {
                this->SearchResultSelected(
                    this,
                    category->GetFieldName(),
                    category->GetSelectedId());
            }

            return true;
        }
    }

    if (key == "KEY_DOWN") {
        if (this->GetFocus() == this->input) {
            this->FocusNext();
            return true;
        }
    }
    else if (key == "KEY_UP") {
        if (IS_CATEGORY(this->GetFocus())) {
            this->SetFocus(this->input);
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}
