#pragma once

#include <stdafx.h>

namespace cursespp {
    class IKeyHandler {
        public:
            virtual ~IKeyHandler() = 0 { }
            virtual bool KeyPress(int64 ch) = 0;
    };
}