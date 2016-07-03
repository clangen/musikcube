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
#include <app/util/Playback.h>

#include "LibraryLayout.h"

using namespace musik::core::library::constants;

#define TRANSPORT_HEIGHT 3
#define SHORTCUTS_HEIGHT 1

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

LibraryLayout::LibraryLayout(PlaybackService& playback, LibraryPtr library)
: LayoutBase()
, playback(playback)
, transport(playback.GetTransport()) {
    this->library = library;
    this->InitializeWindows();
}

LibraryLayout::~LibraryLayout() {

}

void LibraryLayout::Layout() {
    int x = 0, y = 0;
    int cx = Screen::GetWidth(), cy = Screen::GetHeight();

    this->MoveAndResize(x, y, cx, cy);

    int mainHeight = cy - TRANSPORT_HEIGHT - SHORTCUTS_HEIGHT;

    this->browseLayout->MoveAndResize(x, y, cx, mainHeight);
    this->browseLayout->Layout();

    this->nowPlayingLayout->MoveAndResize(x, y, cx, mainHeight);
    this->nowPlayingLayout->Layout();

    this->searchLayout->MoveAndResize(x, y, cx, mainHeight);
    this->searchLayout->Layout();

    this->trackSearch->MoveAndResize(x, y, cx, mainHeight);
    this->trackSearch->Layout();

    this->shortcuts->MoveAndResize(0, cy - 1, cx, 1);

    this->transportView->MoveAndResize(
        1,
        mainHeight,
        cx - 2,
        TRANSPORT_HEIGHT);

    if (!this->visibleLayout) {
        this->ShowBrowse();
    }
}

void LibraryLayout::ChangeMainLayout(std::shared_ptr<cursespp::LayoutBase> newLayout) {
    if (this->visibleLayout != newLayout) {
        if (this->visibleLayout) {
           this->RemoveWindow(this->visibleLayout);
           this->visibleLayout->Hide();
        }

        this->visibleLayout = newLayout;
        this->AddWindow(this->visibleLayout);
        this->visibleLayout->Layout();
        this->visibleLayout->Show();

        if (this->IsVisible()) {
            this->BringToTop();
        }
    }
}

void LibraryLayout::ShowNowPlaying() {
    this->ChangeMainLayout(this->nowPlayingLayout);
}

void LibraryLayout::ShowBrowse() {
    this->ChangeMainLayout(this->browseLayout);
}

void LibraryLayout::ShowSearch() {
    this->ChangeMainLayout(this->searchLayout);
}

void LibraryLayout::ShowTrackSearch() {
    this->ChangeMainLayout(this->trackSearch);
}

void LibraryLayout::InitializeWindows() {
    this->browseLayout.reset(new BrowseLayout(this->playback, this->library));
    this->nowPlayingLayout.reset(new NowPlayingLayout(this->playback, this->library));

    this->searchLayout.reset(new SearchLayout(this->playback, this->library));
    this->searchLayout->SearchResultSelected.connect(this, &LibraryLayout::OnSearchResultSelected);

    this->trackSearch.reset(new TrackSearchLayout(this->playback, this->library));

    this->transportView.reset(new TransportWindow(this->playback));

    this->shortcuts.reset(new ShortcutsWindow());
    this->shortcuts->AddShortcut("ALT+b", "browse");
    this->shortcuts->AddShortcut("ALT+f", "filter");
    this->shortcuts->AddShortcut("ALT+t", "tracks");
    this->shortcuts->AddShortcut("ALT+p", "play queue");
    this->shortcuts->AddShortcut("ALT+s", "settings");

    this->AddWindow(this->transportView);
    this->AddWindow(this->shortcuts);

    this->Layout();
}

void LibraryLayout::OnSearchResultSelected(
    SearchLayout* layout, std::string fieldType, DBID fieldId)
{
    this->ShowBrowse();
    this->browseLayout->ScrollTo(fieldType, fieldId);
}

IWindowPtr LibraryLayout::FocusNext() {
    return this->visibleLayout->FocusNext();
}

IWindowPtr LibraryLayout::FocusPrev() {
    return this->visibleLayout->FocusPrev();
}

IWindowPtr LibraryLayout::GetFocus() {
    return this->visibleLayout->GetFocus();
}

bool LibraryLayout::KeyPress(const std::string& key) {
    if (key == "^[") { /* switches between browse/now playing */
        if (this->visibleLayout == this->nowPlayingLayout ||
            this->visibleLayout == this->searchLayout) {
            this->ShowBrowse();
        }
        else {
            this->ShowNowPlaying();
        }
        return true;
    }
    else if (key == "M-p") {
        this->ShowNowPlaying();
        return true;
    }
    else if (key == "M-b") {
        this->ShowBrowse();
        return true;
    }
    else if (key == "M-f") {
        this->ShowSearch();
        return true;
    }
    else if (key == "M-t") {
        this->ShowTrackSearch();
        return true;
    }
    /* forward to the visible layout */
    else if (this->visibleLayout && this->visibleLayout->KeyPress(key)) {
        return true;
    }
    else if (key == " ") {
        playback::PauseOrResume(this->transport);
        return true;
    }

    return LayoutBase::KeyPress(key);
}
