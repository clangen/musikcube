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
#include "TrackListView.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>
#include <cursespp/ToastOverlay.h>

#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/support/Duration.h>

#include <app/util/Hotkeys.h>
#include <app/util/Playback.h>
#include <app/overlay/PlayQueueOverlays.h>
#include <app/overlay/TrackOverlays.h>

#define WINDOW_MESSAGE_SCROLL_TO_PLAYING 1003
#define WINDOW_MESSAGE_TRACK_LIST_WINDOW_CACHED 1004

/* this is pretty gross, but i think we'll eventually settle on one versus the other
and i don't want to bother adding a bunch of annoying infrastructure to more
dynamnically switch between this */
static bool sGetAsync = false;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

using namespace std::chrono;

/* if the user hasn't changed the selected index in 60 seconds
we assume he's not paying attention, and will automatically scroll
the view to the playing track if it's invisible. */
static const milliseconds AUTO_SCROLL_COOLDOWN = milliseconds(60000LL);

static inline milliseconds now() noexcept {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
}

TrackListView::TrackListView(
    PlaybackService& playback,
    ILibraryPtr library,
    RowDecorator decorator)
: ListWindow(nullptr)
, playback(playback) {
    this->library = library;
    this->library->QueryCompleted.connect(this, &TrackListView::OnQueryCompleted);
    this->playback.TrackChanged.connect(this, &TrackListView::OnTrackChanged);
    this->adapter = std::make_unique<Adapter>(*this);
    this->lastQueryHash = 0;
    this->lastChanged = now();
    this->decorator = decorator;
    this->trackNumType = TrackRowRenderers::TrackNumType::Metadata;
    this->renderer = TrackRowRenderers::Get(TrackRowRenderers::Type::AlbumSort);
    this->playing = playback.GetPlaying();
}

void TrackListView::Requery(std::shared_ptr<TrackListQueryBase> query) {
    this->query = query;
    this->library->Enqueue(this->query);
}

void TrackListView::SelectFirstTrack() {
    this->SetSelectedIndex(this->headers.HeaderAt(0) ? 1 : 0);
}

void TrackListView::SetTrackNumType(TrackRowRenderers::TrackNumType type) {
    if (this->trackNumType != type) {
        this->trackNumType = type;
        this->OnAdapterChanged();
    }
}

void TrackListView::SetRowRenderer(TrackRowRenderers::Renderer renderer) {
    if (!renderer) {
        throw std::runtime_error("invalid Renderer supplied to TrackListView::SetRowRenderer");
    }
    this->renderer = renderer;
}

void TrackListView::OnTrackListWindowCached(const musik::core::TrackList* track, size_t from, size_t to) {
    this->Debounce(WINDOW_MESSAGE_TRACK_LIST_WINDOW_CACHED, 0, 0, 50);
}

void TrackListView::OnQueryCompleted(IQuery* query) {
    if (this->query && query == this->query.get()) {
        if (this->query->GetStatus() == IQuery::Finished) {
            const bool hadTracks = this->tracks && this->tracks->Count() > 0;
            const bool prevQuerySame = this->lastQueryHash == this->query->GetQueryHash();

            this->SetTrackListAndUpateEventHandlers(this->query->GetResult());
            this->AdjustTrackListCacheWindowSize();
            this->headers.Set(this->query->GetHeaders(), this->query->GetDurations());
            this->lastQueryHash = this->query->GetQueryHash();

            /* update our internal state */
            this->OnAdapterChanged();

            /* if the query was functionally the same as the last query, or we
            previously had no tracks, let's scroll to the top and select the first
            track */
            if (!hadTracks || !prevQuerySame) {
                this->ScrollToTop();
                this->SelectFirstTrack();
            }

            /* let external listeners know we updated */
            this->Requeried(this->query.get());

            this->query.reset();
        }
    }
}

std::shared_ptr<TrackList> TrackListView::GetTrackList() noexcept {
    return this->tracks;
}

void TrackListView::SetTrackList(std::shared_ptr<TrackList> trackList) {
    if (this->tracks != trackList) {
        this->SetTrackListAndUpateEventHandlers(trackList);
        this->AdjustTrackListCacheWindowSize();
        this->ScrollToTop();
        this->SelectFirstTrack();
    }

    this->OnAdapterChanged();
}

