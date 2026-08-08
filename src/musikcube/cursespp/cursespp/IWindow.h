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

/** @file IWindow.h @brief Abstract contract for every window in the cursespp toolkit. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/IDisplayable.h>
#include <cursespp/IOrderable.h>
#include <cursespp/IMouseHandler.h>
#include <cursespp/Colors.h>

#include <musikcore/runtime/IMessage.h>
#include <musikcore/runtime/IMessageTarget.h>
#include <string>

namespace cursespp {
    /** @brief The base interface implemented by all windows in the toolkit.
     *
     *  @details IWindow is the central abstraction of cursespp. Every on-screen
     *  element -- windows, layouts and overlays -- derives from this interface.
     *  It combines a handful of smaller interfaces:
     *  - IOrderable, which allows a window to change its z-order (top/bottom).
     *  - IDisplayable, which allows a window to be shown or hidden.
     *  - IMouseHandler, which lets the window consume mouse events.
     *  - musik::core::runtime::IMessageTarget, which lets the window receive
     *    messages posted to it from other parts of the application.
     *
     *  Windows are backed by one or more ncurses/PDCurses WINDOW handles. The
     *  "frame" is the outer border (drawn when the frame is visible) while the
     *  "content" is the interior region where the window draws its actual data.
     *  All coordinates are expressed in terminal cells relative to the parent
     *  window. A window does not necessarily have to repaint on every event;
     *  callers use Invalidate() to mark the window dirty and the framework
     *  repaints it during the next Redraw() pass.
     *
     *  @note All methods must be implemented by concrete subclasses. Implementors
     *  should remember that drawing must be performed with ncurses calls against
     *  the frame/content handles, and that a window should only touch the screen
     *  between a `refresh()`/`wrefresh()` cycle, as the toolkit redraws top-down.
     */
    class IWindow :
        public IOrderable,
        public IDisplayable,
        public IMouseHandler,
        public musik::core::runtime::IMessageTarget
    {
        public:
            virtual ~IWindow() { }

            /** @brief Marks the window's content as stale so it is repainted on the next Redraw(). */
            virtual void Invalidate() = 0;

            /** @brief Immediately redraws the window and its children onto the terminal. */
            virtual void Redraw() = 0;

            /** @brief Sets the parent window.
             *  @param parent the new parent, or nullptr to make this window an orphan.
             */
            virtual void SetParent(IWindow* parent) = 0;

            /** @brief Gives keyboard focus to the window and triggers the focused visual state. */
            virtual void Focus() = 0;

            /** @brief Removes keyboard focus from the window and restores the unfocused visual state. */
            virtual void Blur() = 0;

            /** @brief Sets the color used for the window's content region.
             *  @param color the curses color pair to use for content drawing.
             */
            virtual void SetContentColor(Color color) = 0;

            /** @brief Sets the color used for the window's frame border.
             *  @param color the curses color pair to use for the frame.
             */
            virtual void SetFrameColor(Color color) = 0;

            /** @brief Sets the frame color used while the window has focus.
             *  @param color the curses color pair to use for a focused frame.
             */
            virtual void SetFocusedFrameColor(Color color) = 0;

            /** @brief Sets the content color used while the window has focus.
             *  @param color the curses color pair to use for focused content.
             */
            virtual void SetFocusedContentColor(Color color) = 0;

            /** @brief Returns the current content color.
             *  @return the curses color pair used for content drawing.
             */
            virtual Color GetContentColor() = 0;

            /** @brief Returns the current frame color.
             *  @return the curses color pair used for the frame border.
             */
            virtual Color GetFrameColor() = 0;

            /** @brief Returns the content color used when the window is focused.
             *  @return the focused content curses color pair.
             */
            virtual Color GetFocusedContentColor() = 0;

            /** @brief Returns the frame color used when the window is focused.
             *  @return the focused frame curses color pair.
             */
            virtual Color GetFocusedFrameColor() = 0;

            /** @brief Shows or hides the window's frame border.
             *  @param visible true to draw the border, false to hide it.
             */
            virtual void SetFrameVisible(bool visible) = 0;

            /** @brief Returns whether the window's frame border is currently visible.
             *  @return true if the frame is drawn.
             */
            virtual bool IsFrameVisible() = 0;

            /** @brief Resizes the window.
             *  @param width the new width in terminal cells.
             *  @param height the new height in terminal cells.
             */
            virtual void SetSize(int width, int height) = 0;

            /** @brief Moves the window to a new position relative to its parent.
             *  @param x the new horizontal offset (0 = left edge of parent).
             *  @param y the new vertical offset (0 = top edge of parent).
             */
            virtual void SetPosition(int x, int y) = 0;

            /** @brief Convenience helper that moves and resizes the window in one call.
             *  @param x the new horizontal offset.
             *  @param y the new vertical offset.
             *  @param width the new width.
             *  @param height the new height.
             */
            virtual void MoveAndResize(int x, int y, int width, int height) = 0;

            /** @brief Returns the current window width.
             *  @return the width in terminal cells.
             */
            virtual int GetWidth() const = 0;

            /** @brief Returns the current window height.
             *  @return the height in terminal cells.
             */
            virtual int GetHeight() const = 0;

            /** @brief Returns the height of the content region (excludes the frame border).
             *  @return the content height in terminal cells.
             */
            virtual int GetContentHeight() const = 0;

            /** @brief Returns the width of the content region (excludes the frame border).
             *  @return the content width in terminal cells.
             */
            virtual int GetContentWidth() const = 0;

            /** @brief Returns the horizontal position relative to the parent window.
             *  @return the relative x offset in terminal cells.
             */
            virtual int GetX() const = 0;

            /** @brief Returns the vertical position relative to the parent window.
             *  @return the relative y offset in terminal cells.
             */
            virtual int GetY() const = 0;

            /** @brief Returns the absolute horizontal position on the terminal screen.
             *  @return the screen-absolute x offset in terminal cells.
             */
            virtual int GetAbsoluteX() const = 0;

            /** @brief Returns the absolute vertical position on the terminal screen.
             *  @return the screen-absolute y offset in terminal cells.
             */
            virtual int GetAbsoluteY() const = 0;

            /** @brief Returns the unique identifier assigned to this window.
             *  @return the window's id.
             */
            virtual int GetId() const = 0;

            /** @brief Returns the focus order of the window within its parent layout.
             *  @return the focus order value; lower values are focused earlier.
             */
            virtual int GetFocusOrder() = 0;

            /** @brief Sets the focus order of the window within its parent layout.
             *  @param order the desired focus order, or -1 to assign a default.
             */
            virtual void SetFocusOrder(int order = -1) = 0;

            /** @brief Returns the ncurses WINDOW handle used for the frame border.
             *  @return the underlying frame WINDOW*, or nullptr if unavailable.
             */
            virtual WINDOW* GetFrame() const = 0;

            /** @brief Returns the ncurses WINDOW handle used for the content region.
             *  @return the underlying content WINDOW*, or nullptr if unavailable.
             */
            virtual WINDOW* GetContent() const = 0;

            /** @brief Returns whether the window (and its ancestors) are visible.
             *  @return true if the window is currently displayed.
             */
            virtual bool IsVisible() = 0;

            /** @brief Returns whether the window currently holds keyboard focus.
             *  @return true if the window is focused.
             */
            virtual bool IsFocused() = 0;

            /** @brief Returns the parent window.
             *  @return the parent IWindow*, or nullptr if this window is a root.
             */
            virtual IWindow* GetParent() const = 0;

            /** @brief Called by the framework when an ancestor changes visibility.
             *  @param visible the new visible state propagated to this window.
             */
            virtual void OnParentVisibilityChanged(bool visible) = 0;

            /** @brief Called by the framework when a child window changes visibility.
             *  @param visible the new visible state of the child.
             *  @param child the child window whose visibility changed.
             */
            virtual void OnChildVisibilityChanged(bool visible, IWindow* child) = 0;

            /** @brief Returns whether this window is the top-most window in its z-order.
             *  @return true if the window is on top of all its siblings.
             */
            virtual bool IsTop() = 0;

            /** @brief Sets the title rendered on the frame border.
             *  @param title the title string, or empty to clear it.
             */
            virtual void SetFrameTitle(const std::string& title) = 0;

            /** @brief Returns the title rendered on the frame border.
             *  @return the current frame title string.
             */
            virtual std::string GetFrameTitle() const = 0;
    };

    /** @brief Shared-pointer alias for IWindow to simplify ownership management. */
    typedef std::shared_ptr<IWindow> IWindowPtr;
}
