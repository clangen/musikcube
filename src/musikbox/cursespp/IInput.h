#pragma once

#include <stdafx.h>

class IInput {
    public:
        virtual void WriteChar(int64 ch) = 0;
};