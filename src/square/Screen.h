#pragma once

#include "curses_config.h"

class Screen {
    private:
        Screen();

    public:
        static int GetWidth();
        static int GetHeight();
};