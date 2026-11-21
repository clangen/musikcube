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

#include <cursespp/Colors.h>
#include <app/util/Hotkeys.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <musikcore/debug.h>
#include "NowPlayingLayout.h"

#include <set>
#include <algorithm>

using namespace musik::core;
using namespace musik::cube;
using namespace cursespp;

static std::set<std::string> kEditKeys;

NowPlayingLayout::NowPlayingLayout(
    musik::core::api::Context& musikcore)
: LayoutBase()
, apiPlayback(musikcore.GetPlayback())
, reselectIndex(-1)
, musikcore(musikcore)
{
    this->InitializeWindows();

    this->apiPlayback.SetQueueChangedCallback(
        [](void* data) {
            auto& layout = *static_cast<NowPlayingLayout*>(data);
            if (layout.IsVisible()) {
                layout.RequeryTrackList();
            }
        },
        this);

    this->trackListView->SetRowRenderer(
        TrackRowRenderers::Get(TrackRowRenderers::Type::NowPlaying));

    kEditKeys = {
        Hotkeys::Get(Hotkeys::PlayQueueMoveUp),
        Hotkeys::Get(Hotkeys::PlayQueueMoveDown),
        Hotkeys::Get(Hotkeys::PlayQueueDelete)
    };
}

NowPlayingLayout::~NowPlayingLayout() {
    this->CancelPlaylistLoad();
}

Color NowPlayingLayout::RowDecorator(size_t index) {
    const bool selected = index == trackListView->GetSelectedIndex();

    Color attrs = selected ? Color::ListItemHighlighted : Color::Default;
    if (this->queue && index == this->queue->CurrentRow()) {
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
        this->musikcore,
        std::bind(
            &NowPlayingLayout::RowDecorator,
            this,
            std::placeholders::_1));

    this->trackListView->SetFrameTitle(_TSTR("playqueue_title"));
    this->trackListView->SetQueueActivationCallback([this](size_t row) {
        if (this->queue && this->apiPlayback.PlayQueueRow(*this->queue, row) == MC_ERROR_NOT_FOUND) {
            this->RequeryTrackList();
        }
    });
    this->AddWindow(this->trackListView);
}

void NowPlayingLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->RequeryTrackList();
    }
    else {
        this->CancelPlaylistLoad();
        this->queue.reset();
        this->trackListView->Reset();
    }
}

void NowPlayingLayout::CancelPlaylistLoad() {
    this->playlistWork.Invalidate();
    if (this->playlistRequest) {
        this->musikcore.Cancel(this->playlistRequest);
        this->playlistRequest = 0;
    }
}

