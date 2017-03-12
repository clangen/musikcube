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
#define THEME_COLOR_BACKGROUND 16
#define THEME_COLOR_FOREGROUND 17
#define THEME_COLOR_FOCUSED_BORDER 18
#define THEME_COLOR_TEXT_FOCUSED 19
#define THEME_COLOR_TEXT_ACTIVE 20
#define THEME_COLOR_TEXT_DISABLED 21
#define THEME_COLOR_TEXT_HIDDEN 22
#define THEME_COLOR_TEXT_WARNING 23
#define THEME_COLOR_TEXT_ERROR 24
#define THEME_COLOR_OVERLAY_BACKGROUND 25
#define THEME_COLOR_OVERLAY_FOREGROUND 26
#define THEME_COLOR_OVERLAY_BORDER 27
#define THEME_COLOR_OVERLAY_FOCUSED_BORDER 28
#define THEME_COLOR_SHORTCUTS_BACKGROUND 29
#define THEME_COLOR_SHORTCUTS_FOREGROUND 30
#define THEME_COLOR_SHORTCUTS_BACKGROUND_FOCUSED 31
#define THEME_COLOR_SHORTCUTS_FOREGROUND_FOCUSED 32
#define THEME_COLOR_BUTTON_BACKGROUND_NORMAL 33
#define THEME_COLOR_BUTTON_FOREGROUND_NORMAL 34
#define THEME_COLOR_BUTTON_BACKGROUND_ACTIVE 35
#define THEME_COLOR_BUTTON_FOREGROUND_ACTIVE 36
#define THEME_COLOR_BANNER_BACKGROUND 37
#define THEME_COLOR_BANNER_FOREGROUND 38
#define THEME_COLOR_LIST_HEADER_BACKGROUND 39
#define THEME_COLOR_LIST_HEADER_FOREGROUND 40
#define THEME_COLOR_LIST_ITEM_HIGHLIGHTED_BACKGROUND 41
#define THEME_COLOR_LIST_ITEM_HIGHLIGHTED_FOREGROUND 42
#define THEME_COLOR_LIST_ITEM_ACTIVE_BACKGROUND 43
#define THEME_COLOR_LIST_ITEM_ACTIVE_FOREGROUND 44

/* if we can't set custom colors, but we have the full 256 color
palette, use ones that most closely match our desired colors */
#define COLOR_256_GREEN 148
#define COLOR_256_RED 167
#define COLOR_256_DARK_RED 131
#define COLOR_256_YELLOW 185
#define COLOR_256_MEDIUM_GRAY 240
#define COLOR_256_ORANGE 208
#define COLOR_256_BLUE 123
#define COLOR_256_OFFWHITE 251

#define SCALE(x) ((x * 1000) / 255)

struct Theme {
    struct Color {
        enum Mode { Standard, Palette, Custom };

        Color() {
            Set(-1, -1, -1);
        }

        void Set(int colorId, int hex, int palette) {
            this->colorId = colorId;
            this->r = hex >> 16;
            this->g = (hex >> 8) & 0x0000ff;
            this->b = hex & 0x0000ff;
            this->palette = palette;
        }

        void Set(int colorId, int r, int g, int b, int palette) {
            this->colorId = colorId;
            this->r = r;
            this->g = g;
            this->b = b;
            this->palette = palette;
        }

        int Id(Mode mode, int defaultValue) {
            if (mode == Standard) {
                return defaultValue;
            }
            else if (mode == Palette) {
                return this->palette;
            }

            init_color(this->colorId, SCALE(this->r), SCALE(this->g), SCALE(this->b));
            return this->colorId;
        }

        int colorId;
        int r, g, b;
        int palette;
    };

