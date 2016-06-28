//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include <stdafx.h>
#include "Colors.h"

using namespace cursespp;

static int white = COLOR_WHITE;
static int blue = COLOR_BLUE;
static int red = COLOR_RED;
static int yellow = COLOR_YELLOW;
static int green = COLOR_GREEN;
static int black = COLOR_BLACK;
static int background = -1;

#define COLOR_CUSTOM_WHITE 16
#define COLOR_CUSTOM_BLUE 17
#define COLOR_CUSTOM_RED 18
#define COLOR_CUSTOM_YELLOW 19
#define COLOR_CUSTOM_GREEN 20
#define COLOR_CUSTOM_BLACK 21

Colors::Colors() {
}

void Colors::Init() {
    start_color();
    use_default_colors();

    /* the default colors are a bit harsh for my taste, so
    let's use custom colors if the terminal supports it. in
    the future we'll allow users to configure this via setting */
    if (COLORS > 8) {
        init_color(COLOR_CUSTOM_RED, 1000, 431, 392);
        red = COLOR_CUSTOM_RED;

        init_color(COLOR_CUSTOM_GREEN, 373, 980, 392);
        green = COLOR_CUSTOM_GREEN;

        init_color(COLOR_CUSTOM_YELLOW, 913, 858, 427);
        yellow = COLOR_CUSTOM_YELLOW;
    }

    init_pair(CURSESPP_WHITE_ON_BLUE, white, blue);
    init_pair(CURSESPP_RED_ON_BLUE, red, blue);
    init_pair(CURSESPP_YELLOW_ON_BLUE, yellow, blue);
    init_pair(CURSESPP_BLACK_ON_GREY, black, white);
    init_pair(CURSESPP_BLACK_ON_GREEN, black, green);
    init_pair(CURSESPP_YELLOW_ON_TRANSPARENT, yellow, background);
    init_pair(CURSESPP_WHITE_ON_TRANSPARENT, white, background);
    init_pair(CURSESPP_RED_ON_TRANSPARENT, red, background);
    init_pair(CURSESPP_RED_ON_GREY, red, white);
    init_pair(CURSESPP_GREEN_ON_TRANSPARENT, green, background);
    init_pair(CURSESPP_BLACK_ON_TRANSPARENT, black, background);
    init_pair(CURSESPP_RED_ON_GREEN, red, green);
    init_pair(CURSESPP_BLACK_ON_YELLOW, black, yellow);
}
