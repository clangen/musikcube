#include <stdafx.h>
#include "Colors.h"

using namespace cursespp;

Colors::Colors() {
}

void Colors::Init() {
    init_pair(BOX_COLOR_WHITE_ON_BLUE, COLOR_WHITE, COLOR_BLUE);
    init_pair(BOX_COLOR_RED_ON_BLUE, COLOR_RED, COLOR_BLUE);
    init_pair(BOX_COLOR_YELLOW_ON_BLUE, COLOR_YELLOW, COLOR_BLUE);
    init_pair(BOX_COLOR_BLACK_ON_GREY, COLOR_BLACK, COLOR_WHITE);
    init_pair(BOX_COLOR_BLACK_ON_GREEN, COLOR_BLACK, COLOR_GREEN);
    init_pair(BOX_COLOR_YELLOW_ON_BLACK, COLOR_YELLOW, -1);
    init_pair(BOX_COLOR_WHITE_ON_BLACK, COLOR_WHITE, -1);
    init_pair(BOX_COLOR_RED_ON_BLACK, COLOR_RED, -1);
    init_pair(BOX_COLOR_RED_ON_GREY, COLOR_RED, COLOR_WHITE);
    init_pair(BOX_COLOR_GREEN_ON_BLACK, COLOR_GREEN, -1);
    init_pair(BOX_COLOR_BLACK_ON_BLACK, COLOR_BLACK, -1);
    init_pair(BOX_COLOR_RED_ON_GREEN, COLOR_RED, COLOR_GREEN);
    init_pair(BOX_COLOR_BLACK_ON_YELLOW, COLOR_BLACK, COLOR_YELLOW);
}
