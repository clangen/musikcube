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

/** @file LayoutBase.h @brief Base class for layouts that arrange child windows and manage focus. */
#pragma once

#include <cursespp/ILayout.h>
#include <cursespp/Window.h>

#include <sigslot/sigslot.h>
#include <vector>

namespace cursespp {
    /** @brief A Window that also implements ILayout, managing child windows and focus.
     *
     *  @details LayoutBase is a Window that owns a collection of child windows
     *  in z-order and an ordered subset of "focusable" windows. It forwards
     *  key/mouse input to the focused child and implements linear focus
     *  navigation (next/prev/first/last). When navigation reaches the edge of
     *  the focusable list, the FocusTerminated or FocusWrapped signals fire
     *  depending on the FocusMode. Subclasses override OnLayout() to arrange
     *  child geometry and may override EnsureValidFocus() to customize how a
     *  valid focus target is chosen. All redraw happens top-down, so children
     *  must be marked invalid via Invalidate() rather than drawing directly.
     */
    class LayoutBase:
        public Window,
        public ILayout
    {
        public:
            /** @brief The direction in which focus navigation is moving. */
            enum FocusDirection {
                FocusForward,      /**< Navigating toward the last focusable window. */
                FocusBackward      /**< Navigating toward the first focusable window. */
            };

            /** @brief Fired when focus navigation reaches the edge of the focusable list and terminates. */
            sigslot::signal1<FocusDirection> FocusTerminated;
            /** @brief Fired when focus navigation wraps around to the opposite edge. */
            sigslot::signal1<FocusDirection> FocusWrapped;

            /** @brief Creates a layout.
             *  @param parent the parent window, or nullptr for a root layout.
             */
            LayoutBase(IWindow* parent = NULL);
            /** @brief Destroys the layout and its child windows. */
            ~LayoutBase() override;

            /* IWindow */
            /** @brief Makes the layout (and its children) visible. */
            void Show() override;
            /** @brief Hides the layout (and its children). */
            void Hide() override;
            /** @brief Marks the layout and its children as needing a redraw. */
            void Invalidate() override;
            /** @brief Propagates a parent visibility change to children.
             *  @param visible the new visible state.
             */
            void OnParentVisibilityChanged(bool visible) override;
            /** @brief Responds to a child becoming visible or hidden.
             *  @param visible the child's new visible state.
             *  @param child the child window whose visibility changed.
             */
            void OnChildVisibilityChanged(bool visible, IWindow* child) override;
            /** @brief Gives focus to the first focusable child window. */
            void Focus() override;

            /* IOrderable */
            /** @brief Moves the layout to the front of the z-order. */
            void BringToTop() override;
            /** @brief Moves the layout to the back of the z-order. */
            void SendToBottom() override;

            /* ILayout */
            /** @brief Moves focus to the next focusable window.
             *  @return the newly focused window, or nullptr.
             */
            IWindowPtr FocusNext() override;
            /** @brief Moves focus to the previous focusable window.
             *  @return the newly focused window, or nullptr.
             */
            IWindowPtr FocusPrev() override;

            /** @brief Returns the currently focused window.
             *  @return the focused IWindowPtr, or nullptr if none is focused.
             */
            IWindowPtr GetFocus() override;
            /** @brief Sets focus to the given window.
             *  @param window the window to focus.
             *  @return true if the window was found and focused.
             */
            bool SetFocus(IWindowPtr window) override;

            /** @brief Returns the index of the currently focused window.
             *  @return the focus index into the focusable list.
             */
            int GetFocusIndex() override;
            /** @brief Sets the focused window by index.
             *  @param index the focus index to select.
             *  @param applyFocus true to actually focus the window, false to only record the index.
             */
            void SetFocusIndex(int index, bool applyFocus = true) override;

            /** @brief Returns the number of focusable windows.
             *  @return the focusable window count.
             */
            int GetFocusableCount() override;
            /** @brief Returns the focusable window at a given index.
             *  @param index the focus index.
             *  @return the window at that index.
             */
            IWindowPtr GetFocusableAt(int index) override;

            /** @brief Returns the focus navigation mode.
             *  @return the active FocusMode.
             */
            FocusMode GetFocusMode() const override;
            /** @brief Sets the focus navigation mode.
             *  @param mode the FocusMode to use.
             */
            void SetFocusMode(FocusMode mode) override;

            /** @brief Moves focus to the first focusable window.
             *  @return the window that received focus.
             */
            IWindowPtr FocusFirst() override;
            /** @brief Moves focus to the last focusable window.
             *  @return the window that received focus.
             */
            IWindowPtr FocusLast() override;

            /** @brief Called when this layout's own visibility changes.
             *  @param visible the new visible state.
             */
            void OnVisibilityChanged(bool visible) override;

            /** @brief Arranges all child windows based on their geometry. */
            void Layout() override;

            /* IKeyHandler */
            /** @brief Forwards a key press to the focused child window.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            bool KeyPress(const std::string& key) override;

            /* IMouseHandler */
            /** @brief Forwards a mouse event to the child window under the cursor.
             *  @param mouseEvent the translated mouse event.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& mouseEvent) override;

            /* IWindowGroup */
            /** @brief Adds a child window and re-indexes the focusable list.
             *  @param window the window to add.
             *  @return true on success.
             */
            bool AddWindow(IWindowPtr window) override;
            /** @brief Removes a child window and re-indexes the focusable list.
             *  @param window the window to remove.
             *  @return true if the window was found and removed.
             */
            bool RemoveWindow(IWindowPtr window) override;
            /** @brief Returns the number of child windows.
             *  @return the child window count.
             */
            size_t GetWindowCount() override;
            /** @brief Returns the child window at a given position.
             *  @param position the zero-based index into the child list.
             *  @return the window at that position.
             */
            IWindowPtr GetWindowAt(size_t position) override;

            /* virtual methods we define */
            /** @brief Arranges child windows; subclasses should override this instead of Layout(). */
            virtual void OnLayout();

        protected:

            /** @brief Ensures the focused window is still valid, focusing the best fallback if not.
             *  @return the resolved focused window.
             */
            IWindowPtr EnsureValidFocus();

            /* virtual methods we define */
            /** @brief Computes a valid focus target when navigating forward.
             *  @return the next valid focused window.
             */
            virtual IWindowPtr EnsureValidFocusFromNext();
            /** @brief Computes a valid focus target when navigating backward.
             *  @return the previous valid focused window.
             */
            virtual IWindowPtr EnsureValidFocusFromPrev();

        private:
            void AddFocusable(IWindowPtr window);
            void RemoveFocusable(IWindowPtr window);
            void SortFocusables();
            void IndexFocusables();
            LayoutBase* GetLayoutAtFocusIndex();

            std::vector<IWindowPtr> children;   /**< All child windows in z-order. */
            std::vector<IWindowPtr> focusable;  /**< The ordered subset of focusable children. */
            int focused;                        /**< Index of the currently focused window. */
            FocusMode focusMode;                /**< The focus navigation mode. */
    };
}
