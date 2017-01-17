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

/* if the terminal supports custom colors, these are the palette
indicies we'll use to store them */
#define COLOR_CUSTOM_WHITE 16
#define COLOR_CUSTOM_BLUE 17
#define COLOR_CUSTOM_RED 18
#define COLOR_CUSTOM_YELLOW 19
#define COLOR_CUSTOM_GREEN 20
#define COLOR_CUSTOM_BLACK 21
#define COLOR_CUSTOM_GREY 22
#define COLOR_CUSTOM_SELECTED_LIST_ITEM_BG 23
#define COLOR_CUSTOM_ORANGE 24
#define COLOR_CUSTOM_DARK_RED 25

/* if we can't set custom colors, but we have the full 256 color
palette, use ones that most closely match our desired colors */
#define COLOR_256_GREEN 148
#define COLOR_256_RED 167
#define COLOR_256_DARK_RED 160
#define COLOR_256_YELLOW 185
#define COLOR_256_MEDIUM_GRAY 240
#define COLOR_256_ORANGE 208
#define COLOR_256_BLUE 123
#define COLOR_256_OFFWHITE 251

/* vars that hold our actual color definitions. these may change
later during init if the terminal supports > 8 colors */
static int white = COLOR_WHITE;
static int blue = COLOR_BLUE;
static int red = COLOR_RED;
static int darkRed = COLOR_RED;
static int yellow = COLOR_YELLOW;
static int green = COLOR_GREEN;
static int black = COLOR_BLACK;
static int orange = COLOR_YELLOW;
static int foreground = COLOR_WHITE;
static int background = -1;
static int selected = -1;
static int grey = COLOR_WHITE;

#define SCALE(x) ((x * 1000) / 255)

static int initColor(int id, int r, int g, int b) {
    init_color(id, SCALE(r), SCALE(g), SCALE(b));
    return id;
}

/* some terminals report custom colors are supported, and also
don't error when calling init_color(), but don't actually work. */
static bool canChangeColors() {
#ifdef __APPLE__
    const char* termEnv = std::getenv("TERM_PROGRAM");
    std::string term;
    if (termEnv && strlen(termEnv)) {
        term = std::string(termEnv);
        if (term == "Apple_Terminal") {
            return false;
        }
    }
    return can_change_color();
#else
    return can_change_color();
#endif
}

Colors::Colors() {
}

void Colors::Init(bool disableCustomColors) {
    start_color();
    use_default_colors();

    /* the default colors are a bit harsh for my taste, so
    let's use custom colors if the terminal supports it. in
    the future we'll allow users to configure this via setting */
    if (!disableCustomColors && COLORS > 8) {
        if (canChangeColors()) {
            red = initColor(COLOR_CUSTOM_RED, 220, 82, 86);
            darkRed = initColor(COLOR_CUSTOM_DARK_RED, 175, 66, 71);
            green = initColor(COLOR_CUSTOM_GREEN, 166, 226, 46);
            yellow = initColor(COLOR_CUSTOM_YELLOW, 230, 220, 116);
            selected = initColor(COLOR_CUSTOM_SELECTED_LIST_ITEM_BG, 66, 66, 56);
            grey = initColor(COLOR_CUSTOM_GREY, 128, 128, 128);
            orange = initColor(COLOR_CUSTOM_ORANGE, 255, 150, 32);
            blue = initColor(COLOR_CUSTOM_BLUE, 102, 217, 238);
            white = initColor(COLOR_CUSTOM_WHITE, 230, 230, 230);
            foreground = COLOR_CUSTOM_WHITE;
        }
        else if (COLORS >= 256) {
            green = COLOR_256_GREEN;
            red = COLOR_256_RED;
            darkRed = COLOR_256_DARK_RED;
            yellow = COLOR_256_YELLOW;
            selected = COLOR_256_MEDIUM_GRAY;
            grey = COLOR_256_MEDIUM_GRAY;
            orange = COLOR_256_ORANGE;
            blue = COLOR_256_BLUE;
            white = COLOR_256_OFFWHITE;
            foreground = COLOR_256_OFFWHITE;
        }
    }

    init_pair(CURSESPP_SELECTED_LIST_ITEM, yellow, selected);
    init_pair(CURSESPP_HIGHLIGHTED_LIST_ITEM, black, green);
    init_pair(CURSESPP_HIGHLIGHTED_SELECTED_LIST_ITEM, black, yellow);
    init_pair(CURSESPP_LIST_ITEM_HEADER, green, background);

    init_pair(CURSESPP_DEFAULT_CONTENT_COLOR, foreground, background);
    init_pair(CURSESPP_DEFAULT_FRAME_COLOR, foreground, background);
    init_pair(CURSESPP_FOCUSED_FRAME_COLOR, red, background);

    init_pair(CURSESPP_TEXT_DEFAULT, white, background);
    init_pair(CURSESPP_TEXT_DISABLED, grey, background);
    init_pair(CURSESPP_TEXT_FOCUSED, red, background);
    init_pair(CURSESPP_TEXT_ACTIVE, green, background);
    init_pair(CURSESPP_TEXT_WARNING, yellow, background);
    init_pair(CURSESPP_TEXT_ERROR, red, background);
    init_pair(CURSESPP_TEXT_HIDDEN, black, background);
    init_pair(CURSESPP_TEXT_SEPARATOR, orange, background);

    init_pair(CURSESPP_BUTTON_NORMAL, black, yellow);
    init_pair(CURSESPP_BUTTON_NEGATIVE, white, red);
    init_pair(CURSESPP_BUTTON_HIGHLIGHTED, black, green);

    init_pair(CURSESPP_SHORTCUT_ROW_NORMAL, yellow, selected);
    init_pair(CURSESPP_SHORTCUT_ROW_FOCUSED, white, darkRed);

    init_pair(CURSESPP_OVERLAY_FRAME, blue, selected);
    init_pair(CURSESPP_OVERLAY_BACKGROUND, white, selected);
}
