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
#include <cursespp/SingleLineEntry.h>
#include <cursespp/IMessage.h>

#include "TrackListView.h"

#include <core/library/LocalLibraryConstants.h>

#include <app/util/Text.h>
#include <app/window/EntryWithHeader.h>

#include <boost/format.hpp>
#include <boost/format/group.hpp>
#include <boost/lexical_cast.hpp>

#include <iomanip>

#define WINDOW_MESSAGE_QUERY_COMPLETED 1002

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;
using boost::io::group;
using std::setw;
using std::setfill;
using std::setiosflags;

TrackListView::TrackListView(PlaybackService& playback, LibraryPtr library)
: ListWindow(NULL)
, playback(playback) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->library = library;
    this->library->QueryCompleted.connect(this, &TrackListView::OnQueryCompleted);
    this->playback.TrackChanged.connect(this, &TrackListView::OnTrackChanged);
    this->adapter = new Adapter(*this);
    this->lastQueryHash = 0;
}

TrackListView::~TrackListView() {

}

void TrackListView::Requery(std::shared_ptr<TrackListQueryBase> query) {
    this->query = query;
    this->library->Enqueue(this->query);
}

void TrackListView::OnQueryCompleted(QueryPtr query) {
    if (query == this->query) {
        this->PostMessage(WINDOW_MESSAGE_QUERY_COMPLETED);
    }
}

TrackListView::TrackList TrackListView::GetTrackList() {
    return this->metadata;
}

void TrackListView::Clear() {
    this->query.reset();
    this->metadata.reset(new std::vector<TrackPtr>());
    this->headers.reset(new std::set<size_t>());
    this->OnAdapterChanged();
}

void TrackListView::ProcessMessage(IMessage &message) {
    if (message.Type() == WINDOW_MESSAGE_QUERY_COMPLETED) {
        if (this->query && this->query->GetStatus() == IQuery::Finished) {
            this->metadata = this->query->GetResult();
            this->headers = this->query->GetHeaders();

            /* if the query was functionally the same as the last query, don't
            mess with the selected index */
            if (this->lastQueryHash != query->GetQueryHash()) {
                this->SetSelectedIndex(0);
            }

            this->lastQueryHash = this->query->GetQueryHash();
            this->query.reset();

            this->OnAdapterChanged(); /* internal handling */
            this->Requeried(); /* for external handlers */
        }
    }
}

void TrackListView::OnTrackChanged(size_t index, musik::core::TrackPtr track) {
    this->playing = track;
    this->OnAdapterChanged();
}

IScrollAdapter& TrackListView::GetScrollAdapter() {
    return *this->adapter;
}

TrackListView::Adapter::Adapter(TrackListView &parent)
: parent(parent) {
}

size_t TrackListView::Adapter::GetEntryCount() {
    return parent.metadata ? parent.metadata->size() : 0;
}

#define TRACK_COL_WIDTH 3
#define ARTIST_COL_WIDTH 14
#define ALBUM_COL_WIDTH 14
#define DURATION_COL_WIDTH 5 /* 00:00 */

/* so this part is a bit tricky... we draw multiple columns, but we use
standard std::setw() stuff, which is not aware of multi-byte characters.
so we have to manually adjust the widths (i.e. we can't just use simple
constants) */
#define DISPLAY_WIDTH(chars, str) \
    chars + (str.size() - u8len(str))

IScrollAdapter::EntryPtr TrackListView::Adapter::GetEntry(size_t index) {
    bool selected = index == parent.GetSelectedIndex();
    int64 attrs = selected
        ? COLOR_PAIR(BOX_COLOR_BLACK_ON_GREEN)
        : -1LL;

    TrackPtr track = parent.metadata->at(index);

    TrackPtr playing = parent.playing;
    if (playing &&
        playing->Id() == track->Id() &&
        playing->LibraryId() == track->LibraryId())
    {
        if (selected) {
            attrs = COLOR_PAIR(BOX_COLOR_BLACK_ON_YELLOW);
        }
        else {
            attrs = COLOR_PAIR(BOX_COLOR_YELLOW_ON_BLACK) | A_BOLD;
        }
    }

    std::string trackNum = track->GetValue(constants::Track::TRACK_NUM);
    std::string artist = track->GetValue(constants::Track::ARTIST);
    std::string album = track->GetValue(constants::Track::ALBUM);
    std::string title = track->GetValue(constants::Track::TITLE);
    std::string duration = track->GetValue(constants::Track::DURATION);

    int column0Width = DISPLAY_WIDTH(TRACK_COL_WIDTH, trackNum);
    int column2Width = DISPLAY_WIDTH(DURATION_COL_WIDTH, duration);
    int column3Width = DISPLAY_WIDTH(ARTIST_COL_WIDTH, artist);
    int column4Width = DISPLAY_WIDTH(ALBUM_COL_WIDTH, album);

    size_t column1CharacterCount =
        this->GetWidth() -
        column0Width -
        column2Width -
        column3Width -
        column4Width -
        (3 * 4); /* 3 = spacing */

    int column1Width = DISPLAY_WIDTH(column1CharacterCount, title);

    text::Ellipsize(artist, ARTIST_COL_WIDTH);
    text::Ellipsize(album, ALBUM_COL_WIDTH);
    text::Ellipsize(title, column1CharacterCount);
    duration = text::Duration(duration);

    std::string text = boost::str(
        boost::format("%s   %s   %s   %s   %s")
            % group(setw(column0Width), setfill(' '), trackNum)
            % group(setw(column1Width), setiosflags(std::ios::left), setfill(' '), title)
            % group(setw(column2Width), setiosflags(std::ios::right), setfill(' '), duration)
            % group(setw(column3Width), setiosflags(std::ios::left), setfill(' '), artist)
            % group(setw(column4Width), setiosflags(std::ios::left), setfill(' '), album));

    if (this->parent.headers->find(index) != this->parent.headers->end()) {
        std::string album = track->GetValue(constants::Track::ALBUM);
        std::shared_ptr<EntryWithHeader> entry(new EntryWithHeader(album, text));
        entry->SetAttrs(COLOR_PAIR(BOX_COLOR_GREEN_ON_BLACK), attrs);
        return entry;
    }
    else {
        std::shared_ptr<SingleLineEntry> entry(new SingleLineEntry(text));
        entry->SetAttrs(attrs);
        return entry;
    }
}
