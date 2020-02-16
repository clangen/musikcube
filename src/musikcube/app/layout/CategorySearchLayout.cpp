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
#include <cursespp/Screen.h>
#include <cursespp/Text.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/support/PreferenceKeys.h>
#include <app/util/Hotkeys.h>
#include <app/util/PreferenceKeys.h>
#include "CategorySearchLayout.h"

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::cube;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;
namespace components = musik::core::prefs::components;

#define SEARCH_HEIGHT 3

#define IS_CATEGORY(x) \
    x == this->albums || \
    x == this->artists || \
    x == this->genres

#define CREATE_CATEGORY(view, title, type, order) \
    view.reset(new CategoryListView(playback, this->library, type)); \
    view->EntryActivated.connect(this, &CategorySearchLayout::OnCategoryEntryActivated); \
    view->SetFrameTitle(title); \
    view->SetAllowArrowKeyPropagation(); \
    this->AddWindow(view); \
    view->SetFocusOrder(order);

CategorySearchLayout::CategorySearchLayout(musik::core::audio::PlaybackService& playback, ILibraryPtr library)
: LayoutBase() {
    this->library = library;
    this->prefs = Preferences::ForComponent(components::Settings);
    this->InitializeWindows(playback);
}

CategorySearchLayout::~CategorySearchLayout() {

}

void CategorySearchLayout::LoadLastSession() {
    auto session = Preferences::ForComponent(components::Session);
    const std::string lastFilter = session->GetString(keys::LastCategoryFilter);
    if (lastFilter.size()) {
        this->input->SetText(lastFilter);
    }
}

void CategorySearchLayout::SaveSession() {
    auto session = Preferences::ForComponent(components::Session);
    session->SetString(keys::LastCategoryFilter, this->input->GetText().c_str());
}

void CategorySearchLayout::OnLayout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();
    size_t x = 0, y = 0;

    size_t inputWidth = cx / 2;
    size_t inputX = x + ((cx - inputWidth) / 2);
    this->input->MoveAndResize(inputX, 0, cx / 2, SEARCH_HEIGHT);
    this->input->SetHint(_TSTR("search_filter_hint"));

    size_t labelY = SEARCH_HEIGHT;
    size_t categoryWidth = cx / 3;
    size_t categoryY = labelY;
    size_t categoryHeight = cy - SEARCH_HEIGHT;
    size_t lastCategoryWidth = cx - (categoryWidth * 2);

    this->albums->MoveAndResize(0, categoryY, categoryWidth, categoryHeight);
    this->artists->MoveAndResize(categoryWidth, categoryY, categoryWidth, categoryHeight);
    this->genres->MoveAndResize(categoryWidth * 2, categoryY, lastCategoryWidth, categoryHeight);
}

void CategorySearchLayout::InitializeWindows(musik::core::audio::PlaybackService& playback) {
    this->input.reset(new cursespp::TextInput());
    this->input->TextChanged.connect(this, &CategorySearchLayout::OnInputChanged);
    this->input->EnterPressed.connect(this, &CategorySearchLayout::OnEnterPressed);
    this->input->SetFocusOrder(0);

    this->AddWindow(this->input);

    CREATE_CATEGORY(this->albums, _TSTR("browse_title_albums"), constants::Track::ALBUM, 1);
    CREATE_CATEGORY(this->artists, _TSTR("browse_title_artists"), constants::Track::ARTIST, 2);
    CREATE_CATEGORY(this->genres, _TSTR("browse_title_genres"), constants::Track::GENRE, 3);
}

void CategorySearchLayout::Requery() {
    const std::string& value = this->input->GetText();
    this->albums->Requery(value);
    this->artists->Requery(value);
    this->genres->Requery(value);
}

void CategorySearchLayout::FocusInput() {
    this->SetFocus(this->input);
}

void CategorySearchLayout::OnInputChanged(cursespp::TextInput* sender, std::string value) {
    if (this->IsVisible()) {
        this->Requery();
    }
}

void CategorySearchLayout::OnEnterPressed(cursespp::TextInput* sender) {
    this->SetFocus(this->albums);
}

void CategorySearchLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->Requery();
    }
    else {
        this->SaveSession();
        this->input->SetText("");
        this->albums->Reset();
        this->artists->Reset();
        this->genres->Reset();
        this->SetFocusIndex(0);
    }
}

void CategorySearchLayout::OnCategoryEntryActivated(
    cursespp::ListWindow* listWindow, size_t index)
{
    CategoryListView* category =
        static_cast<CategoryListView*>(listWindow);

    if ((int) index >= 0) {
        this->SearchResultSelected(
            this,
            category->GetFieldName(),
            category->GetSelectedId());
    }
}

bool CategorySearchLayout::KeyPress(const std::string& key) {
    IWindowPtr focus = this->GetFocus();

    if (Hotkeys::Is(Hotkeys::Down, key)) {
        if (this->GetFocus() == this->input) {
            this->FocusNext();
            return true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::Up, key)) {
        if (IS_CATEGORY(this->GetFocus())) {
            this->SetFocus(this->input);
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}
