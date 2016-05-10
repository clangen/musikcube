#pragma once

#include "IWindow.h"

class ILayout {
    virtual IWindow* focusNext() = 0;
    virtual IWindow* focusPrev() = 0;
};