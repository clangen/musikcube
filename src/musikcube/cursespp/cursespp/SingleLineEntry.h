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

/** @file SingleLineEntry.h @brief An IScrollAdapter entry that renders a single line of text. */
#pragma once

#include <cursespp/IScrollAdapter.h>
#include <cursespp/Colors.h>

namespace cursespp {
    /** @brief A scroll adapter entry that displays exactly one line of text.
     *
     *  @details SingleLineEntry wraps a plain string and exposes it through
     *  the IScrollAdapter::IEntry interface. It owns its own Color attributes
     *  (unlike MultiLineEntry) and always reports a line count of one. The
     *  width set via SetWidth() is used by the scrollable window to truncate
     *  the rendered line, if necessary.
     */
    class SingleLineEntry : public IScrollAdapter::IEntry {
        public:
            /** @brief Creates a single-line entry.
             *  @param value the text to display.
             */
            SingleLineEntry(const std::string& value);

            /** @brief Sets the display width used for rendering/truncation.
             *  @param width the width in terminal cells.
             */
            void SetWidth(size_t width) override;
            /** @brief Returns the attributes for the single line.
             *  @param line the line index (ignored; always 0).
             *  @return the entry's Color.
             */
            Color GetAttrs(size_t line) override;
            /** @brief Returns the number of lines (always 1).
             *  @return 1.
             */
            size_t GetLineCount() override;
            /** @brief Returns the text of the line.
             *  @param line the line index (ignored; always 0).
             *  @return the entry's text.
             */
            std::string GetLine(size_t line) override;

            /** @brief Sets the entry's color attributes.
             *  @param attrs the new Color.
             */
            void SetAttrs(Color attrs);
            /** @brief Returns the raw text value.
             *  @return the entry's text.
             */
            std::string GetValue() { return value; }

        private:
            size_t width;       /**< The display width in cells. */
            std::string value;  /**< The rendered text. */
            Color attrs;        /**< The entry's color attributes. */
    };
}