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

#include <core/library/query/local/DirectoryTrackListQuery.h>
#include <core/support/Messages.h>
#include <core/i18n/Locale.h>
#include <cursespp/Colors.h>
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
    ScrollAdapterBase::ItemDecorator decorator =
        std::bind(
            &DirectoryLayout::ListItemDecorator,
            this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4);

    this->adapter = std::make_shared<DirectoryAdapter>();
    this->adapter->SetAllowEscapeRoot(false);
    this->adapter->SetItemDecorator(decorator);

    this->directoryList.reset(new ListWindow());
    this->directoryList->SetFrameTitle(_TSTR("browse_title_directory"));
    this->directoryList->SetAdapter(this->adapter);

    this->trackList.reset(new TrackListView(this->playback, this->library));
    this->trackList->SetFrameTitle(_TSTR("browse_title_tracks"));

    this->AddWindow(this->directoryList);
    this->AddWindow(this->trackList);

    this->directoryList->SelectionChanged.connect(
        this, &DirectoryLayout::OnDirectoryChanged);
}

void DirectoryLayout::SetDirectory(const std::string& directory) {
    this->rootDirectory = directory;
    this->adapter->SetRootDirectory(directory);
    this->Refresh();
}

void DirectoryLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);
    if (visible) {
        this->Refresh(true);
    }
}

void DirectoryLayout::RequeryTrackList(ListWindow *view) {
    size_t selected = this->directoryList->GetSelectedIndex();
    if (selected != ListWindow::NO_SELECTION) {
        if (selected == 0 && this->adapter->GetLeafAt(0) == "..") {
            return;
        }

        std::string fullPath = this->adapter->GetFullPathAt(selected);
        if (fullPath.size()) {
            fullPath = NormalizeDir(fullPath);
            this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
                new DirectoryTrackListQuery(this->library, fullPath)));
        }
    }
}

void DirectoryLayout::OnDirectoryChanged(
    ListWindow *view, size_t newIndex, size_t oldIndex)
{
    this->RequeryTrackList(view);
}

void DirectoryLayout::Refresh(bool requery) {
    this->adapter->Refresh();
    if (requery) { this->Requery(); }
}

void DirectoryLayout::Requery() {
    this->RequeryTrackList(this->directoryList.get());
}

bool DirectoryLayout::IsParentSelected() {
    return
        this->directoryList->GetSelectedIndex() == 0 &&
        this->adapter->GetLeafAt(0) == "..";
}

bool DirectoryLayout::IsParentRoot() {
    std::string root = NormalizeDir(this->rootDirectory);
    return
        root == this->adapter->GetParentPath() ||
        root == this->adapter->GetCurrentPath();
}

bool DirectoryLayout::KeyPress(const std::string& key) {
    if (key == "KEY_ENTER") {
        if (this->GetFocus() = this->directoryList) {
            if (!this->adapter->HasSubDirectories(this->directoryList->GetSelectedIndex())) {
                return true;
            }

            size_t index = this->adapter->Select(this->directoryList.get());
            if (index == DirectoryAdapter::NO_INDEX) {
                index = IsParentRoot() ? 1 : 0;
            }

            this->directoryList->SetSelectedIndex(index);
            if (!this->directoryList->IsEntryVisible(index)) {
                this->directoryList->ScrollTo(index);
            }

            this->Requery();
        }
    }
    else if (Hotkeys::Is(Hotkeys::ViewRefresh, key)) {
        this->Refresh();
        return true;
    }

    return LayoutBase::KeyPress(key);
}

int64_t DirectoryLayout::ListItemDecorator(
    ScrollableWindow* scrollable,
    size_t index,
    size_t line,
    IScrollAdapter::EntryPtr entry)
{
    if (scrollable == this->directoryList.get()) {
        if (this->directoryList->GetSelectedIndex() == index) {
            return COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM);
        }
    }
    return -1;
}