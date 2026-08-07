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

/** @file ScrollAdapterBase.h @brief Base implementation of IScrollAdapter with paged drawing support. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Colors.h>
#include <cursespp/IScrollAdapter.h>
#include <functional>
#include <deque>

namespace cursespp {
    /** @brief Base class for scroll adapters, providing viewport-aware page drawing.
     *
     *  @details ScrollAdapterBase implements the drawing half of IScrollAdapter
     *  in a reusable way. It caches the display size set by the owning
     *  ScrollableWindow and computes which entries are visible when a page is
     *  drawn. Subclasses implement the data model: GetEntryCount(), GetEntry()
     *  and GetLineCount(). An optional ItemDecorator callback lets callers
     *  customize the color used for each entry line. All drawing happens
     *  against the ncurses content window of the ScrollableWindow.
     */
    class ScrollAdapterBase : public IScrollAdapter {
        public:
            /** @brief Callback that determines the color of an entry line.
             *  @param window the scrollable window being drawn.
             *  @param index the entry index.
             *  @param line the line within the entry.
             *  @param entry the entry being drawn.
             *  @return the Color to use for the line.
             */
            typedef std::function<Color(
                ScrollableWindow*,
                size_t,
                size_t,
                EntryPtr)> ItemDecorator;

            /** @brief Creates an adapter. */
            ScrollAdapterBase();
            /** @brief Destroys the adapter. */
            virtual ~ScrollAdapterBase();

            /** @brief Records the display size provided by the scrollable window.
             *  @param width the display width in cells.
             *  @param height the display height in cells.
             */
            void SetDisplaySize(size_t width, size_t height) override;
            /** @brief Draws the page starting at the given index.
             *  @param window the ScrollableWindow to draw into.
             *  @param index the index of the first visible entry.
             *  @param result receives the computed ScrollPosition.
             */
            void DrawPage(ScrollableWindow* window, size_t index, ScrollPosition& result) override;

            /* virtual methods we define */
            /** @brief Returns the number of display lines each entry occupies.
             *  @return the line count per entry (default 1).
             */
            virtual size_t GetLineCount();
            /** @brief Installs a color decorator for entry lines.
             *  @param decorator the decorator callback.
             */
            virtual void SetItemDecorator(ItemDecorator decorator) { this->decorator = decorator; }

        protected:
            /** @brief Fills target with the entries visible from a given top index.
             *  @param window the scrollable window being rendered.
             *  @param desiredTopIndex the index of the first visible entry.
             *  @param target the deque to fill with visible entries.
             *  @return the index of the first visible entry actually used.
             */
            size_t GetVisibleItems(
                cursespp::ScrollableWindow* window,
                size_t desiredTopIndex,
                std::deque<EntryPtr>& target);

            /** @brief Returns the installed item decorator.
             *  @return the ItemDecorator callback.
             */
            virtual ItemDecorator GetItemDecorator() { return this->decorator; }

            /** @brief Returns the cached display width.
             *  @return the width in cells.
             */
            size_t GetWidth() noexcept { return this->width; }
            /** @brief Returns the cached display height.
             *  @return the height in cells.
             */
            size_t GetHeight() noexcept { return this->height; }

        private:
            size_t width, height;        /**< The cached display dimensions. */
            ItemDecorator decorator;     /**< Optional per-line color decorator. */
    };
}
