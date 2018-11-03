//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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
#include <json.hpp>
#include "Colors.h"

using namespace cursespp;
using namespace nlohmann;

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
#define THEME_COLOR_OVERLAY_FOCUSED_TEXT 29
#define THEME_COLOR_SHORTCUTS_BACKGROUND 30
#define THEME_COLOR_SHORTCUTS_FOREGROUND 31
#define THEME_COLOR_SHORTCUTS_BACKGROUND_FOCUSED 32
#define THEME_COLOR_SHORTCUTS_FOREGROUND_FOCUSED 33
#define THEME_COLOR_BUTTON_BACKGROUND_NORMAL 34
#define THEME_COLOR_BUTTON_FOREGROUND_NORMAL 35
#define THEME_COLOR_BUTTON_BACKGROUND_ACTIVE 36
#define THEME_COLOR_BUTTON_FOREGROUND_ACTIVE 37
#define THEME_COLOR_BANNER_BACKGROUND 38
#define THEME_COLOR_BANNER_FOREGROUND 39
#define THEME_COLOR_LIST_HEADER_BACKGROUND 40
#define THEME_COLOR_LIST_HEADER_FOREGROUND 41
#define THEME_COLOR_LIST_HEADER_HIGHLIGHTED_BACKGROUND 42
#define THEME_COLOR_LIST_HEADER_HIGHLIGHTED_FOREGROUND 43
#define THEME_COLOR_LIST_ITEM_HIGHLIGHTED_BACKGROUND 44
#define THEME_COLOR_LIST_ITEM_HIGHLIGHTED_FOREGROUND 45
#define THEME_COLOR_LIST_ITEM_ACTIVE_BACKGROUND 46
#define THEME_COLOR_LIST_ITEM_ACTIVE_FOREGROUND 47
#define THEME_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_BACKGROUND 48
#define THEME_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_FOREGROUND 49
#define THEME_COLOR_FOOTER_BACKGROUND 50
#define THEME_COLOR_FOOTER_FOREGROUND 51

/* user-readable names for the color identifiers above. these are
used as key names in the config files */
#define JSON_KEY_COLOR_BACKGROUND "background"
#define JSON_KEY_COLOR_FOREGROUND "foreground"
#define JSON_KEY_COLOR_FOCUSED_BORDER "focused_border"
#define JSON_KEY_COLOR_TEXT_FOCUSED "text_focused"
#define JSON_KEY_COLOR_TEXT_ACTIVE "text_active"
#define JSON_KEY_COLOR_TEXT_DISABLED "text_disabled"
#define JSON_KEY_COLOR_TEXT_HIDDEN "text_hidden"
#define JSON_KEY_COLOR_TEXT_WARNING "text_warning"
#define JSON_KEY_COLOR_TEXT_ERROR "text_error"
#define JSON_KEY_COLOR_OVERLAY_BACKGROUND "overlay_background"
#define JSON_KEY_COLOR_OVERLAY_FOREGROUND "overlay_foreground"
#define JSON_KEY_COLOR_OVERLAY_BORDER "overlay_border"
#define JSON_KEY_COLOR_OVERLAY_FOCUSED_BORDER "overlay_focused_border"
#define JSON_KEY_COLOR_OVERLAY_FOCUSED_TEXT "overlay_focused_text"
#define JSON_KEY_COLOR_SHORTCUTS_BACKGROUND "shortcuts_background"
#define JSON_KEY_COLOR_SHORTCUTS_FOREGROUND "shortcuts_foreground"
#define JSON_KEY_COLOR_SHORTCUTS_BACKGROUND_FOCUSED "shortcuts_background_focused"
#define JSON_KEY_COLOR_SHORTCUTS_FOREGROUND_FOCUSED "shortcuts_foreground_focused"
#define JSON_KEY_COLOR_BUTTON_BACKGROUND_NORMAL "button_background_normal"
#define JSON_KEY_COLOR_BUTTON_FOREGROUND_NORMAL "button_foreground_normal"
#define JSON_KEY_COLOR_BUTTON_BACKGROUND_ACTIVE "button_background_active"
#define JSON_KEY_COLOR_BUTTON_FOREGROUND_ACTIVE "button_foreground_active"
#define JSON_KEY_COLOR_BANNER_BACKGROUND "banner_background"
#define JSON_KEY_COLOR_BANNER_FOREGROUND "banner_foreground"
#define JSON_KEY_COLOR_FOOTER_BACKGROUND "footer_background"
#define JSON_KEY_COLOR_FOOTER_FOREGROUND "footer_foreground"
#define JSON_KEY_COLOR_LIST_HEADER_BACKGROUND "list_header_background"
#define JSON_KEY_COLOR_LIST_HEADER_FOREGROUND "list_header_foreground"
#define JSON_KEY_COLOR_LIST_HEADER_HIGHLIGHTED_BACKGROUND "list_header_highlighted_background"
#define JSON_KEY_COLOR_LIST_HEADER_HIGHLIGHTED_FOREGROUND "list_header_highlighted_foreground"
#define JSON_KEY_COLOR_LIST_ITEM_HIGHLIGHTED_BACKGROUND "list_item_highlighted_background"
#define JSON_KEY_COLOR_LIST_ITEM_HIGHLIGHTED_FOREGROUND "list_item_highlighted_foreground"
#define JSON_KEY_COLOR_LIST_ITEM_ACTIVE_BACKGROUND "list_item_active_background"
#define JSON_KEY_COLOR_LIST_ITEM_ACTIVE_FOREGROUND "list_item_active_foreground"
#define JSON_KEY_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_BACKGROUND "list_item_active_highlighted_background"
#define JSON_KEY_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_FOREGROUND "list_item_active_highlighted_foreground"

