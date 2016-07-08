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
#include <app/query/SearchTrackListQuery.h>
#include <app/util/Playback.h>

#include "TrackSearchLayout.h"

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

#define SEARCH_HEIGHT 3

#define REQUERY_TRACKLIST 1001
#define REQUERY_INTERVAL_MS 300

#define DEBOUNCE_REQUERY(x) \
    this->RemoveMessage(REQUERY_TRACKLIST); \
    this->PostMessage(REQUERY_TRACKLIST, 0, 0, x);

TrackSearchLayout::TrackSearchLayout(
    PlaybackService& playback,
    musik::core::LibraryPtr library)
: LayoutBase()
, playback(playback)
, library(library) {
    this->InitializeWindows();
}

TrackSearchLayout::~TrackSearchLayout() {
}

void TrackSearchLayout::Layout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();

    if (cx && cy) {
        int x = this->GetX(), y = this->GetY();

        size_t inputWidth = cx / 2;
        size_t inputX = x + ((cx - inputWidth) / 2);
        this->input->MoveAndResize(inputX, y, cx / 2, SEARCH_HEIGHT);

        this->trackList->MoveAndResize(
            x,
            y + SEARCH_HEIGHT,
            this->GetWidth(),
            this->GetHeight() - SEARCH_HEIGHT);
    }
}

void TrackSearchLayout::InitializeWindows() {
    this->input.reset(new cursespp::TextInput());
    this->input->TextChanged.connect(this, &TrackSearchLayout::OnInputChanged);
    this->input->EnterPressed.connect(this, &TrackSearchLayout::OnEnterPressed);
    this->input->SetFocusOrder(0);
    this->AddWindow(this->input);

    this->trackList.reset(new TrackListView(this->playback, this->library));
    this->trackList->SetFocusOrder(1);
    this->AddWindow(this->trackList);

    this->Layout();
}

void TrackSearchLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->SetFocus(this->input);
        this->Requery();
    }
    else {
        this->input->SetText("");
        this->trackList->Clear();
    }
}

void TrackSearchLayout::FocusInput() {
    this->SetFocus(this->input);
}

void TrackSearchLayout::Requery() {
    const std::string& filter = this->input->GetText();
    this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
        new SearchTrackListQuery(this->library, filter)));
}

void TrackSearchLayout::ProcessMessage(IMessage &message) {
    int type = message.Type();

    if (type == REQUERY_TRACKLIST) {
        this->Requery();
    }
}

void TrackSearchLayout::OnInputChanged(cursespp::TextInput* sender, std::string value) {
    if (this->IsVisible()) {
        DEBOUNCE_REQUERY(REQUERY_INTERVAL_MS);
    }
}

void TrackSearchLayout::OnEnterPressed(cursespp::TextInput* sender) {
    if (this->trackList->GetTrackList()->Count()) {
        playback::Play(this->trackList, this->playback, this->GetFocus());
    }
}

bool TrackSearchLayout::KeyPress(const std::string& key) {
    if (key == "^M") { /* enter. play the selection */
        playback::Play(this->trackList, this->playback, this->GetFocus());
        return true;
    }
    else if (key == "KEY_DOWN") {
        if (this->GetFocus() == this->input) {
            this->FocusNext();
            return true;
        }
    }
    else if (key == "KEY_UP") {
        if (this->GetFocus() == this->trackList) {
            this->SetFocus(this->input);
            return true;
        }
    }

    return LayoutBase::KeyPress(key);
}
