#pragma once

#include "curses_config.h"
#include "Window.h"
#include "SystemInfo.h"

class ResourcesWindow : public Window {
    public:
        ResourcesWindow();
        virtual ~ResourcesWindow();

        virtual void Repaint();

    private:

        SystemInfo* systemInfo;
};