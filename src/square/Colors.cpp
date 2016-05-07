#pragma once

#include "stdafx.h"
#include "Colors.h"
#include <curses.h>

Colors::Colors() {
}

void Colors::Init() {
    init_pair(BOX_COLOR_WHITE_ON_BLUE, COLOR_WHITE, COLOR_BLUE);
    init_pair(BOX_COLOR_RED_ON_BLUE, COLOR_RED, COLOR_BLUE);
    init_pair(BOX_COLOR_YELLOW_ON_BLUE, COLOR_YELLOW, COLOR_BLUE);
    init_pair(BOX_COLOR_BLACK_ON_GREY, COLOR_BLACK, COLOR_WHITE);
}