    /* initialized with default values. the user can override them, then call Apply() */
    Theme() {
        /* main */
        background.Set(THEME_COLOR_BACKGROUND, 0, 0, 0, COLOR_BLACK);
        foreground.Set(THEME_COLOR_FOREGROUND, 230, 230, 230, COLOR_256_OFFWHITE);
        focusedBorder.Set(THEME_COLOR_FOCUSED_BORDER, 220, 82, 86, COLOR_256_RED);

        /* text */
        textFocused.Set(THEME_COLOR_TEXT_FOCUSED, 220, 82, 86, COLOR_256_RED);
        textActive.Set(THEME_COLOR_TEXT_ACTIVE, 166, 226, 46, COLOR_256_GREEN);
        textDisabled.Set(THEME_COLOR_TEXT_DISABLED, 128, 128, 128, COLOR_256_MEDIUM_GRAY);
        textHidden.Set(THEME_COLOR_TEXT_HIDDEN, 0, 0, 0, COLOR_BLACK);
        textWarning.Set(THEME_COLOR_TEXT_WARNING, 230, 220, 116, COLOR_256_YELLOW);
        textWarning.Set(THEME_COLOR_TEXT_ERROR, 220, 82, 86, COLOR_256_RED);

        /* overlay */
        overlayBackground.Set(THEME_COLOR_OVERLAY_BACKGROUND, 66, 66, 56, COLOR_256_MEDIUM_GRAY);
        overlayForeground.Set(THEME_COLOR_OVERLAY_FOREGROUND, 230, 230, 230, COLOR_256_OFFWHITE);
        overlayBorder.Set(THEME_COLOR_OVERLAY_BORDER, 102, 217, 238, COLOR_256_BLUE);
        overlayFocusedBorder.Set(THEME_COLOR_OVERLAY_FOCUSED_BORDER, 220, 82, 86, COLOR_256_RED);

        /* shortcut bar */
        shortcutsBackground.Set(THEME_COLOR_SHORTCUTS_BACKGROUND, 66, 66, 56, COLOR_256_MEDIUM_GRAY);
        shortcutsForeground.Set(THEME_COLOR_SHORTCUTS_FOREGROUND, 230, 220, 116, COLOR_256_YELLOW);
        focusedShortcutsBackground.Set(THEME_COLOR_SHORTCUTS_BACKGROUND_FOCUSED, 175, 66, 71, COLOR_256_DARK_RED);
        focusedShortcutsForeground.Set(THEME_COLOR_SHORTCUTS_FOREGROUND_FOCUSED, 230, 230, 230, COLOR_256_OFFWHITE);

        /* buttons */
        buttonBackgroundNormal.Set(THEME_COLOR_BUTTON_BACKGROUND_NORMAL, 230, 220, 116, COLOR_256_YELLOW);
        buttonForegroundNormal.Set(THEME_COLOR_BUTTON_FOREGROUND_NORMAL, 0, 0, 0, COLOR_BLACK);
        buttonBackgroundActive.Set(THEME_COLOR_BUTTON_BACKGROUND_ACTIVE, 166, 226, 46, COLOR_256_GREEN);
        buttonForegroundActive.Set(THEME_COLOR_BUTTON_FOREGROUND_ACTIVE, 0, 0, 0, COLOR_BLACK);

        /* banner */
        bannerBackground.Set(THEME_COLOR_BANNER_BACKGROUND, 255, 150, 32, COLOR_256_ORANGE);
        bannerForeground.Set(THEME_COLOR_BANNER_FOREGROUND, 0, 0, 0, COLOR_BLACK);

        /* listview */
        listHeaderBackground.Set(THEME_COLOR_LIST_HEADER_BACKGROUND, 0, 0, 0, COLOR_BLACK);
        listHeaderForeground.Set(THEME_COLOR_LIST_HEADER_FOREGROUND, 166, 226, 46, COLOR_256_GREEN);
        listHighlightedBackground.Set(THEME_COLOR_LIST_ITEM_HIGHLIGHTED_BACKGROUND, 166, 226, 46, COLOR_256_GREEN);
        listHighlightedForeground.Set(THEME_COLOR_LIST_ITEM_HIGHLIGHTED_FOREGROUND, 0, 0, 0, COLOR_BLACK);
        listActiveBackground.Set(THEME_COLOR_LIST_ITEM_ACTIVE_BACKGROUND, 66, 66, 56, COLOR_256_MEDIUM_GRAY);
        listActiveForeground.Set(THEME_COLOR_LIST_ITEM_ACTIVE_FOREGROUND, 230, 220, 116, COLOR_256_YELLOW);
    }

