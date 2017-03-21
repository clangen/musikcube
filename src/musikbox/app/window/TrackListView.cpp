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
#include "TrackListView.h"

#include <cursespp/Colors.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

#include <core/library/LocalLibraryConstants.h>

#include <glue/util/Duration.h>

#include <app/util/Hotkeys.h>
#include <app/overlay/PlayQueueOverlays.h>

#include <boost/format.hpp>

#define WINDOW_MESSAGE_SCROLL_TO_PLAYING 1003

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::runtime;
using namespace musik::core::sdk;
using namespace musik::box;
using namespace musik::glue;
using namespace cursespp;

using namespace std::chrono;

/* if the user hasn't changed the selected index in 60 seconds
we assume he's not paying attention, and will automatically scroll
the view to the playing track if it's invisible. */
static const milliseconds AUTO_SCROLL_COOLDOWN = milliseconds(60000LL);

static inline milliseconds now() {
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch());
}

static IScrollAdapter::EntryPtr MISSING_ENTRY = IScrollAdapter::EntryPtr();

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

    if (!MISSING_ENTRY) {
        auto e = std::shared_ptr<SingleLineEntry>(new SingleLineEntry("track missing"));
        e->SetAttrs(COLOR_PAIR(CURSESPP_TEXT_ERROR));
        MISSING_ENTRY = e;
    }
}

TrackListView::~TrackListView() {
    delete this->adapter;
    this->adapter = nullptr;
}

void TrackListView::Requery(std::shared_ptr<TrackListQueryBase> query) {
    this->query = query;
    this->library->Enqueue(this->query);
}

void TrackListView::OnQueryCompleted(IQuery* query) {
    if (this->query && query == this->query.get()) {
        if (this->query->GetStatus() == IQuery::Finished) {
            this->tracks = this->query->GetResult();
            this->headers.Set(this->query->GetHeaders());

            /* if the query was functionally the same as the last query, don't
            mess with the selected index */
            if (this->lastQueryHash != this->query->GetQueryHash()) {
                this->ScrollToTop();
                this->SetSelectedIndex(this->headers.HeaderAt(0) ? 1 : 0);
            }

            this->lastQueryHash = this->query->GetQueryHash();

            this->OnAdapterChanged(); /* internal handling */
            this->Requeried(this->query.get()); /* for external handlers */

            this->query.reset();
        }
    }
}

std::shared_ptr<const TrackList> TrackListView::GetTrackList() {
    return this->tracks;
}

void TrackListView::SetTrackList(std::shared_ptr<const TrackList> trackList) {
    if (this->tracks != trackList) {
        this->tracks = trackList;
        this->SetSelectedIndex(0);
        this->ScrollToTop();
        this->OnAdapterChanged();
    }
    else {
        this->OnAdapterChanged();
    }
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

size_t TrackListView::Count() {
    return this->tracks ? this->tracks->Count() : 0;
}

void TrackListView::ScrollToPlaying() {
    if (this->playing && this->tracks) {
        DBID id = this->playing->GetId();
        for (size_t i = 0; i < this->tracks->Count(); i++) {
            if (this->tracks->GetId(i) == id) {
                this->SetSelectedIndex(i);

                auto pos = this->GetScrollPosition();
                size_t first = pos.firstVisibleEntryIndex;
                size_t last = first + pos.visibleEntryCount;
                if (i < first || i >= last) {
                    this->ScrollTo(i);
                }
                break;
            }
        }
    }
}

void TrackListView::ProcessMessage(IMessage &message) {
    if (message.Type() == WINDOW_MESSAGE_SCROLL_TO_PLAYING) {
        this->ScrollToPlaying();
    }
}

bool TrackListView::KeyPress(const std::string& key) {
    bool handled = false;

    if (key == "KEY_ENTER") {
        if (headers.HeaderAt(this->GetSelectedIndex())) {
            auto track = this->GetSelectedTrack();
            unsigned long long fieldId = std::stoull(
                track->GetValue(constants::Track::ALBUM_ID));

            PlayQueueOverlays::ShowAddCategoryOverlay(
                this->playback,
                this->library,
                constants::Track::ALBUM,
                fieldId);

            handled = true;
        }
    }
    else if (Hotkeys::Is(Hotkeys::NavigateJumpToPlaying, key)) {
        this->ScrollToPlaying();
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

    if (now() - lastChanged >= AUTO_SCROLL_COOLDOWN)
    {
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

size_t TrackListView::HeaderCalculator::OffsetTrackIndex(size_t index) {
    size_t result = index;
    if (this->absoluteOffsets) {
        for (auto offset : (*this->absoluteOffsets)) {
            if (result != 0 && offset <= index) {
                --result;
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

static std::string formatWithoutAlbum(TrackPtr track, size_t width) {
    std::string trackNum = text::Align(
        track->GetValue(constants::Track::TRACK_NUM),
        text::AlignRight,
        TRACK_COL_WIDTH);

    std::string duration = text::Align(
        musik::glue::duration::Duration(track->GetValue(constants::Track::DURATION)),
        text::AlignRight,
        DURATION_COL_WIDTH);

    std::string artist = text::Align(
        track->GetValue(constants::Track::ARTIST),
        text::AlignLeft,
        ARTIST_COL_WIDTH);

    int titleWidth =
        width -
        TRACK_COL_WIDTH -
        DURATION_COL_WIDTH -
        ARTIST_COL_WIDTH -
        (3 * 3); /* 3 = spacing */

    titleWidth = std::max(0, titleWidth);

    std::string title = text::Align(
        track->GetValue(constants::Track::TITLE),
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
        auto trackIndex = this->parent.headers.OffsetTrackIndex(rawIndex + 1);
        TrackPtr track = parent.tracks->Get(trackIndex);
        std::string album = track->GetValue(constants::Track::ALBUM);

        std::shared_ptr<TrackListEntry> entry(new
            TrackListEntry(album, trackIndex, RowType::Separator));

        entry->SetAttrs(selected
            ? COLOR_PAIR(CURSESPP_LIST_ITEM_HIGHLIGHTED_HEADER)
            : COLOR_PAIR(CURSESPP_LIST_ITEM_HEADER));

        return entry;
    }

    size_t trackIndex = this->parent.headers.OffsetTrackIndex(rawIndex);
    TrackPtr track = parent.tracks->Get(trackIndex);

    if (!track) {
        return MISSING_ENTRY;
    }

    int64 attrs = CURSESPP_DEFAULT_COLOR;

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
        ? parent.formatter(track, this->GetWidth())
        : formatWithoutAlbum(track, this->GetWidth());

    std::shared_ptr<TrackListEntry> entry(
        new TrackListEntry(text, trackIndex, RowType::Track));

    entry->SetAttrs(attrs);

    return entry;
}