void NowPlayingLayout::SelectQueueRow() {
    if (this->queue && this->queue->Size()) {
        /* regular logic (i.e, no edit operation) */
        if (this->reselectIndex == -1) {
            const size_t index = this->queue->RetainedRow();

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
    if (!updated) {
        return;
    }

    size_t count = 0;
    size_t previousCount = 0;
    const auto ids = updated->Ids(count);
    const auto previous = this->queue ? this->queue->Ids(previousCount) : nullptr;
    const bool changed = !this->queue || count != previousCount ||
        (count && !std::equal(ids, ids + count, previous));
    this->queue = std::move(updated);

    /* Playback remains authoritative for duplicate rows, position, and revision. */
    if (changed) {
        this->trackListView->SetTrackIds(ids, count, 0);
    }
    else {
        this->trackListView->Redraw();
    }
    if (changed || this->reselectIndex >= 0) {
        this->SelectQueueRow();
    }
}

void NowPlayingLayout::OnPlaylistSelected(int64_t playlistId) {
    this->CancelPlaylistLoad();

    try {
        this->playlistRequest = api::OpenPlaylist(this->musikcore, playlistId,
            this->playlistWork.Begin([this](mc_result status, api::TrackResult tracks, std::string_view) {
                this->playlistRequest = 0;
                if (status == MC_OK && this->apiPlayback.ReplaceQueue(tracks) == MC_OK) {
                    this->RequeryTrackList();
                }
            }));
    }
    catch (const api::Error& error) {
        musik::debug::error("NowPlayingLayout", error.what());
    }
}

void NowPlayingLayout::ShowPlaylistPicker(PlaylistAction action) {
    const auto token = this->playlistWork.CurrentToken();
    try {
        const char* title = nullptr;
        switch (action) {
            case PlaylistAction::Load:
                title = "playqueue_overlay_load_playlist_title";
                break;
            case PlaylistAction::Save:
                title = "playqueue_overlay_save_playlist_title";
                break;
            case PlaylistAction::Rename:
                title = "playqueue_overlay_rename_playlist_title";
                break;
            case PlaylistAction::Delete:
                title = "playqueue_overlay_delete_playlist_title";
                break;
        }
        PlayQueueOverlays::ShowPlaylistPicker(this->musikcore, _TSTR(title),
            action == PlaylistAction::Save, token, [this, action](const api::PlaylistEntry& playlist) {
                switch (action) {
                    case PlaylistAction::Load:
                        this->OnPlaylistSelected(playlist.id);
                        break;
                    case PlaylistAction::Save:
                        this->SaveQueueToPlaylist(playlist);
                        break;
                    case PlaylistAction::Rename:
                        this->RenamePlaylist(playlist);
                        break;
                    case PlaylistAction::Delete:
                        this->DeletePlaylist(playlist);
                        break;
                }
            });
    }
    catch (const api::Error& error) {
        musik::debug::error("NowPlayingLayout", error.what());
    }
}

void NowPlayingLayout::SaveQueueToPlaylist(const api::PlaylistEntry& playlist) {
    auto snapshot = this->apiPlayback.Queue();
    if (!snapshot) {
        return;
    }

    size_t count = 0;
    const auto ids = snapshot->Ids(count);
    std::vector<int64_t> tracks;
    if (count) {
        tracks.assign(ids, ids + count);
    }

    PlayQueueOverlays::ShowSavePlaylistOverlay(
        this->musikcore, this->playlistWork.CurrentToken(), playlist, std::move(tracks));
}

void NowPlayingLayout::RenamePlaylist(const api::PlaylistEntry& playlist) {
    PlayQueueOverlays::ShowRenamePlaylistOverlay(this->musikcore, this->playlistWork.CurrentToken(), playlist);
}

void NowPlayingLayout::DeletePlaylist(const api::PlaylistEntry& playlist) {
    PlayQueueOverlays::ShowDeletePlaylistOverlay(this->musikcore, this->playlistWork.CurrentToken(), playlist);
}

bool NowPlayingLayout::KeyPress(const std::string& key) {
    if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistLoad, key)) {
        this->ShowPlaylistPicker(PlaylistAction::Load);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistSave, key)) {
        this->ShowPlaylistPicker(PlaylistAction::Save);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistRename, key)) {
        this->ShowPlaylistPicker(PlaylistAction::Rename);
        return true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueuePlaylistDelete, key)) {
        this->ShowPlaylistPicker(PlaylistAction::Delete);
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
            if (selected >= count) {
                return true;
            }

            size_t target = selected;
            mc_result status;
            if (Hotkeys::Is(Hotkeys::PlayQueueDelete, key)) {
                status = this->apiPlayback.Remove(*this->queue, selected);
            }
            else {
                const bool up = Hotkeys::Is(Hotkeys::PlayQueueMoveUp, key);
                if ((up && selected == 0) || (!up && selected + 1 == count)) {
                    return true;
                }
                target = up ? selected - 1 : selected + 1;
                status = this->apiPlayback.Move(*this->queue, selected, target);
            }

            if (status == MC_OK) {
                this->reselectIndex = narrow_cast<int>(target);
            }
            if (status == MC_OK || status == MC_ERROR_NOT_FOUND) {
                this->RequeryTrackList();
            }
            return true;
        }
    }

    return false;
}