void TrackListView::Reset() {
    this->query.reset();
    this->tracks = std::make_shared<TrackList>(this->library);
    this->headers.Reset();
    this->OnAdapterChanged();
}

void TrackListView::InvalidateData() {
    this->tracks->ClearCache();
    this->OnAdapterChanged();
}

TrackPtr TrackListView::GetSelectedTrack() {
    const auto i = this->GetSelectedTrackIndex();
    return (i == ListWindow::NO_SELECTION) ? TrackPtr() : this->tracks->Get(i);
}

size_t TrackListView::GetSelectedTrackIndex() {
    const auto i = this->GetSelectedIndex();
    if (i != ListWindow::NO_SELECTION) {
        auto entry = adapter->GetEntry(this, i);
        return dynamic_cast<TrackListEntry*>(entry.get())->GetIndex();
    }
    return ListWindow::NO_SELECTION;
}

size_t TrackListView::TrackIndexToAdapterIndex(size_t index) {
    return this->headers.TrackListToAdapterIndex(index);
}

size_t TrackListView::TrackCount() noexcept {
    return this->tracks ? this->tracks->Count() : 0;
}

size_t TrackListView::EntryCount() {
    return this->GetScrollAdapter().GetEntryCount();
}

void TrackListView::ScrollToPlaying() {
    if (this->playing && this->tracks) {
        const int64_t id = this->playing->GetId();
        for (size_t i = 0; i < this->tracks->Count(); i++) {
            if (this->tracks->GetId(i) == id) {
                const size_t rawIndex = headers.TrackListToAdapterIndex(i);
                this->SetSelectedIndex(rawIndex);

                if (!this->IsEntryVisible(rawIndex)) {
                    this->ScrollTo(rawIndex);
                }

                return;
            }
        }
    }
}

void TrackListView::SetTrackListAndUpateEventHandlers(std::shared_ptr<TrackList> trackList) {
    if (this->tracks) {
        this->tracks->WindowCached.disconnect(this);
    }
    this->tracks = trackList;
    if (this->tracks) {
        this->tracks->WindowCached.connect(this, &TrackListView::OnTrackListWindowCached);
    }
    sGetAsync = Preferences::ForComponent(core::prefs::components::Settings)->GetBool(
        core::prefs::keys::AsyncTrackListQueries, true);
}

void TrackListView::ProcessMessage(IMessage &message) {
    if (message.Type() == WINDOW_MESSAGE_SCROLL_TO_PLAYING) {
        this->ScrollToPlaying();
    }
    else if (message.Type() == WINDOW_MESSAGE_TRACK_LIST_WINDOW_CACHED) {
        this->Redraw();
    }
    else {
        ListWindow::ProcessMessage(message);
    }
}

void TrackListView::AdjustTrackListCacheWindowSize() {
    const auto height = this->GetHeight();
    if (tracks && height > 0) {
        tracks->SetCacheWindowSize(this->GetHeight());
    }
}

void TrackListView::OnDimensionsChanged() {
    this->AdjustTrackListCacheWindowSize();
    ListWindow::OnDimensionsChanged();
}

bool TrackListView::OnEntryActivated(size_t index) {
    if (headers.HeaderAt(this->GetSelectedIndex())) {
        TrackPtr track = this->GetSelectedTrack();
        PlayQueueOverlays::ShowAlbumDividerOverlay(
            MessageQueue(), this->playback, this->library, track);
    }
    else {
        playback::PlaySelected(*this, this->playback);
    }
    return true;
}

bool TrackListView::OnEntryContextMenu(size_t index) {
    ListWindow::OnEntryContextMenu(index);
    this->ShowContextMenu();
    return true;
}

void TrackListView::ShowContextMenu() {
    TrackPtr track = this->GetSelectedTrack();
    if (!headers.HeaderAt(this->GetSelectedIndex())) {
        if (track) {
            PlayQueueOverlays::ShowAddTrackOverlay(
                MessageQueue(), this->library, this->playback, track);
        }
    }
    else {
        PlayQueueOverlays::ShowAlbumDividerOverlay(
            MessageQueue(), this->playback, this->library, track);
    }
}

