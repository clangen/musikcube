#pragma once

#include "IWindowGroup.h"

class ILayout : public IWindowGroup {
    public:
        virtual IWindow* FocusNext() = 0;
        virtual IWindow* FocusPrev() = 0;
        virtual IWindow* GetFocus() = 0;
        virtual void Layout() = 0;
        virtual void Hide() = 0;
        virtual void Show() = 0;

        virtual void OnIdle() = 0;
};