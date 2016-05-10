#pragma once

#include "stdafx.h"

class IInput {
    public:
        virtual void WriteChar(int ch) = 0;
        virtual void Focus() = 0;
};