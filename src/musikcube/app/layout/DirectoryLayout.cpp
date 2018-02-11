//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include <core/library/query/local/CategoryTrackListQuery.h>
#include <core/library/query/local/SavePlaylistQuery.h>
#include <core/support/Messages.h>
#include <core/i18n/Locale.h>
#include <app/util/Hotkeys.h>
#include <app/util/Playback.h>
#include <app/util/Messages.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/BrowseOverlays.h>

#include "DirectoryLayout.h"

using namespace musik;
using namespace musik::core::library::constants;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db::local;
using namespace musik::core::library;
using namespace musik::cube;
using namespace cursespp;

static size_t MAX_CATEGORY_WIDTH = 40;
static int MIN_LIST_TITLE_HEIGHT = 26;

DirectoryLayout::DirectoryLayout(
    musik::core::audio::PlaybackService& playback,
    ILibraryPtr library)
: LayoutBase()
, playback(playback)
, library(library) {
    this->InitializeWindows();
}

DirectoryLayout::~DirectoryLayout() {
}

void DirectoryLayout::OnLayout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();
    size_t x = 0, y = 0;
    size_t directoryWidth = std::min(MAX_CATEGORY_WIDTH, cx / 4);
    this->directoryList->MoveAndResize(x, y, directoryWidth, cy);
    this->trackList->MoveAndResize(x + directoryWidth, y, cx - directoryWidth, cy);
    this->directoryList->SetFocusOrder(0);
    this->trackList->SetFocusOrder(1);
}

void DirectoryLayout::InitializeWindows() {
    this->directoryList.reset(new ListWindow());
    this->directoryList->SetFrameTitle(_TSTR("browse_title_directory"));

    this->trackList.reset(new TrackListView(this->playback, this->library));
    this->trackList->SetFrameTitle(_TSTR("browse_title_tracks"));

    this->AddWindow(this->directoryList);
    this->AddWindow(this->trackList);

    this->directoryList->SelectionChanged.connect(
        this, &DirectoryLayout::OnDirectoryChanged);
}

void DirectoryLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    // if (visible) {
    //     this->categoryList->Requery();
    // }
}

void DirectoryLayout::RequeryTrackList(ListWindow *view) {
    // if (view == this->categoryList.get()) {
    //     int64_t selectedId = this->categoryList->GetSelectedId();
    //     if (selectedId != -1) {
    //         auto column = this->categoryList->GetFieldName();
    //         this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
    //             new CategoryTrackListQuery(this->library, column, selectedId)));
    //     }
    //     else {
    //         this->trackList->Clear();
    //     }
    // }
}

void DirectoryLayout::OnDirectoryChanged(
    ListWindow *view, size_t newIndex, size_t oldIndex)
{
    this->RequeryTrackList(view);
}

bool DirectoryLayout::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        /* if the tracklist is NOT focused (i.e. the focus is on a
        category window), start playback from the top. */
        if (this->GetFocus() != this->trackList) {
            auto tracks = trackList->GetTrackList();
            if (tracks) {
                playback.Play(*tracks.get(), 0);
                return true;
            }
        }
    }
    else if (Hotkeys::Is(Hotkeys::ViewRefresh, key)) {
        // this->categoryList->Requery();
        return true;
    }

    return LayoutBase::KeyPress(key);
}