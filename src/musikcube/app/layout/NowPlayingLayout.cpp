//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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
#include <musikcore/ffi/consumer/playback-binding.h>

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>
#include <cursespp/Text.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <app/util/Hotkeys.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/util/PreferenceKeys.h>
#include <musikcore/library/query/NowPlayingTrackListQuery.h>
#include <musikcore/library/query/GetPlaylistQuery.h>
#include <musikcore/library/query/PersistedPlayQueueQuery.h>
#include <musikcore/support/Duration.h>
#include <musikcore/support/PreferenceKeys.h>
#include "NowPlayingLayout.h"

#include <set>
#include <algorithm>

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::cube;
using namespace musik::core::library::query;
using namespace cursespp;

namespace keys = musik::cube::prefs::keys;
namespace components = musik::core::prefs::components;

static std::set<std::string> kEditKeys;

static std::string formatWithAlbum(TrackPtr track, size_t index, size_t width);

NowPlayingLayout::NowPlayingLayout(
    musik::core::audio::PlaybackService& playback,
    musik::core::ILibraryPtr library)
: LayoutBase()
, playback(playback)
, apiPlayback(musik::core::audio::CreatePlaybackHandle(playback))
, library(library)
, lastPlaylistQueryId(-1)
, reselectIndex(-1) {
    this->prefs = Preferences::ForComponent(components::Settings);
    this->InitializeWindows();

    this->displayQueue = std::make_shared<TrackList>(this->library);
    this->apiPlayback.SetQueueChangedCallback(
        [](void* data) {
            auto& layout = *static_cast<NowPlayingLayout*>(data);
            if (layout.IsVisible()) layout.RequeryTrackList();
        }, this);

    this->trackListView->SetRowRenderer(
        TrackRowRenderers::Get(TrackRowRenderers::Type::NowPlaying));

    kEditKeys = {
        Hotkeys::Get(Hotkeys::PlayQueueMoveUp),
        Hotkeys::Get(Hotkeys::PlayQueueMoveDown),
        Hotkeys::Get(Hotkeys::PlayQueueDelete)
    };
}

Color NowPlayingLayout::RowDecorator(musik::core::TrackPtr track, size_t index) {
    const bool selected = index == trackListView->GetSelectedIndex();

    Color attrs = selected ? Color::ListItemHighlighted : Color::Default;
    if (track && this->queue && index == this->queue->CurrentRow()) {
        attrs = selected ? Color::ListItemHighlightedSelected : Color::ListItemSelected;
    }

    return attrs;
}

void NowPlayingLayout::OnLayout() {
    this->trackListView->MoveAndResize(
        0,
        0,
        this->GetWidth(),
        this->GetHeight());

    this->trackListView->SetFocusOrder(0);
}

void NowPlayingLayout::InitializeWindows() {
    this->trackListView = std::make_shared<TrackListView>(
        this->playback,
        this->library,
        std::bind(
            &NowPlayingLayout::RowDecorator,
            this,
            std::placeholders::_1,
            std::placeholders::_2));

    this->trackListView->SetFrameTitle(_TSTR("playqueue_title"));
    this->trackListView->SetQueueActivationCallback([this](size_t row) {
        this->apiPlayback.PlayQueueRow(row);
    });
    this->trackListView->Requeried.connect(this, &NowPlayingLayout::OnTrackListRequeried);
    this->AddWindow(this->trackListView);
}

void NowPlayingLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->RequeryTrackList();
    }
    else {
        this->queue.reset();
        this->trackListView->Reset();
    }
}

void NowPlayingLayout::OnTrackListRequeried(musik::core::library::query::TrackListQueryBase* query) {
    /* in most cases we pull the TrackList directly from the PlaybackService.
    however, some user operations cause the TrackList to be loaded from
    the database, e.g. loading regular playlists. in these cases, copy
    the tracks TO the PlaybackService, then refresh! */
    if (query && query->GetId() == this->lastPlaylistQueryId) {
        this->playback.CopyFrom(*query->GetResult());
        this->lastPlaylistQueryId = -1;
    }
}

