#pragma once

#include "stdafx.h"
#include "TransportWindow.h"

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/WindowMessage.h>

#include <app/util/Text.h>

#include <core/debug.h>
#include <core/library/track/LibraryTrack.h>
#include <core/playback/NonLibraryTrackHelper.h>
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

using musik::core::audio::Transport;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::NonLibraryTrackHelper;
using musik::core::QueryPtr;
using namespace musik::box;

#define REFRESH_TRANSPORT_READOUT 1001
#define REFRESH_INTERVAL_MS 1000

#define SCHEDULE_REFRESH(x) \
    this->RemoveMessage(REFRESH_TRANSPORT_READOUT); \
    this->PostMessage(REFRESH_TRANSPORT_READOUT, 0, 0, x);

TransportWindow::TransportWindow(LibraryPtr library, Transport& transport)
: Window(NULL) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->SetFrameVisible(false);
    this->library = library;
    this->library->QueryCompleted.connect(this, &TransportWindow::OnQueryCompleted);
    this->transport = &transport;
    this->transport->PlaybackEvent.connect(this, &TransportWindow::OnTransportPlaybackEvent);
    this->transport->VolumeChanged.connect(this, &TransportWindow::OnTransportVolumeChanged);
    this->paused = false;
}

TransportWindow::~TransportWindow() {
}

void TransportWindow::Show() {
    Window::Show();
    this->Update();
}

void TransportWindow::ProcessMessage(IWindowMessage &message) {
    int type = message.MessageType();
    
    if (type == REFRESH_TRANSPORT_READOUT) {
        this->Update();
        SCHEDULE_REFRESH(REFRESH_INTERVAL_MS)
    }
}

void TransportWindow::OnTransportPlaybackEvent(int eventType, std::string url) {
    if (eventType == Transport::EventStarted) {
        this->trackQuery.reset(new SingleTrackQuery(url));
        this->library->Enqueue(this->trackQuery);
        SCHEDULE_REFRESH(0)
    }
}

void TransportWindow::OnTransportVolumeChanged() {
    SCHEDULE_REFRESH(0)
}

void TransportWindow::OnQueryCompleted(QueryPtr query) {
    if (query == this->trackQuery && query->GetStatus() == QueryBase::Finished) {
        this->currentTrack = this->trackQuery->GetResult();
        SCHEDULE_REFRESH(0)
    }
}

void TransportWindow::Update() {
    this->Clear();
    WINDOW *c = this->GetContent();

    int64 gb = COLOR_PAIR(BOX_COLOR_GREEN_ON_BLACK);

    /* playing SONG TITLE from ALBUM NAME */

    std::string title, album, duration;
    
    if (this->currentTrack) {
        title = this->currentTrack->GetValue("title");
        album = this->currentTrack->GetValue("album");
        duration = this->currentTrack->GetValue("duration");
    }
    
    title = title.size() ? title : "song title";
    album = album.size() ? album : "album name";
    duration = duration.size() ? duration : "0";

    wprintw(c, "playing ");

    wattron(c, gb);
    wprintw(c, title.c_str());
    wattroff(c, gb);

    wprintw(c, " from ");

    wattron(c, gb);
    wprintw(c, album.c_str());
    wattroff(c, gb);

    /* volume slider */

    wprintw(c, "\n");
    
    int volumePercent = (size_t) round(this->transport->Volume() * 100.0f) - 1;
    int thumbOffset = min(9, (volumePercent * 10) / 100);

    std::string volume = "vol ";

    for (int i = 0; i < 10; i++) {
        volume += (i == thumbOffset) ? "■" : "─";
    }

    volume += "  ";

    wprintw(c, volume.c_str());

    /* time slider */

    transport->Position();

    int secondsCurrent = (int) round(transport->Position());
    int secondsTotal = boost::lexical_cast<int>(duration);

    std::string currentTime = text::Duration(secondsCurrent);
    std::string totalTime = text::Duration(secondsTotal);

    size_t timerWidth =
        this->GetContentWidth() -
        u8len(volume) -
        currentTime.size() -
        totalTime.size() -
        2; /* padding */

    thumbOffset = 0;

    if (secondsTotal) {
        size_t progress = (secondsCurrent * 100) / secondsTotal;
        thumbOffset = min(timerWidth - 1, (progress * timerWidth) / 100);
    }

    std::string timerTrack = "";
    for (size_t i = 0; i < timerWidth; i++) {
        timerTrack += (i == thumbOffset) ? "■" : "─";
    }

    wprintw(c, "%s %s %s", 
        currentTime.c_str(), 
        timerTrack.c_str(), 
        totalTime.c_str());

    this->Repaint();
}