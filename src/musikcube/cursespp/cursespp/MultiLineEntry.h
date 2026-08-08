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

/** @file MultiLineEntry.h @brief An IScrollAdapter entry that wraps text across multiple lines. */
#pragma once

#include <cursespp/IScrollAdapter.h>
#include <vector>

namespace cursespp {
    /** @brief A scroll adapter entry that word-wraps a single string into multiple lines.
     *
     *  @details MultiLineEntry takes a long string and breaks it into display
     *  lines at the width imposed by SetWidth(). It implements
     *  IScrollAdapter::IEntry so a ScrollableWindow can render each wrapped
     *  line individually. All lines share the same Color attributes. Re-setting
     *  the width re-computes the wrapped lines, so the owning window should
     *  re-query GetLineCount() after a resize.
     */
    class MultiLineEntry : public IScrollAdapter::IEntry {
        public:
            /** @brief Creates a wrapped entry.
             *  @param value the full text to display.
             *  @param attrs the Color attributes for all lines.
             */
            MultiLineEntry(const std::string& value, Color attrs = Color::Default);

            /** @brief Returns the number of wrapped lines.
             *  @return the line count.
             */
            size_t GetLineCount() override;
            /** @brief Returns the text of a wrapped line.
             *  @param line the zero-based line index.
             *  @return the line's text.
             */
            std::string GetLine(size_t line) override;
            /** @brief Sets the display width and re-wraps the text.
             *  @param width the width in terminal cells.
             */
            void SetWidth(size_t width) override;
            /** @brief Returns the attributes for a line.
             *  @param line the zero-based line index.
             *  @return the Color for that line.
             */
            Color GetAttrs(size_t line) override;

        private:
            std::string value;                 /**< The full unwrapped text. */
            std::vector<std::string> lines;    /**< The wrapped display lines. */
            size_t charCount;                  /**< Number of characters in the source text. */
            Color attrs;                       /**< Shared attributes for all lines. */
            size_t width;                      /**< The display width in cells. */
    };
}