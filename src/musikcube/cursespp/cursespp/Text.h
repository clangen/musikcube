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

/** @file Text.h @brief Text layout helpers and key-name normalization utilities. */
#pragma once

#include <string>
#include <vector>

namespace cursespp {
    /** @brief Text layout helpers for measuring, aligning and breaking strings. */
    namespace text {
        /** @brief The horizontal alignment of rendered text. */
        enum TextAlign {
            AlignLeft,      /**< Align text to the left edge. */
            AlignCenter,    /**< Center text within the available width. */
            AlignRight      /**< Align text to the right edge. */
        };

        /** @brief Truncates a string to a maximum display width, adding an ellipsis.
         *  @param str the source string.
         *  @param len the maximum display width in cells.
         *  @return the ellipsized string.
         */
        std::string Ellipsize(const std::string& str, size_t len);
        /** @brief Pads a string so it fits a width with the given alignment.
         *  @param str the source string.
         *  @param align the TextAlign to apply.
         *  @param len the target display width in cells.
         *  @return the aligned string.
         */
        std::string Align(const std::string& str, TextAlign align, size_t len);
        /** @brief Splits a string into display lines that fit the given width.
         *  @param line the source string.
         *  @param width the maximum width per line in cells.
         *  @return the list of broken lines.
         */
        std::vector<std::string> BreakLines(const std::string& line, size_t width);
        /** @brief Splits a string on one or more delimiter characters.
         *  @param str the source string.
         *  @param delimiters the set of delimiter characters (default space).
         *  @param trimEmpty true to omit empty tokens.
         *  @return the list of tokens.
         */
        std::vector<std::string> Split(const std::string& str, const std::string& delimiters = " ", bool trimEmpty = false);
    }

    /** @brief Keyboard key-name helpers used by the input pipeline. */
    namespace key {
        /** @brief Normalizes a key name to a canonical form (e.g. "KEY_UP" -> "UP").
         *  @param keyname the raw key name.
         *  @return the normalized key string.
         */
        std::string Normalize(const std::string& keyname);
        /** @brief Translates a raw curses character code into a normalized key string.
         *  @param ch the raw key code.
         *  @return the normalized key string, e.g. "ENTER" or "F5".
         */
        std::string Read(int64_t ch);
    }
}
