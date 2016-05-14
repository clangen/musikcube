#pragma once

#include "IWindow.h"

class IWindowGroup {
    public:
        virtual bool AddWindow(IWindowPtr window) = 0;
        virtual bool RemoveWindow(IWindowPtr window) = 0;
        virtual size_t GetWindowCount() = 0;
        virtual IWindowPtr GetWindowAt(size_t position) = 0;
};