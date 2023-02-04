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

    class Window : public IWindow, public std::enable_shared_from_this<IWindow> {
        public:
            /* range of message ids reserved for internal use. exercise caution if
            creating your own messages within this range. */
            static const int kFirstReservedMessageId;
            static const int kLastReservedMessageId;

            sigslot::signal2<Window*, const IMouseHandler::Event*> MouseEvent;

            Window(IWindow* parent = nullptr);
            virtual ~Window();

            Window(const Window& other) = delete;
            Window& operator=(const Window& other) = delete;

            static bool WriteToScreen(IInput* input);
            static void InvalidateScreen();
            static void Freeze();
            static void Unfreeze();
            static void SetNavigationKeys(std::shared_ptr<INavigationKeys> keys);
            static musik::core::runtime::IMessageQueue& MessageQueue();

            /* IWindow */
            void SetParent(IWindow* parent) override;

            void Show() override;
            void Hide() override;
            void Redraw() override;
            void Invalidate() override;

            void SetFrameVisible(bool visible) override;
            bool IsFrameVisible() override;

            void Focus() override;
            void Blur() override;

            void SetContentColor(Color color) override;
            void SetFrameColor(Color color) override;
            void SetFocusedContentColor(Color color) override;
            void SetFocusedFrameColor(Color color) override;

            Color GetContentColor() override { return this->contentColor; }
            Color GetFrameColor() override { return this->frameColor; }
            Color GetFocusedContentColor() override { return this->focusedContentColor; }
            Color GetFocusedFrameColor() override { return this->focusedFrameColor; }

            void SetSize(int width, int height) override;
            void SetPosition(int x, int y) override;
            void MoveAndResize(int x, int y, int width, int height) override;

            int GetWidth() const override;
            int GetHeight() const override;
            int GetContentHeight() const override;
            int GetContentWidth() const override;
            int GetX() const override;
            int GetY() const override;
            int GetAbsoluteX() const override;
            int GetAbsoluteY() const override;
            int GetId() const override;

            void SetFrameTitle(const std::string& title) override;
            std::string GetFrameTitle() const override;

            void BringToTop() override;
            void SendToBottom() override;

            void ProcessMessage(musik::core::runtime::IMessage &message) override;

            WINDOW* GetFrame() const override;
            WINDOW* GetContent() const override;

            int GetFocusOrder() override;
            void SetFocusOrder(int order = -1) override;

            bool IsVisible() override;
            bool IsFocused() override;
            bool IsTop() override;

            IWindow* GetParent() const override;
            void OnParentVisibilityChanged(bool visible) override;
            void OnChildVisibilityChanged(bool visible, IWindow* child) override;

            bool HasBadBounds() noexcept { return this->badBounds; }

            /* IMouseHandler */
            bool ProcessMouseEvent(const IMouseHandler::Event& mouseEvent) override;

        protected:

            void Broadcast(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);
            void Post(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);
            void Debounce(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);
            void Remove(int messageType);
            bool FocusInParent();

            static INavigationKeys& NavigationKeys();

            void Recreate();
            void Clear();
            void DrawFrameAndTitle();
            void RepaintBackground();
            void RecreateForUpdatedDimensions();
            void DestroyIfBadBounds();
            bool IsParentVisible();

            bool CheckForBoundsError();

            virtual void Create();
            virtual void Destroy();
            virtual void DecorateFrame();
            virtual void OnDimensionsChanged();
            virtual void OnVisibilityChanged(bool visible);
            virtual void OnFocusChanged(bool focused);
            virtual void OnRedraw();
            virtual void OnAddedToParent(IWindow* newParent);
            virtual void OnRemovedFromParent(IWindow* oldParent);

            void NotifyVisibilityChange(bool becameVisibile);

        private:
            IWindow* parent;
            PANEL* framePanel;
            WINDOW* frame;
            PANEL* contentPanel;
            WINDOW* content;
            bool badBounds;
            bool drawFrame;
            bool isVisibleInParent, isDirty;
            int focusOrder;
            int id;
            Color contentColor, frameColor;
            Color focusedContentColor, focusedFrameColor;
            std::string title;
            int width, height, x, y;
            int lastAbsoluteX, lastAbsoluteY;
            bool lastNotifiedVisible{ false };
    };
}
