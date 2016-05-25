#pragma once

#include <stdafx.h>
#include "Screen.h"

using namespace cursespp;

Screen::Screen() {
}

int Screen::GetWidth() {
    return getmaxx(stdscr);
}

int Screen::GetHeight() {
    return getmaxy(stdscr);
}
