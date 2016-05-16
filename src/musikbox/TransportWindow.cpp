#pragma once

#include "stdafx.h"
#include "TransportWindow.h"
#include "Screen.h"
#include "Colors.h"
#include "WindowMessage.h"

#include <core/debug.h>

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>

using musik::core::audio::Transport;

TransportWindow::TransportWindow(IWindow *parent, Transport& transport)
: Window(parent) {
    this->SetContentColor(BOX_COLOR_BLACK_ON_GREEN);
    this->transport = &transport;
    this->paused = false;
}

TransportWindow::~TransportWindow() {
}

void TransportWindow::Update() {
    this->Clear();
    WINDOW *c = this->GetContent();

    double volume = (this->transport->Volume() * 100.0);

    wprintw(c, "volume %.1f%%\n", volume);
    wprintw(c, "filename: ");

    this->Repaint();
}