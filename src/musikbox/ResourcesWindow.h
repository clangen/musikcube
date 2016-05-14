#pragma once

#include "curses_config.h"
#include "Window.h"
#include "SystemInfo.h"

class ResourcesWindow : public Window {
    public:
        ResourcesWindow(IWindow *parent = NULL);
        virtual ~ResourcesWindow();

        virtual void Repaint();

    private:

        SystemInfo* systemInfo;
};