#pragma once

#include "curses_config.h"
#include "BorderedWindow.h"
#include "SystemInfo.h"

class ResourcesWindow : public BorderedWindow {
    public:
        ResourcesWindow();
        virtual ~ResourcesWindow();

        virtual void Repaint();

    private:

        SystemInfo* systemInfo;
};