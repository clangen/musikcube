#pragma once

#include "IWindowGroup.h"
#include "IDisplayable.h"

class ILayout : public IWindowGroup, public IDisplayable {
    public:
        virtual IWindow* FocusNext() = 0;
        virtual IWindow* FocusPrev() = 0;
        virtual IWindow* GetFocus() = 0;
        virtual void Layout() = 0;
};