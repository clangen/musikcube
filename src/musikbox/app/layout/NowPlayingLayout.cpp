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
#include <cursespp/Text.h>
#include <core/library/LocalLibraryConstants.h>
#include <app/query/NowPlayingTrackListQuery.h>
#include <app/util/Duration.h>
#include "NowPlayingLayout.h"

#include <boost/format.hpp>
#include <boost/format/group.hpp>
#include <boost/lexical_cast.hpp>

#include <iomanip>

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

using boost::io::group;
using std::setw;
using std::setfill;
using std::setiosflags;

static std::string formatWithAlbum(TrackPtr track, size_t width);

NowPlayingLayout::NowPlayingLayout(
    PlaybackService& playback,
    musik::core::LibraryPtr library)
: LayoutBase()
, playback(playback)
, library(library) {
    this->InitializeWindows();
    this->playback.Shuffled.connect(this, &NowPlayingLayout::OnPlaybackShuffled);
}

NowPlayingLayout::~NowPlayingLayout() {

}

void NowPlayingLayout::Layout() {
    size_t cx = this->GetWidth(), cy = this->GetHeight();

    if (cx && cy) {
        this->trackList->MoveAndResize(
            0,
            0,
            this->GetWidth(),
            this->GetHeight());

        this->trackList->SetFocusOrder(1);
    }
}

void NowPlayingLayout::InitializeWindows() {
    this->trackList.reset(new TrackListView(
        this->playback,
        this->library,
        std::bind(formatWithAlbum, std::placeholders::_1, std::placeholders::_2)));

    this->trackList->Requeried.connect(this, &NowPlayingLayout::OnTrackListRequeried);
    this->AddWindow(this->trackList);
    this->Layout();
}

IWindowPtr NowPlayingLayout::GetFocus() {
    return this->trackList;
}

void NowPlayingLayout::OnVisibilityChanged(bool visible) {
    LayoutBase::OnVisibilityChanged(visible);

    if (visible) {
        this->RequeryTrackList();
    }
    else {
        this->trackList->Clear();
    }
}

void NowPlayingLayout::OnTrackListRequeried() {
    if (playback.Count()) {
        size_t index = playback.GetIndex();
        this->trackList->SetSelectedIndex(index);
        this->trackList->ScrollTo(index == 0 ? index : index - 1);
    }
}

void NowPlayingLayout::OnPlaybackShuffled(bool shuffled) {
    this->RequeryTrackList();
}

void NowPlayingLayout::RequeryTrackList() {
    this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
        new NowPlayingTrackListQuery(this->library, this->playback)));
}

bool NowPlayingLayout::KeyPress(const std::string& key) {
    if (key == "^M") { /* enter. play the selection */
        this->playback.Play(this->trackList->GetSelectedIndex());
        return true;
    }

    return LayoutBase::KeyPress(key);
}

#define TRACK_COL_WIDTH 3
#define ARTIST_COL_WIDTH 14
#define ALBUM_COL_WIDTH 14
#define DURATION_COL_WIDTH 5 /* 00:00 */

/* see TrackListView.cpp for more info */
#define DISPLAY_WIDTH(chars, str) \
    chars + (str.size() - u8len(str))

static std::string formatWithAlbum(TrackPtr track, size_t width) {
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
        width -
        column0Width -
        column2Width -
        column3Width -
        column4Width -
        (3 * 4); /* 3 = spacing */

    int column1Width = DISPLAY_WIDTH(column1CharacterCount, title);

    text::Ellipsize(artist, ARTIST_COL_WIDTH);
    text::Ellipsize(album, ALBUM_COL_WIDTH);
    text::Ellipsize(title, column1CharacterCount);
    duration = duration::Duration(duration);

    return boost::str(
        boost::format("%s   %s   %s   %s   %s")
        % group(setw(column0Width), setfill(' '), trackNum)
        % group(setw(column1Width), setiosflags(std::ios::left), setfill(' '), title)
        % group(setw(column2Width), setiosflags(std::ios::right), setfill(' '), duration)
        % group(setw(column3Width), setiosflags(std::ios::left), setfill(' '), artist)
        % group(setw(column4Width), setiosflags(std::ios::left), setfill(' '), album));
}