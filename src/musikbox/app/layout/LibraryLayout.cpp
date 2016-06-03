#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <core/library/LocalLibraryConstants.h>

#include <app/query/CategoryTrackListQuery.h>

#include "LibraryLayout.h"

using namespace musik::core::library::constants;

#define CATEGORY_WIDTH 25

#ifdef WIN32
    #define TRANSPORT_HEIGHT 3
#else
    #define TRANSPORT_HEIGHT 2
#endif

#define DEFAULT_CATEGORY constants::Track::ARTIST_ID

using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::library;
using namespace musik::box;
using namespace cursespp;

LibraryLayout::LibraryLayout(PlaybackService& playback, LibraryPtr library)
: LayoutBase()
, playback(playback)
, transport(playback.GetTransport()) {
    this->library = library;
    this->InitializeWindows();
}

LibraryLayout::~LibraryLayout() {

}

void LibraryLayout::Layout() {
    int x = 0, y = 0;
    int cx = Screen::GetWidth(), cy = Screen::GetHeight();

    this->MoveAndResize(x, y, cx, cy);

    this->browseLayout->MoveAndResize(x, y, cx, cy - TRANSPORT_HEIGHT);
    this->nowPlayingLayout->MoveAndResize(x, y, cx, cy - TRANSPORT_HEIGHT);

    this->transportView->MoveAndResize(
        1,
        cy - TRANSPORT_HEIGHT,
        cx - 2,
        TRANSPORT_HEIGHT);

    this->ShowBrowse();
}

void LibraryLayout::ShowNowPlaying() {
    if (this->focusedLayout != this->nowPlayingLayout) {
        this->AddWindow(this->nowPlayingLayout);
        this->RemoveWindow(this->browseLayout);
        this->focusedLayout = this->nowPlayingLayout;
        this->nowPlayingLayout->Layout();
        this->nowPlayingLayout->Show();
        this->BringToTop();
    }
}

void LibraryLayout::ShowBrowse() {
    if (this->focusedLayout != this->browseLayout) {
        this->RemoveWindow(this->nowPlayingLayout);
        this->AddWindow(this->browseLayout);
        this->focusedLayout = this->browseLayout;
        this->browseLayout->Layout();
        this->BringToTop();
    }
}

void LibraryLayout::InitializeWindows() {
    this->browseLayout.reset(new BrowseLayout(this->playback, this->library));
    this->nowPlayingLayout.reset(new NowPlayingLayout(this->playback, this->library));
    this->transportView.reset(new TransportWindow(this->playback));

    this->AddWindow(this->transportView);

    this->Layout();
}

void LibraryLayout::Show() {
    LayoutBase::Show();
    this->transportView->Update();
}

IWindowPtr LibraryLayout::FocusNext() {
    return this->focusedLayout->FocusNext();
}

IWindowPtr LibraryLayout::FocusPrev() {
    return this->focusedLayout->FocusPrev();
}

IWindowPtr LibraryLayout::GetFocus() {
    return this->focusedLayout->GetFocus();
}

bool LibraryLayout::KeyPress(const std::string& key) {
    if (key == "^N") {
        this->ShowNowPlaying();
        return true;
    }
    else if (key == "^[") {
        this->ShowBrowse();
        return true;
    }
    else if (this->focusedLayout && this->focusedLayout->KeyPress(key)) {
        return true;
    }
    else if (key == " ") {
        /* copied from GlobalHotkeys. should probably be generalized
        at some point. */
        int state = this->transport.GetPlaybackState();
        if (state == Transport::PlaybackPaused) {
            this->transport.Resume();
        }
        else if (state == Transport::PlaybackPlaying) {
            this->transport.Pause();
        }
    }

    return LayoutBase::KeyPress(key);
}
