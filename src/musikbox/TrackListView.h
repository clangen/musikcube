#pragma once

#include "curses_config.h"
#include "Window.h"

class TrackListView : public Window {
    public:
        TrackListView(IWindow *parent = NULL);
        ~TrackListView();

    private:
};