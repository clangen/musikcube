#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <core/library/LocalLibraryConstants.h>

#include <app/query/NowPlayingTrackListQuery.h>

#include "NowPlayingLayout.h"

using namespace musik::core::library::constants;

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

NowPlayingLayout::NowPlayingLayout(
    PlaybackService& playback,
    musik::core::LibraryPtr library)
: LayoutBase()
, playback(playback)
, library(library) {
    this->InitializeWindows();
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
    this->trackList.reset(new TrackListView(this->playback, this->library));
    this->AddWindow(this->trackList);
    this->Layout();
}

IWindowPtr NowPlayingLayout::GetFocus() {
    return this->trackList;
}

void NowPlayingLayout::Show() {
    LayoutBase::Show();
    this->RequeryTrackList();
}

void NowPlayingLayout::RequeryTrackList() {
    this->trackList->Requery(std::shared_ptr<TrackListQueryBase>(
        new NowPlayingTrackListQuery(this->playback)));
}

bool NowPlayingLayout::KeyPress(const std::string& key) {
    if (key == "^M") { /* enter. play the selection */
        this->playback.Play(this->trackList->GetSelectedIndex());
        return true;
    }

    return LayoutBase::KeyPress(key);
}