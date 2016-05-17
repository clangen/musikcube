#pragma once

#include <stdafx.h>

class IKeyHandler {
    public:
        virtual void KeyPress(int64 ch) = 0;
};