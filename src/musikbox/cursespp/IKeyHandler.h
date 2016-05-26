#pragma once

#include <stdafx.h>

namespace cursespp {
    class IKeyHandler {
        public:
            virtual ~IKeyHandler() { }
            virtual bool KeyPress(int64 ch) = 0;
    };
}