#define JSON_KEY_NAME "name"
#define JSON_KEY_SCHEMA_VERSION "schemaVersion"
#define JSON_KEY_COLORS "colors"
#define JSON_KEY_HEX "hex"
#define JSON_KEY_PALETTE "palette"

#define JSON_CURRENT_SCHEMA_VERSION 1

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

/* as much as I hate to do this, it was copied directly from
musik::core::FileToByteArray to keep things properly encapsulated. */
static bool fileToByteArray(const std::string& path, char** target, int& size) {
#ifdef WIN32
    std::wstring u16fn = u8to16(path);
    FILE* file = _wfopen(u16fn.c_str(), L"rb");
#else
    FILE* file = fopen(path.c_str(), "rb");
#endif

    *target = nullptr;
    size = 0;

    if (!file) {
        return false;
    }

    bool success = false;

    if (fseek(file, 0L, SEEK_END) == 0) {
        long fileSize = ftell(file);
        if (fileSize == -1) {
            goto close_and_return;
        }

        if (fseek(file, 0L, SEEK_SET) != 0) {
            goto close_and_return;
        }

        *target = (char*)malloc(sizeof(char) * (fileSize + 1));
        size = fread(*target, sizeof(char), fileSize, file);

        if (size == fileSize) {
            (*target)[size] = 0;
            success = true;
        }
    }

close_and_return:
    fclose(file);

    if (!success) {
        free(*target);
    }

    return success;
}

struct Theme {
    struct Color {
        Color() {
            Set(0, 0, 0, 0, -1);
        }

        void Set(int colorId, int r, int g, int b, int palette) {
            this->colorId = colorId;
            this->r = r;
            this->g = g;
            this->b = b;
            this->palette = palette;
        }

