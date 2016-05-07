#pragma once

#include "stdafx.h"
#include "BorderedWindow.h"
#include "OutputWindow.h"
#include <core/playback/Transport.h>

using namespace musik::core::audio;

class TransportWindow : public BorderedWindow {
    public:
        TransportWindow(Transport& transport);
        ~TransportWindow();

        virtual void Repaint();

    private:

        bool paused;
        Transport* transport;
};