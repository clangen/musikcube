#pragma once

#include <stdafx.h>

namespace cursespp {
    class IKeyHandler {
        public:
            virtual ~IKeyHandler() { }
            virtual bool KeyPress(const std::string& key) = 0;
    };
}
