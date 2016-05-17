#pragma once

#include <cursespp/Window.h>
#include <core/playback/Transport.h>
#include "OutputWindow.h"

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