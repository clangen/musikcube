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

/** @file ListWindow.h @brief A selectable, scrollable list backed by an IScrollAdapter. */
#pragma once

#include <cursespp/IScrollable.h>
#include <cursespp/IScrollAdapter.h>
#include <cursespp/ScrollableWindow.h>
#include <sigslot/sigslot.h>
#include <functional>

namespace cursespp {
    /** @brief A ScrollableWindow that adds a cursor/selection and user interactions.
     *
     *  @details ListWindow extends ScrollableWindow with a highlighted selection
     *  cursor. It emits sigslot signals for selection changes, list invalidation
     *  and entry activation/context-menu requests so owners can react to user
     *  input. Scrolling always keeps the selected entry visible. An optional
     *  Decorator can customize drawing of the frame/content, and the window can
     *  show a proportional Scrollbar in the right gutter. Mouse wheel events
     *  are translated into scrolling/selection moves.
     */
    class ListWindow: public ScrollableWindow {
        public:
            /** @brief Sentinel value representing "no selection" (no entry highlighted). */
            static size_t NO_SELECTION;

            /** @brief Callback used to customize how the list draws itself. */
            using Decorator = std::function<void(ListWindow*)>;

            /** @brief Fired with the list, new index and old index when the selection changes. */
            sigslot::signal3<ListWindow*, size_t, size_t> SelectionChanged;
            /** @brief Fired with the list and the affected index when the adapter changes. */
            sigslot::signal2<ListWindow*, size_t> Invalidated;
            /** @brief Fired with the list and the index when the user activates an entry (e.g. Enter). */
            sigslot::signal2<ListWindow*, size_t> EntryActivated;
            /** @brief Fired with the list and the index when the user requests a context menu. */
            sigslot::signal2<ListWindow*, size_t> EntryContextMenu;

            /** @brief Creates a list backed by the given adapter.
             *  @param adapter the scroll adapter supplying the entries.
             *  @param parent the parent window, or nullptr.
             */
            ListWindow(std::shared_ptr<IScrollAdapter> adapter, IWindow *parent = nullptr);
            /** @brief Creates an empty list (adapter can be set later).
             *  @param parent the parent window, or nullptr.
             */
            ListWindow(IWindow *parent = nullptr);

            /** @brief Destroys the list. */
            virtual ~ListWindow();

            /** @brief Scrolls to the top and selects the first entry. */
            void ScrollToTop() override;
            /** @brief Scrolls to the bottom and selects the last entry. */
            void ScrollToBottom() override;
            /** @brief Scrolls up, moving the selection.
             *  @param delta the number of entries to move.
             */
            void ScrollUp(int delta = 1) override;
            /** @brief Scrolls down, moving the selection.
             *  @param delta the number of entries to move.
             */
            void ScrollDown(int delta = 1) override;
            /** @brief Scrolls up one page. */
            void PageUp() override;
            /** @brief Scrolls down one page. */
            void PageDown() override;

            /** @brief Marks the list and its backing content as needing a redraw. */
            void Invalidate() override;
            /** @brief Called when the scroll adapter is swapped; resets the selection. */
            void OnAdapterChanged() override;

            /** @brief Returns the current scroll position.
             *  @return a const reference to the ScrollPosition.
             */
            const IScrollAdapter::ScrollPosition& GetScrollPosition() override;

            /** @brief Handles keys for selection navigation, activation and context menu.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            bool KeyPress(const std::string& key) override;
            /** @brief Handles clicks and wheel events.
             *  @param event the translated mouse event.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& event) override;

            /** @brief Shows or hides the scrollbar gutter.
             *  @param visible true to draw the scrollbar.
             */
            void SetScrollbarVisible(bool visible);
            /** @brief Installs a drawing decorator.
             *  @param decorator the decorator callback, or nullptr to clear it.
             */
            void SetDecorator(Decorator decorator);

            /* virtual methods we define */
            /** @brief Scrolls so the given entry becomes the first visible entry.
             *  @param index the entry index to scroll to.
             */
            virtual void ScrollTo(size_t index);
            /** @brief Returns the currently selected entry index.
             *  @return the selected index, or NO_SELECTION.
             */
            virtual size_t GetSelectedIndex();
            /** @brief Moves the selection to the given index.
             *  @param index the index to select.
             */
            virtual void SetSelectedIndex(size_t index);
            /** @brief Returns whether the given entry is currently visible.
             *  @param index the entry index.
             *  @return true if the entry is within the visible range.
             */
            virtual bool IsEntryVisible(size_t index);

        protected:
            /** @brief Handles content area resizing by re-adjusting the scroll position. */
            void OnDimensionsChanged() override;
            /** @brief Draws the frame; applies the decorator when set. */
            void DecorateFrame() override;
            /** @brief Returns a mutable reference to the internal scroll position.
             *  @return a reference to the ScrollPosition.
             */
            IScrollAdapter::ScrollPosition& GetMutableScrollPosition() override;

            /* virtual methods we define */
            /** @brief Called when the selection changes; subclasses may override.
             *  @param newIndex the newly selected index.
             *  @param oldIndex the previously selected index.
             */
            virtual void OnSelectionChanged(size_t newIndex, size_t oldIndex);
            /** @brief Called when an entry is activated; subclasses may override.
             *  @param index the activated entry index.
             *  @return true if the activation was handled.
             */
            virtual bool OnEntryActivated(size_t index);
            /** @brief Called when a context-menu is requested for an entry.
             *  @param index the entry index.
             *  @return true if the request was handled.
             */
            virtual bool OnEntryContextMenu(size_t index);
            /** @brief Called when the list is invalidated; subclasses may override. */
            virtual void OnInvalidated();

        private:
            bool IsSelectedItemCompletelyVisible();

            bool showScrollbar;                            /**< Whether the scrollbar gutter is drawn. */
            IScrollAdapter::ScrollPosition scrollPosition; /**< The current viewport position. */
            size_t selectedIndex;                          /**< The currently selected entry index. */
            Decorator decorator;                           /**< Optional drawing decorator. */
    };
}
