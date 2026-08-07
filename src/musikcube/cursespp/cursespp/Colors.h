//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/** @file Colors.h @brief Abstract color pairs and the theme/color-mode manager. */
#pragma once

#include <cursespp/curses_config.h>
#include <string>
#include <vector>

namespace cursespp {
    /** @brief An abstract color reference mapped to a curses color pair.
     *
     *  @details Color wraps a symbolic color identifier (a member of
     *  Color::Type) and resolves it to an ncurses/PDCurses color pair via
     *  COLOR_PAIR(). It converts implicitly to int64_t so it can be passed
     *  directly to curses attribute- and color-using functions. The Default
     *  value maps to -1, which lets widgets opt out of explicit coloring.
     */
    class Color {
        public:
            /** @brief The symbolic colors used by the toolkit. */
            enum Type {
                Default = -1,

                ListItemSelected = 1,
                ListItemHighlighted = 2,
                ListItemError = 3,
                ListItemHighlightedSelected = 4,
                ListItemHeader = 5,
                ListItemHeaderHighlighted = 6,

                ContentColorDefault = 7,
                FrameDefault = 8,
                FrameFocused = 9,
                FrameImportant = 10,

                TextDefault = 11,
                TextDisabled = 12,
                TextFocused = 13,
                TextActive = 14,
                TextWarning = 15,
                TextError = 16,
                TextHidden = 17,

                ButtonDefault = 18,
                ButtonHighlighted = 19,

                ShortcutRowDefault = 20,
                ShortcutRowFocused = 21,

                OverlayFrame = 22,
                OverlayContent = 23,
                OverlayTextInputFrame = 24,
                OverlayTextFocused = 25,
                OverlayListFrame = 26,
                OverlayListFrameFocused = 27,

                Header = 28,
                Footer = 29,
                Banner = 30
            };

            /** @brief Creates a Default (uncolored) Color. */
            Color() {
                this->value = -1LL;
            }

            /** @brief Creates a Color from a symbolic type.
             *  @param type the Color::Type to resolve to a color pair.
             */
            Color(Type type) {
                this->value = (type == Default) ? -1LL : COLOR_PAIR(type);
            }

            /** @brief Copies another color.
             *  @param color the source Color.
             */
            Color(const Color& color) {
                this->value = color;
            }

            /** @brief Implicitly converts to a curses attribute value.
             *  @return the raw int64_t color-pair value.
             */
            operator int64_t() const {
                return this->value;
            }

        private:
            int64_t value;   /**< The resolved curses color-pair value. */
    };

    /** @brief Global color system: initializes curses colors and manages themes.
     *
     *  @details Colors is a static utility. Init() must be called once during
     *  startup with the desired color Mode (RGB, Palette or Basic) and a
     *  background resolution BgType. SetTheme() loads a named theme file that
     *  remaps the Color::Type values, and ListThemes() enumerates the available
     *  themes. All windows obtain their colors through Color, so theme changes
     *  apply application-wide after the next redraw.
     */
    class Colors {
        private:
            Colors();

        public:
            /** @brief The terminal color capability mode. */
            enum Mode {
                RGB,      /**< True-color (24-bit) output. */
                Palette,  /**< 256-color palette output. */
                Basic     /**< Basic 8/16-color output. */
            };

            /** @brief How the background color is resolved. */
            enum BgType {
                Theme,    /**< Use the background defined by the active theme. */
                Inherit,  /**< Inherit the terminal's background color. */
            };

            /** @brief Initializes the curses color system.
             *  @param mode the color Mode to use.
             *  @param bgType the background resolution strategy.
             */
            static void Init(Mode mode = Mode::Basic, BgType bgType = BgType::Theme);
            /** @brief Loads a color theme by name.
             *  @param name the theme name.
             */
            static void SetTheme(const std::string& name);
            /** @brief Lists the available color themes.
             *  @return a vector of theme names.
             */
            static std::vector<std::string> ListThemes();
    };
}
