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
#include "TrackListView.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>
#include <cursespp/ToastOverlay.h>

#include <core/library/LocalLibraryConstants.h>
#include <core/audio/PlaybackService.h>

#include <core/support/Duration.h>

#include <app/util/Hotkeys.h>
#include <app/util/Playback.h>
#include <app/overlay/PlayQueueOverlays.h>

#include <boost/format.hpp>

#define WINDOW_MESSAGE_SCROLL_TO_PLAYING 1003

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

static inline milliseconds now() {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
}

TrackListView::TrackListView(
    PlaybackService& playback,
    ILibraryPtr library,
    RowFormatter formatter,
    RowDecorator decorator)
: ListWindow(nullptr)
, playback(playback) {
    this->library = library;
    this->library->QueryCompleted.connect(this, &TrackListView::OnQueryCompleted);
    this->playback.TrackChanged.connect(this, &TrackListView::OnTrackChanged);
    this->adapter = new Adapter(*this);
    this->lastQueryHash = 0;
    this->lastChanged = now();
    this->formatter = formatter;
    this->decorator = decorator;
    this->trackNumType = TrackNumType::Metadata;
}

TrackListView::~TrackListView() {
    delete this->adapter;
    this->adapter = nullptr;
}

void TrackListView::Requery(std::shared_ptr<TrackListQueryBase> query) {
    this->query = query;
    this->library->Enqueue(this->query);
}

void TrackListView::SelectFirstTrack() {
    this->SetSelectedIndex(this->headers.HeaderAt(0) ? 1 : 0);
}

void TrackListView::SetTrackNumType(TrackNumType type) {
    if (this->trackNumType != type) {
        this->trackNumType = type;
        this->OnAdapterChanged();
    }
}