        void Set(const json& obj) {
            if (!obj.is_null()) {
                std::string hex = obj.value(JSON_KEY_HEX, "");
                long palette = obj.value(JSON_KEY_PALETTE, -1);

                if (hex.length() == 7 && hex[0] == '#') {
                    int rgb = strtol(hex.substr(1).c_str(), 0, 16);
                    this->b = rgb & 0x0000ff;
                    this->r = rgb >> 16;
                    this->g = (rgb >> 8) & 0x0000ff;
                }
                else if (palette > 15 && palette < 256) {
                    /* we may have already been initialized with a default
                    color. but if we have a palette color, clear the RGB
                    values and prefer the value from the theme. */
                    this->r = this->b = this->g = -1;
                }

                if (palette != -1) {
                    this->palette = palette;
                }
            }
        }

        int Id(Colors::Mode mode, int defaultValue) {
            if (mode == Colors::Basic) {
                return defaultValue;
            }
            else if (mode == Colors::Palette) {
                return this->palette;
            }
            else {
                if (this->colorId > 15 && this->r >= 0 && this->g >= 0 && this->b >= 0) {
                    init_color(this->colorId, SCALE(this->r), SCALE(this->g), SCALE(this->b));
                    return this->colorId;
                }
                else {
                    /* if RGB mode was requested but no RGB value exists, fall back to the
                    palette value. */
                    return this->palette;
                }
            }
            return -1;
        }

        int colorId;
        int r, g, b;
        int palette;
    };

    Theme() {
    }

    void Reset() {
        /* main */
        background.Set(THEME_COLOR_BACKGROUND, 24, 24, 20, -1);
        foreground.Set(THEME_COLOR_FOREGROUND, 220, 220, 220, COLOR_256_OFFWHITE);
        focusedBorder.Set(THEME_COLOR_FOCUSED_BORDER, 220, 82, 86, COLOR_256_RED);

        /* text */
        textFocused.Set(THEME_COLOR_TEXT_FOCUSED, 220, 82, 86, COLOR_256_RED);
        textActive.Set(THEME_COLOR_TEXT_ACTIVE, 166, 226, 46, COLOR_256_GREEN);
        textDisabled.Set(THEME_COLOR_TEXT_DISABLED, 128, 128, 128, COLOR_256_MEDIUM_GRAY);
        textHidden.Set(THEME_COLOR_TEXT_HIDDEN, 66, 66, 56, COLOR_BLACK);
        textWarning.Set(THEME_COLOR_TEXT_WARNING, 230, 220, 116, COLOR_256_YELLOW);
        textError.Set(THEME_COLOR_TEXT_ERROR, 220, 82, 86, COLOR_256_RED);

        /* overlay */
        overlayBackground.Set(THEME_COLOR_OVERLAY_BACKGROUND, 66, 66, 56, COLOR_256_MEDIUM_GRAY);
        overlayForeground.Set(THEME_COLOR_OVERLAY_FOREGROUND, 220, 220, 220, COLOR_256_OFFWHITE);
        overlayBorder.Set(THEME_COLOR_OVERLAY_BORDER, 102, 217, 238, COLOR_256_BLUE);
        overlayFocusedBorder.Set(THEME_COLOR_OVERLAY_FOCUSED_BORDER, 220, 82, 86, COLOR_256_RED);
        overlayFocusedText.Set(THEME_COLOR_OVERLAY_FOCUSED_TEXT, 220, 82, 86, COLOR_256_RED);

        /* shortcut bar */
        shortcutsBackground.Set(THEME_COLOR_SHORTCUTS_BACKGROUND, 66, 66, 56, COLOR_256_MEDIUM_GRAY);
        shortcutsForeground.Set(THEME_COLOR_SHORTCUTS_FOREGROUND, 230, 220, 116, COLOR_256_YELLOW);
        focusedShortcutsBackground.Set(THEME_COLOR_SHORTCUTS_BACKGROUND_FOCUSED, 175, 66, 71, COLOR_256_DARK_RED);
        focusedShortcutsForeground.Set(THEME_COLOR_SHORTCUTS_FOREGROUND_FOCUSED, 220, 220, 220, COLOR_256_OFFWHITE);

        /* buttons */
        buttonBackgroundNormal.Set(THEME_COLOR_BUTTON_BACKGROUND_NORMAL, 230, 220, 116, COLOR_256_YELLOW);
        buttonForegroundNormal.Set(THEME_COLOR_BUTTON_FOREGROUND_NORMAL, 24, 24, 20, COLOR_BLACK);
        buttonBackgroundActive.Set(THEME_COLOR_BUTTON_BACKGROUND_ACTIVE, 166, 226, 46, COLOR_256_GREEN);
        buttonForegroundActive.Set(THEME_COLOR_BUTTON_FOREGROUND_ACTIVE, 24, 24, 20, COLOR_BLACK);

        /* banner */
        bannerBackground.Set(THEME_COLOR_BANNER_BACKGROUND, 255, 150, 32, COLOR_256_ORANGE);
        bannerForeground.Set(THEME_COLOR_BANNER_FOREGROUND, 24, 24, 20, COLOR_BLACK);

        /* footer */
        footerBackground.Set(THEME_COLOR_FOOTER_BACKGROUND, 102, 217, 238, COLOR_256_BLUE);
        footerForeground.Set(THEME_COLOR_FOOTER_FOREGROUND, 24, 24, 20, COLOR_BLACK);

        /* listview */
        listHeaderBackground.Set(THEME_COLOR_LIST_HEADER_BACKGROUND, 36, 36, 31, -1);
        listHeaderForeground.Set(THEME_COLOR_LIST_HEADER_FOREGROUND, 166, 226, 46, COLOR_256_GREEN);
        listHeaderHighlightedBackground.Set(THEME_COLOR_LIST_HEADER_HIGHLIGHTED_BACKGROUND, 166, 226, 46, COLOR_256_GREEN);
        listHeaderHighlightedForeground.Set(THEME_COLOR_LIST_HEADER_HIGHLIGHTED_FOREGROUND, 36, 36, 31, COLOR_BLACK);
        listHighlightedBackground.Set(THEME_COLOR_LIST_ITEM_HIGHLIGHTED_BACKGROUND, 166, 226, 46, COLOR_256_GREEN);
        listHighlightedForeground.Set(THEME_COLOR_LIST_ITEM_HIGHLIGHTED_FOREGROUND, 24, 24, 20, COLOR_BLACK);
        listActiveBackground.Set(THEME_COLOR_LIST_ITEM_ACTIVE_BACKGROUND, 66, 66, 56, COLOR_256_MEDIUM_GRAY);
        listActiveForeground.Set(THEME_COLOR_LIST_ITEM_ACTIVE_FOREGROUND, 230, 220, 116, COLOR_256_YELLOW);
        listActiveHighlightedBackground.Set(THEME_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_BACKGROUND, 230, 220, 116, COLOR_256_YELLOW);
        listActiveHighlightedForeground.Set(THEME_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_FOREGROUND, 24, 24, 20, COLOR_BLACK);
    }

