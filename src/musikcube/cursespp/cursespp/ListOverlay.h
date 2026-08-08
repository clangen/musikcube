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

/** @file ListOverlay.h @brief A modal overlay that lets the user pick an item from a list. */
#pragma once

#include <cursespp/OverlayBase.h>
#include <cursespp/ListWindow.h>

#include <vector>
#include <map>

namespace cursespp {
    /** @brief A modal overlay presenting a selectable list of items.
     *
     *  @details ListOverlay displays an IScrollAdapter-backed list inside a
     *  titled box. The user navigates with arrows/page keys; activating an
     *  entry fires the ItemSelectedCallback, and the optional
     *  DeleteKeyCallback lets a delete key remove entries from the adapter.
     *  A KeyInterceptorCallback can observe or swallow individual keys. When
     *  dismissed, the DismissedCallback fires. The overlay optionally shows a
     *  scrollbar and auto-dismisses on selection unless disabled.
     */
    class ListOverlay:
        public OverlayBase,
        public sigslot::has_slots<>
    {
        public:
            /** @brief Callback fired when the user selects an item. */
            using ItemSelectedCallback = std::function<void(ListOverlay* sender, IScrollAdapterPtr adapter, size_t index)>;
            /** @brief Callback fired when the user presses the delete key on an item. */
            using DeleteKeyCallback = std::function<void(ListOverlay* sender, IScrollAdapterPtr adapter, size_t index)>;
            /** @brief Callback fired when the overlay is dismissed. */
            using DismissedCallback = std::function<void(ListOverlay* sender)>;
            /** @brief Callback that observes/swallows keys before normal handling.
             *  @return true to consume the key, false to allow normal handling.
             */
            using KeyInterceptorCallback = std::function<bool(ListOverlay* sender, std::string key)>;

            /** @brief Creates an empty list overlay. */
            ListOverlay();
            /** @brief Destroys the overlay. */
            virtual ~ListOverlay();

            /** @brief Sets the overlay title.
             *  @param title the title text.
             *  @return *this for chaining.
             */
            ListOverlay& SetTitle(const std::string& title);
            /** @brief Sets the item provider.
             *  @param adapter the IScrollAdapterPtr supplying the items.
             *  @return *this for chaining.
             */
            ListOverlay& SetAdapter(IScrollAdapterPtr adapter);
            /** @brief Registers the item-selection callback.
             *  @param cb the ItemSelectedCallback.
             *  @return *this for chaining.
             */
            ListOverlay& SetItemSelectedCallback(ItemSelectedCallback cb);
            /** @brief Registers the delete-key callback.
             *  @param cb the DeleteKeyCallback.
             *  @return *this for chaining.
             */
            ListOverlay& SetDeleteKeyCallback(DeleteKeyCallback cb);
            /** @brief Registers the dismissed callback.
             *  @param cb the DismissedCallback.
             *  @return *this for chaining.
             */
            ListOverlay& SetDismissedCallback(DismissedCallback cb);
            /** @brief Registers a key interceptor callback.
             *  @param cb the KeyInterceptorCallback.
             *  @return *this for chaining.
             */
            ListOverlay& SetKeyInterceptorCallback(KeyInterceptorCallback cb);
            /** @brief Selects an item when the overlay is shown.
             *  @param index the index to select.
             *  @return *this for chaining.
             */
            ListOverlay& SetSelectedIndex(size_t index);
            /** @brief Sets the overlay width in cells.
             *  @param width the desired width.
             *  @return *this for chaining.
             */
            ListOverlay& SetWidth(int width);
            /** @brief Sets the overlay width as a percentage of the screen.
             *  @param percent the width percentage (e.g. 50).
             *  @return *this for chaining.
             */
            ListOverlay& SetWidthPercent(int percent);
            /** @brief Controls whether the overlay dismisses itself after selection.
             *  @param autoDismiss true to auto-dismiss.
             *  @return *this for chaining.
             */
            ListOverlay& SetAutoDismiss(bool autoDismiss);

            /** @brief Returns the currently selected item index.
             *  @return the selected index, or ListWindow::NO_SELECTION.
             */
            size_t GetSelectedIndex();

            /** @brief Arranges the overlay and its contents. */
            void Layout() override;
            /** @brief Handles navigation, selection, delete and interceptor keys.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            bool KeyPress(const std::string& key) override;

            /** @brief Re-applies the adapter's contents to the internal list window. */
            void RefreshAdapter();

        protected:
            /** @brief Called when the overlay becomes visible or hidden.
             *  @param visible the new visible state.
             */
            void OnVisibilityChanged(bool visible) override;
            /** @brief Called after the overlay is dismissed. */
            void OnDismissed() override;

        private:
            void OnListEntryActivated(cursespp::ListWindow* sender, size_t index);

            class CustomListWindow;

            void RecalculateSize();
            bool ScrollbarVisible();
            void UpdateContents();

            std::string title;                          /**< The overlay title. */
            int x, y;                                   /**< The overlay position. */
            int width, height;                          /**< The overlay dimensions. */
            int setWidth, setWidthPercent;              /**< Requested width and width percentage. */
            bool autoDismiss;                           /**< Whether selection dismisses the overlay. */
            IScrollAdapterPtr adapter;                  /**< The item provider. */
            std::shared_ptr<CustomListWindow> listWindow; /**< The internal list window. */
            std::shared_ptr<Window> scrollbar;          /**< The optional scrollbar window. */
            ItemSelectedCallback itemSelectedCallback;  /**< Callback on item selection. */
            DeleteKeyCallback deleteKeyCallback;        /**< Callback on delete key. */
            DismissedCallback dismissedCallback;        /**< Callback on dismissal. */
            KeyInterceptorCallback keyInterceptorCallback; /**< Callback observing/swallowing keys. */
    };
}