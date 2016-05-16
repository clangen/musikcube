#pragma once

#include "stdafx.h"
#include "TransportWindow.h"
#include "Screen.h"
#include "Colors.h"
#include "WindowMessage.h"

#include <core/debug.h>
#include <core/sdk/IPlugin.h>
#include <core/plugin/PluginFactory.h>
#include <core/library/track/TrackFactory.h>
#include <core/library/LibraryFactory.h>
#include <core/library/Indexer.h>

#include <boost/format.hpp>

#include <boost/algorithm/string.hpp>

#define MESSAGE_TYPE_UPDATE 1001
#define UPDATE_INTERVAL_MS 500

using musik::core::audio::Transport;

TransportWindow::TransportWindow(IWindow *parent, Transport& transport)
: Window(parent) {
    this->SetContentColor(BOX_COLOR_BLACK_ON_GREEN);
    this->transport = &transport;
    this->paused = false;
}

TransportWindow::~TransportWindow() {
}

void TransportWindow::Show() {
    Window::Show();
    this->UpdateAndRepaint();
    Post(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
}

void TransportWindow::ProcessMessage(IWindowMessage &message) {
    if (message.MessageType() == MESSAGE_TYPE_UPDATE) {
        this->UpdateAndRepaint();
        Post(MESSAGE_TYPE_UPDATE, 0, 0, UPDATE_INTERVAL_MS);
    }
}

void TransportWindow::UpdateAndRepaint() {
    this->Clear();
    WINDOW *c = this->GetContent();

    double volume = (this->transport->Volume() * 100.0);

    wprintw(c, "volume %.1f%%\n", volume);
    wprintw(c, "filename: ");

    this->Repaint();
}