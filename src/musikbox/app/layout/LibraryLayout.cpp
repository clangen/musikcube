#include "stdafx.h"

#include <cursespp/Colors.h>
#include <cursespp/Screen.h>

#include <core/library/LocalLibraryConstants.h>

#include <app/query/CategoryTrackListQuery.h>

#include "LibraryLayout.h"

using namespace musik::core::library::constants;

#ifdef WIN32
    #define TRANSPORT_HEIGHT 3
#else
    #define TRANSPORT_HEIGHT 2
#endif

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
    this->browseLayout->Layout();
    this->nowPlayingLayout->MoveAndResize(x, y, cx, cy - TRANSPORT_HEIGHT);
    this->nowPlayingLayout->Layout();

    this->transportView->MoveAndResize(
        1,
        cy - TRANSPORT_HEIGHT,
        cx - 2,
        TRANSPORT_HEIGHT);

    if (!this->visibleLayout) {
        this->ShowBrowse();
    }
}

void LibraryLayout::ChangeMainLayout(std::shared_ptr<cursespp::LayoutBase> newLayout) {
    if (this->visibleLayout != newLayout) {
        if (this->visibleLayout) {
           this->RemoveWindow(this->visibleLayout);
           this->visibleLayout->Hide();
        }

        this->visibleLayout = newLayout;
        this->AddWindow(this->visibleLayout);
        this->visibleLayout->Layout();
        this->visibleLayout->Show();

        if (this->IsVisible()) {
            this->BringToTop();
        }
    }
}

void LibraryLayout::ShowNowPlaying() {
    this->ChangeMainLayout(this->nowPlayingLayout);
}

void LibraryLayout::ShowBrowse() {
    this->ChangeMainLayout(this->browseLayout);
}

void LibraryLayout::InitializeWindows() {
    this->browseLayout.reset(new BrowseLayout(this->playback, this->library));
    this->nowPlayingLayout.reset(new NowPlayingLayout(this->playback, this->library));
    this->transportView.reset(new TransportWindow(this->playback));

    this->AddWindow(this->transportView);

    this->Layout();
}

IWindowPtr LibraryLayout::FocusNext() {
    return this->visibleLayout->FocusNext();
}

IWindowPtr LibraryLayout::FocusPrev() {
    return this->visibleLayout->FocusPrev();
}

IWindowPtr LibraryLayout::GetFocus() {
    return this->visibleLayout->GetFocus();
}

bool LibraryLayout::KeyPress(const std::string& key) {
    if (key == "^[" || key == "M-n") { /* escape switches between browse/now playing */
        (this->visibleLayout == this->nowPlayingLayout)
            ? this->ShowBrowse() : this->ShowNowPlaying();
    }
    /* forward to the visible layout */
    else if (this->visibleLayout && this->visibleLayout->KeyPress(key)) {
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
