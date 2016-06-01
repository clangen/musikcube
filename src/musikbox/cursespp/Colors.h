#pragma once

#include "curses_config.h"

#define BOX_COLOR_WHITE_ON_BLUE 1
#define BOX_COLOR_RED_ON_BLUE 2
#define BOX_COLOR_YELLOW_ON_BLUE 3
#define BOX_COLOR_BLACK_ON_GREY 4
#define BOX_COLOR_BLACK_ON_GREEN 5
#define BOX_COLOR_YELLOW_ON_BLACK 6
#define BOX_COLOR_WHITE_ON_BLACK 7
#define BOX_COLOR_RED_ON_BLACK 8
#define BOX_COLOR_RED_ON_GREY 9
#define BOX_COLOR_GREEN_ON_BLACK 10
#define BOX_COLOR_BLACK_ON_BLACK 11
#define BOX_COLOR_RED_ON_GREEN 12
#define BOX_COLOR_BLACK_ON_YELLOW 13

namespace cursespp {
    class Colors {
        private:
            Colors();

        public:
            static void Init();
    };
}