bool TrackListView::KeyPress(const std::string& key) {
    bool handled = false;

    if (Hotkeys::Is(Hotkeys::ContextMenu, key)) {
        this->ShowContextMenu();
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::NavigateJumpToPlaying, key)) {
        this->ScrollToPlaying();
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::PlayQueueHotSwap, key)) {
        if (!headers.HeaderAt(this->GetSelectedIndex())) {
            if (playback::HotSwap(*this, this->playback)) {
                ToastOverlay::Show(_TSTR("tracklist_hot_swap_success_toast"), 2000);
            }
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListRateTrack, key)) {
        TrackPtr track = this->GetSelectedTrack();
        if (track && !headers.HeaderAt(this->GetSelectedIndex())) {
            TrackOverlays::ShowRateTrackOverlay(
                track,
                this->library,
                [this](bool success) {
                    if (success && this->tracks) {
                        this->InvalidateData();
                    }
                });
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListNextGroup, key)) {
        const size_t next = this->headers.NextHeaderIndex(this->GetSelectedIndex());
        if (next != HeaderCalculator::NO_INDEX) {
            this->SetSelectedIndex(next);
            if (!IsEntryVisible(next)) {
                this->ScrollTo(next);
            }
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListPreviousGroup, key)) {
        const size_t prev = this->headers.PrevHeaderIndex(this->GetSelectedIndex());
        if (prev != HeaderCalculator::NO_INDEX) {
            this->SetSelectedIndex(prev);
            if (!IsEntryVisible(prev)) {
                this->ScrollTo(prev);
            }
        }
        handled = true;
    }
    else if (Hotkeys::Is(Hotkeys::TrackListPlayFromTop, key)) {
        playback::PlayFromTop(*this, this->playback);
        handled = true;
    }

    if (!handled) {
        handled = ListWindow::KeyPress(key);
    }

    if (handled) {
        this->lastChanged = now();

        this->Debounce(
            WINDOW_MESSAGE_SCROLL_TO_PLAYING,
            0, 0, AUTO_SCROLL_COOLDOWN.count());
    }

    return handled;
}

void TrackListView::OnTrackChanged(size_t index, musik::core::TrackPtr track) {
    this->playing = track;
    this->OnAdapterChanged();

    if (now() - lastChanged >= AUTO_SCROLL_COOLDOWN) {
        this->ScrollToPlaying();
    }
}

IScrollAdapter& TrackListView::GetScrollAdapter() noexcept {
    return *this->adapter;
}

TrackListView::Adapter::Adapter(TrackListView &parent)
: parent(parent) {
}

/* * * * TrackListView::HeaderCalculator * * * */

void TrackListView::HeaderCalculator::Set(Headers rawOffsets, Durations durations) {
    this->rawOffsets = rawOffsets;
    this->durations = durations;
    this->absoluteOffsets.reset();
    if (rawOffsets) {
        this->absoluteOffsets = std::make_shared<std::set<size_t>>();
        size_t i = 0;
        for (auto val : (*this->rawOffsets)) {
            this->absoluteOffsets->insert(val + i);
            i++;
        }
    }
}

void TrackListView::HeaderCalculator::Reset() noexcept {
    this->absoluteOffsets.reset();
    this->rawOffsets.reset();
}

size_t TrackListView::HeaderCalculator::AdapterToTrackListIndex(size_t index) {
    return this->ApplyHeaderOffset(index, this->absoluteOffsets, -1);
}

size_t TrackListView::HeaderCalculator::DurationFromAdapterIndex(size_t index) {
    /* ugh, HeaderCalculator should probably be re-thought, but this is especially ugly */
    const auto adjustedIndex = this->ApplyHeaderOffset(index + 1, this->absoluteOffsets, -1);
    const auto it = this->durations->find(adjustedIndex);
    if (it != this->durations->end()) {
        return it->second;
    }
    return 0;
}

size_t TrackListView::HeaderCalculator::TrackListToAdapterIndex(size_t index) {
    return this->ApplyHeaderOffset(index, this->rawOffsets, 1);
}