    /* initializes all of the color pairs from the specified colors, then applies them
    to the current session! */
    void Apply(Color::Mode mode) {
        int backgroundId = background.Id(mode, -1);
        int foregroundId = foreground.Id(mode, -1);

        /* main */
        init_pair(CURSESPP_DEFAULT_CONTENT_COLOR, foregroundId, backgroundId);
        init_pair(CURSESPP_DEFAULT_FRAME_COLOR, foregroundId, backgroundId);
        init_pair(CURSESPP_FOCUSED_FRAME_COLOR, focusedBorder.Id(mode, COLOR_RED),backgroundId);

        /* text */
        init_pair(CURSESPP_TEXT_DEFAULT, foregroundId, backgroundId);
        init_pair(CURSESPP_TEXT_DISABLED, textDisabled.Id(mode, -1), backgroundId);
        init_pair(CURSESPP_TEXT_FOCUSED, textFocused.Id(mode, COLOR_RED), backgroundId);
        init_pair(CURSESPP_TEXT_ACTIVE, textActive.Id(mode, COLOR_GREEN), backgroundId);
        init_pair(CURSESPP_TEXT_WARNING, textWarning.Id(mode, COLOR_YELLOW), backgroundId);
        init_pair(CURSESPP_TEXT_ERROR, textError.Id(mode, COLOR_RED), backgroundId);
        init_pair(CURSESPP_TEXT_HIDDEN, textHidden.Id(mode, COLOR_BLACK), backgroundId);

        /* overlay */
        int overlayBgId = overlayBackground.Id(mode, -1);
        init_pair(CURSESPP_OVERLAY_FRAME, overlayBorder.Id(mode, COLOR_BLUE), overlayBgId);
        init_pair(CURSESPP_OVERLAY_CONTENT, overlayForeground.Id(mode, -1), overlayBgId);
        init_pair(CURSESPP_OVERLAY_INPUT_FRAME, overlayFocusedBorder.Id(mode, COLOR_RED), overlayBgId);

        /* shortcuts */
        init_pair(
            CURSESPP_SHORTCUT_ROW_NORMAL,
            shortcutsForeground.Id(mode, COLOR_YELLOW),
            shortcutsBackground.Id(mode, -1));

        init_pair(
            CURSESPP_SHORTCUT_ROW_FOCUSED,
            focusedShortcutsForeground.Id(mode, COLOR_WHITE),
            focusedShortcutsBackground.Id(mode, COLOR_RED));

        /* buttons */
        init_pair(
            CURSESPP_BUTTON_NORMAL,
            buttonForegroundNormal.Id(mode, COLOR_BLACK),
            buttonBackgroundNormal.Id(mode, COLOR_YELLOW));

        init_pair(
            CURSESPP_BUTTON_HIGHLIGHTED,
            buttonForegroundActive.Id(mode, COLOR_BLACK),
            buttonBackgroundActive.Id(mode, COLOR_GREEN));

        /* banner */
        init_pair(
            CURSESPP_BANNER,
            bannerForeground.Id(mode, COLOR_BLACK),
            bannerBackground.Id(mode, COLOR_YELLOW));

        /* list items */
        init_pair(
            CURSESPP_LIST_ITEM_HEADER,
            listHeaderForeground.Id(mode, COLOR_GREEN),
            listHeaderBackground.Id(mode, -1));

        init_pair(
            CURSESPP_SELECTED_LIST_ITEM,
            listActiveForeground.Id(mode, COLOR_YELLOW),
            listActiveBackground.Id(mode, COLOR_BLACK));

        init_pair(
            CURSESPP_HIGHLIGHTED_LIST_ITEM,
            listHighlightedForeground.Id(mode, COLOR_BLACK),
            listHighlightedBackground.Id(mode, COLOR_GREEN));

        init_pair( /* note: swap active fg/bg */
            CURSESPP_HIGHLIGHTED_SELECTED_LIST_ITEM,
            listActiveBackground.Id(mode, COLOR_BLACK),
            listActiveForeground.Id(mode, COLOR_YELLOW));
    }

    /* main */
    Color background;
    Color foreground;
    Color focusedBorder;

    /* text */
    Color textFocused;
    Color textActive;
    Color textDisabled;
    Color textHidden;
    Color textWarning;
    Color textError;

    /* overlay */
    Color overlayBackground;
    Color overlayForeground;
    Color overlayBorder;
    Color overlayFocusedBorder;

    /* shortcut bar */
    Color shortcutsBackground;
    Color shortcutsForeground;
    Color focusedShortcutsBackground;
    Color focusedShortcutsForeground;

    /* buttons */
    Color buttonBackgroundNormal;
    Color buttonForegroundNormal;
    Color buttonBackgroundActive;
    Color buttonForegroundActive;

    /* banner */
    Color bannerBackground;
    Color bannerForeground;

    /* listview */
    Color listHeaderBackground;
    Color listHeaderForeground;
    Color listHighlightedBackground;
    Color listHighlightedForeground;
    Color listActiveForeground;
    Color listActiveBackground;
};

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
    return !!can_change_color();
#endif
}

Colors::Colors() {
}

static Theme defaultTheme;

void Colors::Init(bool disableCustomColors) {
    start_color();
    use_default_colors();

    using Color = Theme::Color;
    Color::Mode mode = Color::Standard;
    if (!disableCustomColors && COLORS > 8) {
        mode = canChangeColors() ? Color::Custom : Color::Palette;
    }

    defaultTheme.Apply(mode);
}
