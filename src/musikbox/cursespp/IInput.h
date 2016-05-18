#pragma once

#include <stdafx.h>

class IInput {
    public:
        virtual ~IInput() = 0 { }

        virtual void WriteChar(int64 ch) = 0;
};