size_t TrackListView::HeaderCalculator::ApplyHeaderOffset(size_t index, Headers offsets, int delta) {
    size_t result = index;
    if (offsets) {
        for (auto offset : (*offsets)) {
            if (offset <= index) {
                result += delta;
            }
            else {
                break;
            }
        }
    }
    return result;
}

size_t TrackListView::HeaderCalculator::Count() noexcept {
    return this->absoluteOffsets ? this->absoluteOffsets->size() : 0;
}

bool TrackListView::HeaderCalculator::HeaderAt(size_t index) {
    return this->absoluteOffsets &&
        this->absoluteOffsets->find(index) != this->absoluteOffsets->end();
}

size_t TrackListView::HeaderCalculator::NextHeaderIndex(size_t selectedIndex) noexcept {
    size_t result = NO_INDEX;
    if (this->absoluteOffsets) {
        for (auto val : (*this->absoluteOffsets)) {
            if (val > selectedIndex) {
                result = val;
                break;
            }
        }
    }
    return result;
}

size_t TrackListView::HeaderCalculator::PrevHeaderIndex(size_t selectedIndex) noexcept {
    size_t result = 0;
    if (this->absoluteOffsets) {
        for (auto val : (*this->absoluteOffsets)) {
            if (selectedIndex > val) {
                result = val;
            }
            else {
                break;
            }
        }
    }
    return result;
}

/* * * * TrackListView::Adapter * * * */

size_t TrackListView::Adapter::GetEntryCount() noexcept {
    return parent.tracks ? parent.tracks->Count() + parent.headers.Count() : 0;
}

IScrollAdapter::EntryPtr TrackListView::Adapter::GetEntry(cursespp::ScrollableWindow* window, size_t rawIndex) {
    const bool selected = (rawIndex == parent.GetSelectedIndex());

    if (this->parent.headers.HeaderAt(rawIndex)) {
        /* the next track at the next logical index will have the album
        tracks we're interesetd in. */
        auto const trackIndex = this->parent.headers.AdapterToTrackListIndex(rawIndex + 1);
        TrackPtr track = parent.tracks->Get(trackIndex, sGetAsync);

        if (track) {
            std::string album = track->GetMetadataState() == MetadataState::Loaded
                ? track->GetString(constants::Track::ALBUM) : "-";

            if (!album.size()) {
                album = _TSTR("tracklist_unknown_album");
            }

            const auto duration = this->parent.headers.DurationFromAdapterIndex(rawIndex);
            if (duration > 0) {
                album += " - " + core::duration::DurationWithHours(duration);
            }

            album = text::Ellipsize(album, this->GetWidth());

            auto entry = std::make_shared<TrackListEntry>(
                album, narrow_cast<int>(trackIndex), RowType::Separator);

            entry->SetAttrs(selected
                ? Color::ListItemHeaderHighlighted
                : Color::ListItemHeader);

            return entry;
        }
    }

    const size_t trackIndex = this->parent.headers.AdapterToTrackListIndex(rawIndex);
    TrackPtr track = parent.tracks->Get(trackIndex, sGetAsync);

    Color attrs = Color::Default;

    if (parent.decorator) {
        attrs = parent.decorator(track, trackIndex);
    }
    else {
        attrs = selected ? Color::ListItemHighlighted : Color::Default;

        TrackPtr playing = parent.playing;
        if (playing &&
            playing->GetId() == track->GetId() &&
            playing->LibraryId() == track->LibraryId())
        {
            if (selected) {
                attrs = Color::ListItemHighlightedSelected;
            }
            else {
                attrs = Color::ListItemSelected;
            }
        }
    }

    if (!track) {
        auto entry = std::make_shared<SingleLineEntry>("  -");
        entry->SetAttrs(attrs);
        return entry;
    }

    if (track->GetMetadataState() != MetadataState::Loaded) {
        attrs = Color::TextDisabled;
    }

    std::string text = parent.renderer(
        track, rawIndex, this->GetWidth(), parent.trackNumType);

    auto entry = std::make_shared<TrackListEntry>(
        text, narrow_cast<int>(trackIndex), RowType::Track);

    entry->SetAttrs(attrs);

    return entry;
}