    bool LoadFromFile(const std::string& fn) {
        if (!fn.size()) {
            return false;
        }

        bool success = false;
        char* buffer = nullptr;
        int size = 0;

        if (fileToByteArray(fn, &buffer, size)) {
            try {
                json data = json::parse(buffer);

                /* validate schema version */
                int schemaVersion = data[JSON_KEY_SCHEMA_VERSION];
                if (schemaVersion == JSON_CURRENT_SCHEMA_VERSION) {
                    std::string name = data[JSON_KEY_NAME];
                    json colors = data[JSON_KEY_COLORS];
                    static json unset;

                    /* actually read the theme values! */
                    this->background.Set(colors.value(JSON_KEY_COLOR_BACKGROUND, unset));
                    this->foreground.Set(colors.value(JSON_KEY_COLOR_FOREGROUND, unset));
                    this->focusedBorder.Set(colors.value(JSON_KEY_COLOR_FOCUSED_BORDER, unset));
                    this->textFocused.Set(colors.value(JSON_KEY_COLOR_TEXT_FOCUSED, unset));
                    this->textActive.Set(colors.value(JSON_KEY_COLOR_TEXT_ACTIVE, unset));
                    this->textDisabled.Set(colors.value(JSON_KEY_COLOR_TEXT_DISABLED, unset));
                    this->textHidden.Set(colors.value(JSON_KEY_COLOR_TEXT_HIDDEN, unset));
                    this->textWarning.Set(colors.value(JSON_KEY_COLOR_TEXT_WARNING, unset));
                    this->textError.Set(colors.value(JSON_KEY_COLOR_TEXT_ERROR, unset));
                    this->overlayBackground.Set(colors.value(JSON_KEY_COLOR_OVERLAY_BACKGROUND, unset));
                    this->overlayForeground.Set(colors.value(JSON_KEY_COLOR_OVERLAY_FOREGROUND, unset));
                    this->overlayBorder.Set(colors.value(JSON_KEY_COLOR_OVERLAY_BORDER, unset));
                    this->overlayFocusedBorder.Set(colors.value(JSON_KEY_COLOR_OVERLAY_FOCUSED_BORDER, unset));
                    this->overlayFocusedText.Set(colors.value(JSON_KEY_COLOR_OVERLAY_FOCUSED_TEXT, unset));
                    this->shortcutsBackground.Set(colors.value(JSON_KEY_COLOR_SHORTCUTS_BACKGROUND, unset));
                    this->shortcutsForeground.Set(colors.value(JSON_KEY_COLOR_SHORTCUTS_FOREGROUND, unset));
                    this->focusedShortcutsBackground.Set(colors.value(JSON_KEY_COLOR_SHORTCUTS_BACKGROUND_FOCUSED, unset));
                    this->focusedShortcutsForeground.Set(colors.value(JSON_KEY_COLOR_SHORTCUTS_FOREGROUND_FOCUSED, unset));
                    this->buttonBackgroundNormal.Set(colors.value(JSON_KEY_COLOR_BUTTON_BACKGROUND_NORMAL, unset));
                    this->buttonForegroundNormal.Set(colors.value(JSON_KEY_COLOR_BUTTON_FOREGROUND_NORMAL, unset));
                    this->buttonBackgroundActive.Set(colors.value(JSON_KEY_COLOR_BUTTON_BACKGROUND_ACTIVE, unset));
                    this->buttonForegroundActive.Set(colors.value(JSON_KEY_COLOR_BUTTON_FOREGROUND_ACTIVE, unset));
                    this->bannerBackground.Set(colors.value(JSON_KEY_COLOR_BANNER_BACKGROUND, unset));
                    this->bannerForeground.Set(colors.value(JSON_KEY_COLOR_BANNER_FOREGROUND, unset));
                    this->footerBackground.Set(colors.value(JSON_KEY_COLOR_FOOTER_BACKGROUND, unset));
                    this->footerForeground.Set(colors.value(JSON_KEY_COLOR_FOOTER_FOREGROUND, unset));
                    this->listHeaderBackground.Set(colors.value(JSON_KEY_COLOR_LIST_HEADER_BACKGROUND, unset));
                    this->listHeaderForeground.Set(colors.value(JSON_KEY_COLOR_LIST_HEADER_FOREGROUND, unset));
                    this->listHeaderHighlightedBackground.Set(colors.value(JSON_KEY_COLOR_LIST_HEADER_HIGHLIGHTED_BACKGROUND, unset));
                    this->listHeaderHighlightedForeground.Set(colors.value(JSON_KEY_COLOR_LIST_HEADER_HIGHLIGHTED_FOREGROUND, unset));
                    this->listHighlightedBackground.Set(colors.value(JSON_KEY_COLOR_LIST_ITEM_HIGHLIGHTED_BACKGROUND, unset));
                    this->listHighlightedForeground.Set(colors.value(JSON_KEY_COLOR_LIST_ITEM_HIGHLIGHTED_FOREGROUND, unset));
                    this->listActiveBackground.Set(colors.value(JSON_KEY_COLOR_LIST_ITEM_ACTIVE_BACKGROUND, unset));
                    this->listActiveForeground.Set(colors.value(JSON_KEY_COLOR_LIST_ITEM_ACTIVE_FOREGROUND, unset));
                    this->listActiveHighlightedBackground.Set(colors.value(JSON_KEY_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_BACKGROUND, unset));
                    this->listActiveHighlightedForeground.Set(colors.value(JSON_KEY_COLOR_LIST_ITEM_ACTIVE_HIGHLIGHTED_FOREGROUND, unset));

                    success = true;
                }
            }
            catch (...) {
            }
        }

        free(buffer);
        return success;
    }

