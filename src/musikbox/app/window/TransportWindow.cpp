#pragma once

#include "stdafx.h"
#include "TransportWindow.h"

#include <cursespp/Screen.h>
#include <cursespp/Colors.h>
#include <cursespp/WindowMessage.h>

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

#define UPDATE_MESSAGE_TYPE 1000
#define UPDATE_INTERVAL_MS 250

TransportWindow::TransportWindow(LibraryPtr library, Transport& transport)
: Window(NULL) {
    this->SetContentColor(BOX_COLOR_WHITE_ON_BLACK);
    this->SetFrameVisible(false);
    this->library = library;
    this->library->QueryCompleted.connect(this, &TransportWindow::OnQueryCompleted);
    this->transport = &transport;
    this->transport->TrackStarted.connect(this, &TransportWindow::OnTransportStarted);
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
    if (message.MessageType() == UPDATE_MESSAGE_TYPE) {
        this->Update();
    }
}

void TransportWindow::OnTransportStarted(std::string url) {
    this->trackQuery.reset(new SingleTrackQuery(url));
    this->library->Enqueue(this->trackQuery);
}

void TransportWindow::OnTransportVolumeChanged() {
    Post(UPDATE_MESSAGE_TYPE);
}

void TransportWindow::OnQueryCompleted(QueryPtr query) {
    if (query == this->trackQuery && query->GetStatus() == QueryBase::Finished) {
        this->currentTrack = this->trackQuery->GetResult();
        Post(UPDATE_MESSAGE_TYPE);
    }
}

void TransportWindow::Update() {
    this->Clear();
    WINDOW *c = this->GetContent();

    double volume = (this->transport->Volume() * 100.0);

    int64 gb = COLOR_PAIR(BOX_COLOR_GREEN_ON_BLACK);

    std::string title, album;
    
    if (this->currentTrack) {
        title = this->currentTrack->GetValue("title");
        album = this->currentTrack->GetValue("album");
    }
    
    title = title.size() ? title : "song title";
    album = album.size() ? album : "album name";
    
    wprintw(c, "playing ");

    wattron(c, gb);
    wprintw(c, title.c_str());
    wattroff(c, gb);

    wprintw(c, " from ");

    wattron(c, gb);
    wprintw(c, album.c_str());
    wattroff(c, gb);
    
    wprintw(c, "\n");
    wprintw(c, "volume %.1f%%", volume);

    this->Repaint();
}