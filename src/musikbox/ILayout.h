#pragma once

#include "IWindow.h"

class ILayout {
    public:
        virtual IWindow* FocusNext() = 0;
        virtual IWindow* FocusPrev() = 0;
        virtual IWindow* GetFocus() = 0;
        virtual void Layout() = 0;
        virtual void OnIdle() = 0;
};