    /* initializes all of the color pairs from the specified colors, then applies them
    to the current session! */
    void Apply(Colors::Mode mode, Colors::BgType bgType) {
        int backgroundId = (bgType == Colors::BgType::Theme) ? background.Id(mode, -1) : -1;
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
        init_pair(CURSESPP_OVERLAY_TEXT_FOCUSED, overlayFocusedText.Id(mode, COLOR_RED), overlayBgId);

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

        /* footer */
        init_pair(
            CURSESPP_FOOTER,
            footerForeground.Id(mode, COLOR_BLACK),
            footerBackground.Id(mode, COLOR_BLUE));

        /* list items */
        init_pair(
            CURSESPP_LIST_ITEM_HEADER,
            listHeaderForeground.Id(mode, COLOR_GREEN),
            listHeaderBackground.Id(mode, -1));

        init_pair(
            CURSESPP_LIST_ITEM_HIGHLIGHTED_HEADER,
            listHeaderHighlightedForeground.Id(mode, -1),
            listHeaderHighlightedBackground.Id(mode, COLOR_GREEN));

        init_pair(
            CURSESPP_SELECTED_LIST_ITEM,
            listActiveForeground.Id(mode, COLOR_YELLOW),
            listActiveBackground.Id(mode, COLOR_BLACK));

        init_pair(
            CURSESPP_HIGHLIGHTED_LIST_ITEM,
            listHighlightedForeground.Id(mode, COLOR_BLACK),
            listHighlightedBackground.Id(mode, COLOR_GREEN));

        init_pair(
            CURSESPP_HIGHLIGHTED_SELECTED_LIST_ITEM,
            listActiveHighlightedForeground.Id(mode, COLOR_BLACK),
            listActiveHighlightedBackground.Id(mode, COLOR_YELLOW));

        init_pair(
            CURSESPP_HIGHLIGHTED_ERROR_LIST_ITEM,
            textError.Id(mode, COLOR_RED),
            listHighlightedBackground.Id(mode, COLOR_GREEN));
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
    Color overlayFocusedText;

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

    /* footer */
    Color footerBackground;
    Color footerForeground;

    /* listview */
    Color listHeaderBackground;
    Color listHeaderForeground;
    Color listHeaderHighlightedBackground;
    Color listHeaderHighlightedForeground;
    Color listHighlightedBackground;
    Color listHighlightedForeground;
    Color listActiveForeground;
    Color listActiveBackground;
    Color listActiveHighlightedBackground;
    Color listActiveHighlightedForeground;
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

static Theme theme;
static Colors::Mode colorMode = Colors::Basic;
static Colors::BgType bgType = Colors::Theme;

void Colors::Init(Colors::Mode mode, Colors::BgType bgType) {
    start_color();
    use_default_colors();

    ::colorMode = Colors::Basic;
    ::bgType = bgType;

    if (mode != Colors::Basic && COLORS > 8) {
        if (mode == Colors::RGB && canChangeColors()) {
            ::colorMode = Colors::RGB;
        }
        else {
            ::colorMode = Colors::Palette;
        }
    }

    theme.Reset();
    theme.Apply(::colorMode, ::bgType);
}

void Colors::SetTheme(const std::string& fn) {
    theme.Reset();
    theme.LoadFromFile(fn);
    theme.Apply(::colorMode, ::bgType);
}