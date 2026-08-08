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

/** @file IScrollAdapter.h @brief Interface providing content to scrollable windows. */
#pragma once

#include <string>
#include <memory>
#include <cursespp/Colors.h>

namespace cursespp {
    class ScrollableWindow;

    /** @brief Contract decoupling a ScrollableWindow from its data model.
     *
     *  @details An IScrollAdapter supplies the entries that a ScrollableWindow
     *  renders. The window informs the adapter of its display size, queries the
     *  total entry count, requests individual entries for focus/hit-testing,
     *  and asks the adapter to DrawPage() when the viewport needs repainting.
     *  This lets one scrollable window display very different data sources
     *  (plain strings, wrapped text, custom models) without changing the
     *  window itself.
     */
    class IScrollAdapter {
        public:
            virtual ~IScrollAdapter() { }

            /** @brief Describes the current viewport state of the scrollable window. */
            struct ScrollPosition {
                ScrollPosition() {
                    firstVisibleEntryIndex = 0;
                    visibleEntryCount = 0;
                    lineCount = 0;
                    logicalIndex = 0;
                    totalEntries = 0;
                }

                size_t firstVisibleEntryIndex;  /**< Index of the first entry visible at the top. */
                size_t visibleEntryCount;       /**< Number of entries currently visible. */
                size_t lineCount;               /**< Number of display lines in the viewport. */
                size_t totalEntries;            /**< Total number of entries in the data set. */
                size_t logicalIndex;            /**< The logical (focus) index within the viewport. */
            };

            /** @brief A single entry rendered by the scrollable window. */
            class IEntry {
                public:
                    virtual ~IEntry() { }
                    /** @brief Returns the number of display lines this entry occupies.
                     *  @return the line count.
                     */
                    virtual size_t GetLineCount() = 0;
                    /** @brief Returns the text of a display line.
                     *  @param line the zero-based line index.
                     *  @return the line's text.
                     */
                    virtual std::string GetLine(size_t line) = 0;
                    /** @brief Sets the display width used for wrapping/truncation.
                     *  @param width the width in terminal cells.
                     */
                    virtual void SetWidth(size_t width) = 0;
                    /** @brief Returns the color attributes of a display line.
                     *  @param line the zero-based line index.
                     *  @return the Color for that line.
                     */
                    virtual Color GetAttrs(size_t line) = 0;
            };

            /** @brief Shared-pointer alias for an entry. */
            typedef std::shared_ptr<IEntry> EntryPtr;

            /** @brief Informs the adapter of the window's display size.
             *  @param width the width in terminal cells.
             *  @param height the height in terminal cells.
             */
            virtual void SetDisplaySize(size_t width, size_t height) = 0;
            /** @brief Returns the total number of entries.
             *  @return the entry count.
             */
            virtual size_t GetEntryCount() = 0;
            /** @brief Returns the entry at a given index.
             *  @param window the requesting ScrollableWindow.
             *  @param index the entry index.
             *  @return the EntryPtr at that index.
             */
            virtual EntryPtr GetEntry(ScrollableWindow* window, size_t index) = 0;
            /** @brief Draws the visible page starting at the given index.
             *  @param window the ScrollableWindow to draw into.
             *  @param index the index of the first visible entry.
             *  @param result receives the computed ScrollPosition.
             */
            virtual void DrawPage(ScrollableWindow* window, size_t index, ScrollPosition& result) = 0;
    };

    /** @brief Shared-pointer alias for an IScrollAdapter. */
    typedef std::shared_ptr<IScrollAdapter> IScrollAdapterPtr;
}
