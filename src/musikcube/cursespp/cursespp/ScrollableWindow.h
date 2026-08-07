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

/** @file ScrollableWindow.h @brief A window that scrolls content provided by an IScrollAdapter. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Window.h>
#include <cursespp/IScrollAdapter.h>
#include <cursespp/IScrollable.h>
#include <cursespp/IKeyHandler.h>

namespace cursespp {
    /** @brief A Window that displays a scrollable page of content via an IScrollAdapter.
     *
     *  @details ScrollableWindow renders the entries exposed by an
     *  IScrollAdapter within its content area. It tracks a ScrollPosition
     *  describing which entry is first visible and how many entries/lines fit,
     *  and handles arrow/page/scroll input through the IScrollable interface.
     *  Drawing is delegated to the adapter's DrawPage() so the window itself
     *  never knows the concrete data model. Optional arrow-key propagation
     *  lets an unhandled arrow bubble up to a parent (e.g. for tab-style
     *  navigation). The Scrollbar class is a friend and reads the same
     *  ScrollPosition to render a proportional scrollbar.
     */
    class ScrollableWindow:
        public Window,
        public IScrollable,
        public IKeyHandler
    {
        public:
            /** @brief Creates a scrollable window bound to an adapter.
             *  @param adapter the scroll adapter that supplies the content.
             *  @param parent the parent window, or nullptr.
             */
            ScrollableWindow(
                std::shared_ptr<IScrollAdapter> adapter,
                IWindow *parent = nullptr);

            /** @brief Creates a scrollable window without an adapter (set one later).
             *  @param parent the parent window, or nullptr.
             */
            ScrollableWindow(IWindow *parent = nullptr);

            /** @brief Destroys the window. */
            virtual ~ScrollableWindow();

            /* IWindow */
            /** @brief Marks the window visible and schedules a redraw. */
            void Show() override;
            /** @brief Notifies the window that its content area changed size. */
            void OnDimensionsChanged() override;
            /** @brief Handles navigation keys (arrows, page up/down, home/end) for scrolling.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            bool KeyPress(const std::string& key) override;
            /** @brief Gives the window focus. */
            void Focus() override;
            /** @brief Removes focus from the window. */
            void Blur() override;
            /** @brief Redraws the visible page via the adapter. */
            void OnRedraw() override;

            /* IMouseHandler */
            /** @brief Handles wheel events and scrollbar dragging.
             *  @param event the translated mouse event.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& event) override;

            /* IScrollable */
            /** @brief Scrolls to the first entry. */
            void ScrollToTop() override;
            /** @brief Scrolls to the last entry. */
            void ScrollToBottom() override;
            /** @brief Scrolls up by a number of entries.
             *  @param delta the number of entries to scroll.
             */
            void ScrollUp(int delta = 1) override;
            /** @brief Scrolls down by a number of entries.
             *  @param delta the number of entries to scroll.
             */
            void ScrollDown(int delta = 1) override;
            /** @brief Scrolls up by one page. */
            void PageUp() override;
            /** @brief Scrolls down by one page. */
            void PageDown() override;

            /** @brief Controls whether unhandled arrow keys propagate to the parent.
             *  @param allow true to propagate arrow keys upward.
             */
            void SetAllowArrowKeyPropagation(bool allow = true);

            /* virtual methods we define */
            /** @brief Swaps in a new scroll adapter.
             *  @param adapter the new adapter, or nullptr to detach.
             */
            virtual void SetAdapter(std::shared_ptr<IScrollAdapter> adapter);
            /** @brief Called after the adapter changes; subclasses may re-layout here. */
            virtual void OnAdapterChanged();
            /** @brief Returns the current scroll position.
             *  @return a const reference to the ScrollPosition.
             */
            virtual const IScrollAdapter::ScrollPosition& GetScrollPosition();

        protected:
            friend class Scrollbar;

            /** @brief Returns the active adapter.
             *  @return a reference to the IScrollAdapter.
             */
            virtual IScrollAdapter& GetScrollAdapter();
            /** @brief Returns a mutable scroll position for direct manipulation.
             *  @return a reference to the internal ScrollPosition.
             */
            virtual IScrollAdapter::ScrollPosition& GetMutableScrollPosition();

            /** @brief Returns the index of the first entry of the previous page.
             *  @return the entry index, or 0 if at the top.
             */
            size_t GetPreviousPageEntryIndex();
            /** @brief Returns whether the last entry of the dataset is currently visible.
             *  @return true if the bottom of the content is reached.
             */
            bool IsLastItemVisible();

        private:
            std::shared_ptr<IScrollAdapter> adapter;           /**< The content provider. */
            IScrollAdapter::ScrollPosition scrollPosition;     /**< The current viewport position. */
            bool allowArrowKeyPropagation;                     /**< Whether arrow keys may bubble to the parent. */
    };
}
