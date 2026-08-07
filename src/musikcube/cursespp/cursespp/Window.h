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

/** @file Window.h @brief The concrete base window implementation backed by curses PANEL/WINDOW handles. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/IWindow.h>
#include <cursespp/INavigationKeys.h>
#include <musikcore/runtime/IMessageQueue.h>
#include <sigslot/sigslot.h>

#ifdef WIN32
    #define IDLE_TIMEOUT_MS 1
    #define REDRAW_DEBOUNCE_MS 100
#else
    #define IDLE_TIMEOUT_MS 75
    #define REDRAW_DEBOUNCE_MS 100
#endif

namespace cursespp {
    class IInput;

    /** @brief The default concrete implementation of the IWindow interface.
     *
     *  @details Window is the workhorse of cursespp. Each Window owns a pair of
     *  curses WINDOW handles -- one for the frame border and one for the content
     *  region -- and wraps them in ncurses PANELs so they can be stacked in
     *  z-order. It implements all of IWindow's contract and provides a set of
     *  protected virtual hooks (Create, Destroy, OnRedraw, OnDimensionsChanged,
     *  OnFocusChanged, etc.) that subclasses override to supply their drawing
     *  and behavior.
     *
     *  Drawing model: Window does not paint on every event. Callers invoke
     *  Invalidate() to mark the window dirty; the framework then repaints it on
     *  the next cycle, honoring the REDRAW_DEBOUNCE_MS window so rapid bursts of
     *  invalidations are coalesced. On Win32 the IDLE_TIMEOUT_MS is much shorter
     *  because the app is driven by a fast message pump.
     *
     *  Window also provides a static message queue and navigation-key registry
     *  shared by the whole toolkit, and a static "write to screen" input pump
     *  that drives the global redraw loop.
     */
    class Window : public IWindow, public std::enable_shared_from_this<IWindow> {
        public:
            /* @brief The first message id in the reserved internal range.
             *
             * This constant (and its companion kLastReservedMessageId) delimit a
             * range of message ids used internally by the toolkit. Exercise
             * caution if you create your own messages within this range. */
            static const int kFirstReservedMessageId;

            /* @brief The last message id in the reserved internal range. */
            static const int kLastReservedMessageId;

            /** @brief Emitted whenever a mouse event targets this window.
             *
             *  @note The payload is a raw pointer to a temporary IMouseHandler::Event.
             *  Do not retain it beyond the callback.
             */
            sigslot::signal2<Window*, const IMouseHandler::Event*> MouseEvent;

            /** @brief Creates a window.
             *  @param parent the initial parent, or nullptr for an orphan window.
             */
            Window(IWindow* parent = nullptr);

            virtual ~Window();

            /** @brief Windows may not be copied; each instance owns curses handles. */
            Window(const Window& other) = delete;

            /** @brief Windows may not be copy-assigned. */
            Window& operator=(const Window& other) = delete;

            /** @brief Feeds a single keystroke into the global input pipeline.
             *  @param input the input source providing the key string.
             *  @return true if the keystroke was accepted.
             */
            static bool WriteToScreen(IInput* input);

            /** @brief Marks the entire terminal as stale and schedules a global repaint. */
            static void InvalidateScreen();

            /** @brief Suspends the global redraw loop until Unfreeze() is called. */
            static void Freeze();

            /** @brief Resumes the global redraw loop previously suspended by Freeze(). */
            static void Unfreeze();

            /** @brief Installs the toolkit-wide navigation key translator.
             *  @param keys the shared INavigationKeys implementation to use.
             */
            static void SetNavigationKeys(std::shared_ptr<INavigationKeys> keys);

            /** @brief Returns the toolkit-wide message queue.
             *  @return a reference to the shared IMessageQueue.
             */
            static musik::core::runtime::IMessageQueue& MessageQueue();

            /* IWindow */

            /** @brief Sets the parent window and, if the parent is a group, registers this window with it.
             *  @param parent the new parent, or nullptr to detach.
             */
            void SetParent(IWindow* parent) override;

            /** @brief Makes the window visible and repaints it. */
            void Show() override;

            /** @brief Hides the window and erases it from the terminal. */
            void Hide() override;

            /** @brief Immediately repaints this window and its frame. */
            void Redraw() override;

            /** @brief Marks the window dirty so it is repainted on the next cycle. */
            void Invalidate() override;

            /** @brief Shows or hides the frame border.
             *  @param visible true to draw the border.
             */
            void SetFrameVisible(bool visible) override;

            /** @brief Returns whether the frame border is visible.
             *  @return true if the frame is drawn.
             */
            bool IsFrameVisible() override;

            /** @brief Gives focus to the window and triggers OnFocusChanged(true). */
            void Focus() override;

            /** @brief Removes focus from the window and triggers OnFocusChanged(false). */
            void Blur() override;

            /** @brief Sets the content color.
             *  @param color the curses color pair to use.
             */
            void SetContentColor(Color color) override;

            /** @brief Sets the frame color.
             *  @param color the curses color pair to use.
             */
            void SetFrameColor(Color color) override;

            /** @brief Sets the content color used while focused.
             *  @param color the curses color pair to use.
             */
            void SetFocusedContentColor(Color color) override;

            /** @brief Sets the frame color used while focused.
             *  @param color the curses color pair to use.
             */
            void SetFocusedFrameColor(Color color) override;

            /** @brief Returns the current content color.
             *  @return the content curses color pair.
             */
            Color GetContentColor() override { return this->contentColor; }

            /** @brief Returns the current frame color.
             *  @return the frame curses color pair.
             */
            Color GetFrameColor() override { return this->frameColor; }

            /** @brief Returns the focused content color.
             *  @return the focused content curses color pair.
             */
            Color GetFocusedContentColor() override { return this->focusedContentColor; }

            /** @brief Returns the focused frame color.
             *  @return the focused frame curses color pair.
             */
            Color GetFocusedFrameColor() override { return this->focusedFrameColor; }

            /** @brief Resizes the window's backing curses windows.
             *  @param width the new width in cells.
             *  @param height the new height in cells.
             */
            void SetSize(int width, int height) override;

            /** @brief Moves the window relative to its parent.
             *  @param x the new horizontal offset.
             *  @param y the new vertical offset.
             */
            void SetPosition(int x, int y) override;

            /** @brief Moves and resizes the window in one step.
             *  @param x the new horizontal offset.
             *  @param y the new vertical offset.
             *  @param width the new width.
             *  @param height the new height.
             */
            void MoveAndResize(int x, int y, int width, int height) override;

            /** @brief Returns the window width.
             *  @return the width in cells.
             */
            int GetWidth() const override;

            /** @brief Returns the window height.
             *  @return the height in cells.
             */
            int GetHeight() const override;

            /** @brief Returns the content-region height.
             *  @return the height inside the frame, in cells.
             */
            int GetContentHeight() const override;

            /** @brief Returns the content-region width.
             *  @return the width inside the frame, in cells.
             */
            int GetContentWidth() const override;

            /** @brief Returns the horizontal position relative to the parent.
             *  @return the relative x offset in cells.
             */
            int GetX() const override;

            /** @brief Returns the vertical position relative to the parent.
             *  @return the relative y offset in cells.
             */
            int GetY() const override;

            /** @brief Returns the screen-absolute horizontal position.
             *  @return the absolute x offset in cells.
             */
            int GetAbsoluteX() const override;

            /** @brief Returns the screen-absolute vertical position.
             *  @return the absolute y offset in cells.
             */
            int GetAbsoluteY() const override;

            /** @brief Returns the window's unique id.
             *  @return the id assigned when the window was created.
             */
            int GetId() const override;

            /** @brief Sets the title displayed on the frame border.
             *  @param title the title text.
             */
            void SetFrameTitle(const std::string& title) override;

            /** @brief Returns the frame title.
             *  @return the current title string.
             */
            std::string GetFrameTitle() const override;

            /** @brief Moves this window to the top of the z-order. */
            void BringToTop() override;

            /** @brief Moves this window to the bottom of the z-order. */
            void SendToBottom() override;

            /** @brief Handles messages dispatched to this window from the shared queue. */
            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            /** @brief Returns the curses handle used for the frame border.
             *  @return the frame WINDOW*.
             */
            WINDOW* GetFrame() const override;

            /** @brief Returns the curses handle used for the content region.
             *  @return the content WINDOW*.
             */
            WINDOW* GetContent() const override;

            /** @brief Returns the window's focus order.
             *  @return the focus order value.
             */
            int GetFocusOrder() override;

            /** @brief Sets the window's focus order.
             *  @param order the focus order, or -1 for a default value.
             */
            void SetFocusOrder(int order = -1) override;

            /** @brief Returns whether the window and its ancestors are visible.
             *  @return true if visible.
             */
            bool IsVisible() override;

            /** @brief Returns whether the window currently has focus.
             *  @return true if focused.
             */
            bool IsFocused() override;

            /** @brief Returns whether this window is on top of its sibling z-order.
             *  @return true if the window is the top-most.
             */
            bool IsTop() override;

            /** @brief Returns the parent window.
             *  @return the parent IWindow*, or nullptr.
             */
            IWindow* GetParent() const override;

            /** @brief Propagates an ancestor visibility change into this window.
             *  @param visible the new visible state.
             */
            void OnParentVisibilityChanged(bool visible) override;

            /** @brief Handles a visibility change of a child window.
             *  @param visible the child's new visible state.
             *  @param child the child whose visibility changed.
             */
            void OnChildVisibilityChanged(bool visible, IWindow* child) override;

            /** @brief Returns whether the backing curses windows have invalid geometry.
             *  @return true if the window currently reports bad bounds.
             */
            bool HasBadBounds() noexcept { return this->badBounds; }

            /* IMouseHandler */

            /** @brief Routes a mouse event to this window and its MouseEvent signal.
             *  @param mouseEvent the event to process.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& mouseEvent) override;

        protected:

            /** @brief Posts a message and blocks this thread until it is processed.
             *  @param messageType the message id.
             *  @param user1 optional first payload value.
             *  @param user2 optional second payload value.
             *  @param delay optional delay in milliseconds.
             */
            void Broadcast(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);

            /** @brief Enqueues a message for asynchronous delivery.
             *  @param messageType the message id.
             *  @param user1 optional first payload value.
             *  @param user2 optional second payload value.
             *  @param delay optional delay in milliseconds.
             */
            void Post(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);

            /** @brief Enqueues a message, replacing any identical pending message.
             *  @param messageType the message id.
             *  @param user1 optional first payload value.
             *  @param user2 optional second payload value.
             *  @param delay optional delay in milliseconds.
             */
            void Debounce(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);

            /** @brief Removes all pending messages of the given type.
             *  @param messageType the message id to purge.
             */
            void Remove(int messageType);

            /** @brief Attempts to acquire focus within the parent layout.
             *  @return true if the focus request succeeded.
             */
            bool FocusInParent();

            /** @brief Returns the toolkit-wide navigation key translator.
             *  @return a reference to the shared INavigationKeys.
             */
            static INavigationKeys& NavigationKeys();

            /** @brief Recreates the backing curses windows and panels (e.g. after a resize). */
            void Recreate();

            /** @brief Clears the content region of the window. */
            void Clear();

            /** @brief Draws the frame border and title onto the frame handle. */
            void DrawFrameAndTitle();

            /** @brief Repaints the background of the window with the active color scheme. */
            void RepaintBackground();

            /** @brief Recreates the curses handles when the window dimensions change. */
            void RecreateForUpdatedDimensions();

            /** @brief Destroys and recreates handles if the current bounds are invalid. */
            void DestroyIfBadBounds();

            /** @brief Returns whether the window's parent chain is visible.
             *  @return true if the parent is visible.
             */
            bool IsParentVisible();

            /** @brief Checks for terminal geometry errors and flags the window accordingly.
             *  @return true if a bounds error was detected.
             */
            bool CheckForBoundsError();

            /** @brief Hook invoked after the curses handles are (re)created. */
            virtual void Create();

            /** @brief Hook invoked just before the curses handles are destroyed. */
            virtual void Destroy();

            /** @brief Hook invoked when the frame is decorated (border and title drawn). */
            virtual void DecorateFrame();

            /** @brief Hook invoked when the window's dimensions change. */
            virtual void OnDimensionsChanged();

            /** @brief Hook invoked when the window's visible state changes.
             *  @param visible the new visible state.
             */
            virtual void OnVisibilityChanged(bool visible);

            /** @brief Hook invoked when the window's focus state changes.
             *  @param focused the new focus state.
             */
            virtual void OnFocusChanged(bool focused);

            /** @brief Hook invoked when the window needs to repaint its content. */
            virtual void OnRedraw();

            /** @brief Hook invoked after this window is attached to a new parent.
             *  @param newParent the parent this window was added to.
             */
            virtual void OnAddedToParent(IWindow* newParent);

            /** @brief Hook invoked after this window is detached from a parent.
             *  @param oldParent the parent this window was removed from.
             */
            virtual void OnRemovedFromParent(IWindow* oldParent);

            /** @brief Propagates a visibility change to children and re-lays-out as needed.
             *  @param becameVisibile true if the window just became visible.
             */
            void NotifyVisibilityChange(bool becameVisibile);

        private:
            IWindow* parent;                        /**< The parent window, or nullptr. */
            PANEL* framePanel;                      /**< Curses panel wrapping the frame window (z-ordering). */
            WINDOW* frame;                          /**< Curses handle for the frame border. */
            PANEL* contentPanel;                    /**< Curses panel wrapping the content window. */
            WINDOW* content;                        /**< Curses handle for the content region. */
            bool badBounds;                         /**< True if the terminal reported invalid geometry. */
            bool drawFrame;                         /**< Whether the frame border is drawn. */
            bool isVisibleInParent, isDirty;        /**< Visibility state and redraw dirty flag. */
            int focusOrder;                         /**< Focus order used by the parent layout. */
            int id;                                 /**< Unique window id. */
            Color contentColor, frameColor;         /**< Color pairs for content and frame. */
            Color focusedContentColor, focusedFrameColor; /**< Color pairs used while focused. */
            std::string title;                      /**< Title rendered on the frame border. */
            int width, height, x, y;                /**< Current geometry relative to the parent. */
            int lastAbsoluteX, lastAbsoluteY;       /**< Last known screen-absolute position. */
            bool lastNotifiedVisible{ false };      /**< Last visibility value propagated to children. */
    };
}
