#pragma once

#include "curses_config.h"

namespace cursespp {
    class Screen {
        private:
            Screen();

        public:
            static int GetWidth();
            static int GetHeight();
    };
}