void NowPlayingLayout::SelectQueueRow() {
    if (this->queue && this->queue->Size()) {
        /* regular logic (i.e, no edit operation) */
        if (this->reselectIndex == -1) {
            const size_t index = this->queue->CurrentRow();

            if (index == ListWindow::NO_SELECTION) { /* not playing? */
                this->trackListView->ScrollTo(0);
                this->trackListView->SetSelectedIndex(0);
            }
            else { /* playing... */
                const size_t scrollToIndex = index == 0 ? index : index - 1;
                if (!this->trackListView->IsEntryVisible(scrollToIndex)) {
                    this->trackListView->ScrollTo(scrollToIndex);
                }

                this->trackListView->SetSelectedIndex(index);
            }
        }
        /* user just finished an edit. this is a bit of a hack; we're notified
        of the edit completion asynchronously, so before we complete the edit
        we stash the index we need to re-select. */
        else {
            /* ensure the correct index is selected, and that it's properly
            scrolled into view */
            this->reselectIndex = std::min((int) this->trackListView->TrackCount() - 1, this->reselectIndex);
            this->trackListView->SetSelectedIndex(narrow_cast<size_t>(this->reselectIndex));

            if (!this->trackListView->IsEntryVisible(narrow_cast<size_t>(this->reselectIndex))) {
                this->trackListView->ScrollTo(narrow_cast<size_t>(this->reselectIndex));
            }

            this->reselectIndex = -1;
        }

        /* if after a bunch of monkeying around there's still nothing
        selected, but we have contents, let's just select the first item */
        const auto sel = this->trackListView->GetSelectedIndex();
        if (sel == ListWindow::NO_SELECTION || sel >= this->trackListView->TrackCount()) {
            this->trackListView->SetSelectedIndex(0);
            this->trackListView->ScrollTo(0);
        }
    }
    else {
        this->reselectIndex = -1;
        this->trackListView->ScrollTo(0);
    }
}

void NowPlayingLayout::RequeryTrackList() {
    auto updated = this->apiPlayback.Queue();
    if (!updated) return;

    size_t count = 0;
    size_t previousCount = 0;
    const auto ids = updated->Ids(count);
    const auto previous = this->queue ? this->queue->Ids(previousCount) : nullptr;
    const bool changed = !this->queue || count != previousCount ||
        (count && !std::equal(ids, ids + count, previous));
    this->queue = std::move(updated);

    /* Reuse the renderer's metadata cache while updating the ordered rows. */
    if (changed) {
        TrackList replacement(this->library, ids, count);
        this->displayQueue->Swap(replacement);
    }
    this->trackListView->SetTrackList(this->displayQueue);
    if (changed || this->reselectIndex >= 0) this->SelectQueueRow();
}

void NowPlayingLayout::OnPlaylistSelected(int64_t playlistId) {
    auto query = std::make_shared<GetPlaylistQuery>(library, playlistId);
    this->lastPlaylistQueryId = query->GetId();
    this->trackListView->Requery(query);
}

bool NowPlayingLayout::KeyPress(const std::string& key) {
    if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistLoad, key)) {
        PlayQueueOverlays::ShowLoadPlaylistOverlay(
            this->playback,
            this->library,
            std::bind(&NowPlayingLayout::OnPlaylistSelected, this, std::placeholders::_1));
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistSave, key)) {
        PlayQueueOverlays::ShowSavePlaylistOverlay(
            this->MessageQueue(), this->playback, this->library);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistRename, key)) {
        PlayQueueOverlays::ShowRenamePlaylistOverlay(this->library);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistDelete, key)) {
        PlayQueueOverlays::ShowDeletePlaylistOverlay(this->library);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueueClear, key)) {
        this->apiPlayback.ClearQueue();
        return true;
    }
    else if (ProcessEditOperation(key)) {
        return true;
    }

    return LayoutBase::KeyPress(key);
}

bool NowPlayingLayout::ProcessEditOperation(const std::string& key) {
    if (kEditKeys.find(key) != kEditKeys.end()) {
        if (this->queue && !this->queue->IsShuffled()) {
            const size_t selected = this->trackListView->GetSelectedIndex();
            this->reselectIndex = -1;

            const auto count = this->queue->Size();
            if (selected >= count) return true;

            if (Hotkeys::Is(Hotkeys::PlayQueueMoveUp, key) && selected > 0) {
                const auto to = selected - 1;
                if (this->apiPlayback.Move(selected, to) == MC_OK) reselectIndex = narrow_cast<int>(to);
            }
            else if (Hotkeys::Is(Hotkeys::PlayQueueMoveDown, key) && selected + 1 < count) {
                const auto to = selected + 1;
                if (this->apiPlayback.Move(selected, to) == MC_OK) reselectIndex = narrow_cast<int>(to);
            }
            else if (Hotkeys::Is(Hotkeys::PlayQueueDelete, key)) {
                if (this->apiPlayback.Remove(selected) == MC_OK) reselectIndex = narrow_cast<int>(selected);
            }

            return true;
        }
    }

    return false;
}
