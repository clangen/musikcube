#pragma once

#include "curses_config.h"
#include "Window.h"
#include "OutputWindow.h"
#include <core/playback/Transport.h>

using namespace musik::core::audio;

class TransportWindow : public Window {
    public:
        TransportWindow(IWindow *parent, Transport& transport);
        ~TransportWindow();

        void Update();

    private:
        bool paused;
        Transport* transport;
};