void TrackListView::OnQueryCompleted(IQuery* query) {
    if (this->query && query == this->query.get()) {
        if (this->query->GetStatus() == IQuery::Finished) {
            bool hadTracks = this->tracks && this->tracks->Count() > 0;
            bool prevQuerySame = this->lastQueryHash == this->query->GetQueryHash();

            this->tracks = this->query->GetResult();
            this->headers.Set(this->query->GetHeaders());
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

std::shared_ptr<TrackList> TrackListView::GetTrackList() {
    return this->tracks;
}

void TrackListView::SetTrackList(std::shared_ptr<TrackList> trackList) {
    if (this->tracks != trackList) {
        this->tracks = trackList;
        this->ScrollToTop();
        this->SelectFirstTrack();
    }

    this->OnAdapterChanged();
}

void TrackListView::Clear() {
    this->query.reset();
    this->tracks.reset(new TrackList(this->library));
    this->headers.Reset();
    this->OnAdapterChanged();
}

TrackPtr TrackListView::GetSelectedTrack() {
    auto i = this->GetSelectedTrackIndex();
    return (i == ListWindow::NO_SELECTION) ? TrackPtr() : this->tracks->Get(i);
}

size_t TrackListView::GetSelectedTrackIndex() {
    auto i = this->GetSelectedIndex();
    if (i != ListWindow::NO_SELECTION) {
        auto entry = adapter->GetEntry(this, i);
        return static_cast<TrackListEntry*>(entry.get())->GetIndex();
    }
    return ListWindow::NO_SELECTION;
}

size_t TrackListView::TrackIndexToAdapterIndex(size_t index) {
    return this->headers.TrackListToAdapterIndex(index);
}

size_t TrackListView::TrackCount() {
    return this->tracks ? this->tracks->Count() : 0;
}

size_t TrackListView::EntryCount() {
    return this->GetScrollAdapter().GetEntryCount();
}

void TrackListView::ScrollToPlaying() {
    if (this->playing && this->tracks) {
        int64_t id = this->playing->GetId();
        for (size_t i = 0; i < this->tracks->Count(); i++) {
            if (this->tracks->GetId(i) == id) {
                size_t rawIndex = headers.TrackListToAdapterIndex(i);
                this->SetSelectedIndex(rawIndex);

                if (!this->IsEntryVisible(rawIndex)) {
                    this->ScrollTo(rawIndex);
                }

                return;
            }
        }
    }
}

void TrackListView::ProcessMessage(IMessage &message) {
    if (message.Type() == WINDOW_MESSAGE_SCROLL_TO_PLAYING) {
        this->ScrollToPlaying();
    }
}

void TrackListView::OnEntryActivated(size_t index) {
    if (headers.HeaderAt(this->GetSelectedIndex())) {
        TrackPtr track = this->GetSelectedTrack();
        PlayQueueOverlays::ShowAlbumDividerOverlay(
            MessageQueue(), this->playback, this->library, track);
    }
    else {
        playback::Play(*this, this->playback);
    }

    ListWindow::OnEntryActivated(index);
}

void TrackListView::OnEntryContextMenu(size_t index) {
    ListWindow::OnEntryContextMenu(index);
    this->ShowContextMenu();
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

    if (!handled) {
        handled = ListWindow::KeyPress(key);
    }

    if (handled) {
        this->lastChanged = now();

        this->DebounceMessage(
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

IScrollAdapter& TrackListView::GetScrollAdapter() {
    return *this->adapter;
}

TrackListView::Adapter::Adapter(TrackListView &parent)
: parent(parent) {
}

/* * * * TrackListView::HeaderCalculator * * * */

void TrackListView::HeaderCalculator::Set(Headers rawOffsets) {
    this->rawOffsets = rawOffsets;
    this->absoluteOffsets.reset();
    if (rawOffsets) {
        this->absoluteOffsets.reset(new std::set<size_t>());
        size_t i = 0;
        for (auto val : (*this->rawOffsets)) {
            this->absoluteOffsets->insert(val + i);
            i++;
        }
    }
}

void TrackListView::HeaderCalculator::Reset() {
    this->absoluteOffsets.reset();
    this->rawOffsets.reset();
}

size_t TrackListView::HeaderCalculator::AdapterToTrackListIndex(size_t index) {
    return this->ApplyHeaderOffset(index, this->absoluteOffsets, -1);
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

size_t TrackListView::HeaderCalculator::Count() {
    return this->absoluteOffsets ? this->absoluteOffsets->size() : 0;
}

bool TrackListView::HeaderCalculator::HeaderAt(size_t index) {
    return this->absoluteOffsets &&
        this->absoluteOffsets->find(index) != this->absoluteOffsets->end();
}

/* * * * TrackListView::Adapter * * * */

size_t TrackListView::Adapter::GetEntryCount() {
    return parent.tracks ? parent.tracks->Count() + parent.headers.Count() : 0;
}

#define TRACK_COL_WIDTH 3
#define ARTIST_COL_WIDTH 17
#define DURATION_COL_WIDTH 5 /* 00:00 */
#define DIGITS(x) (x > 9 ? (int) log10((double) x) + 1 : 1)

using TrackNumType = TrackListView::TrackNumType;

static std::string formatWithoutAlbum(TrackPtr track, size_t index, size_t width, TrackNumType type) {
    std::string trackNum;

    int trackColWidth = TRACK_COL_WIDTH;
    if (type == TrackNumType::Metadata) {
        trackNum = text::Align(
            track->GetString(constants::Track::TRACK_NUM),
            text::AlignRight,
            TRACK_COL_WIDTH);
    }
    else {
        trackColWidth = std::max(TRACK_COL_WIDTH, DIGITS(index + 1));
        trackNum = text::Align(std::to_string(index + 1), text::AlignRight, trackColWidth);
    }

    std::string duration = text::Align(
        musik::core::duration::Duration(track->GetString(constants::Track::DURATION)),
        text::AlignRight,
        DURATION_COL_WIDTH);

    std::string artist = text::Align(
        track->GetString(constants::Track::ARTIST),
        text::AlignLeft,
        ARTIST_COL_WIDTH);

    int titleWidth =
        width -
        trackColWidth -
        DURATION_COL_WIDTH -
        ARTIST_COL_WIDTH -
        (3 * 3); /* 3 = spacing */

    titleWidth = std::max(0, titleWidth);

    std::string title = text::Align(
        track->GetString(constants::Track::TITLE),
        text::AlignLeft,
        (int) titleWidth);

    return boost::str(
        boost::format("%s   %s   %s   %s")
        % trackNum % title % duration % artist);
}

IScrollAdapter::EntryPtr TrackListView::Adapter::GetEntry(cursespp::ScrollableWindow* window, size_t rawIndex) {
    bool selected = (rawIndex == parent.GetSelectedIndex());

    if (this->parent.headers.HeaderAt(rawIndex)) {
        /* the next track at the next logical index will have the album
        tracks we're interesetd in. */
        auto trackIndex = this->parent.headers.AdapterToTrackListIndex(rawIndex + 1);
        TrackPtr track = parent.tracks->Get(trackIndex);

        if (track) {
            std::string album = track->GetString(constants::Track::ALBUM);

            if (!album.size()) {
                album = _TSTR("tracklist_unknown_album");
            }

            album = text::Ellipsize(album, this->GetWidth());

            std::shared_ptr<TrackListEntry> entry(new
                TrackListEntry(album, trackIndex, RowType::Separator));

            entry->SetAttrs(selected
                ? COLOR_PAIR(CURSESPP_LIST_ITEM_HIGHLIGHTED_HEADER)
                : COLOR_PAIR(CURSESPP_LIST_ITEM_HEADER));

            return entry;
        }
    }

    size_t trackIndex = this->parent.headers.AdapterToTrackListIndex(rawIndex);
    TrackPtr track = parent.tracks->Get(trackIndex);

    if (!track) {
        auto entry = std::shared_ptr<SingleLineEntry>(new SingleLineEntry("track missing"));

        entry->SetAttrs(COLOR_PAIR(selected
            ? CURSESPP_HIGHLIGHTED_ERROR_LIST_ITEM : CURSESPP_TEXT_ERROR));

        return entry;
    }

    int64_t attrs = CURSESPP_DEFAULT_COLOR;

    if (parent.decorator) {
        attrs = parent.decorator(track, trackIndex);
    }
    else {
        attrs = selected
            ? COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM)
            : CURSESPP_DEFAULT_COLOR;

        TrackPtr playing = parent.playing;
        if (playing &&
            playing->GetId() == track->GetId() &&
            playing->LibraryId() == track->LibraryId())
        {
            if (selected) {
                attrs = COLOR_PAIR(CURSESPP_HIGHLIGHTED_SELECTED_LIST_ITEM);
            }
            else {
                attrs = COLOR_PAIR(CURSESPP_SELECTED_LIST_ITEM);
            }
        }
    }

    std::string text = parent.formatter
        ? parent.formatter(track, rawIndex, this->GetWidth())
        : formatWithoutAlbum(track, rawIndex, this->GetWidth(), parent.trackNumType);

    std::shared_ptr<TrackListEntry> entry(
        new TrackListEntry(text, trackIndex, RowType::Track));

    entry->SetAttrs(attrs);

    return entry;
}
