//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "curses_config.h"
#include "IWindow.h"
#include "INavigationKeys.h"
#include <core/runtime/IMessageQueue.h>

#ifdef WIN32
#define IDLE_TIMEOUT_MS 10
#define REDRAW_DEBOUNCE_MS 100
#else
#define IDLE_TIMEOUT_MS 75
#define REDRAW_DEBOUNCE_MS 100
#endif

namespace cursespp {
    class IInput;

    class Window : public IWindow, public std::enable_shared_from_this<IWindow> {
        public:
            Window(IWindow* parent = nullptr);
            virtual ~Window();

            Window(const Window& other) = delete;
            Window& operator=(const Window& other) = delete;

            virtual void SetParent(IWindow* parent);

            virtual void Show();
            virtual void Hide();
            virtual void Redraw();
            virtual void Invalidate();

            virtual void SetFrameVisible(bool visible);
            virtual bool IsFrameVisible();

            virtual void Focus();
            virtual void Blur();

            virtual void SetContentColor(int64_t color);
            virtual void SetFrameColor(int64_t color);
            virtual void SetFocusedContentColor(int64_t color);
            virtual void SetFocusedFrameColor(int64_t color);

            virtual int64_t GetContentColor() { return this->contentColor; }
            virtual int64_t GetFrameColor() { return this->frameColor; }
            virtual int64_t GetFocusedContentColor() { return this->focusedContentColor; }
            virtual int64_t GetFocusedFrameColor() { return this->focusedFrameColor; }

            virtual void SetSize(int width, int height);
            virtual void SetPosition(int x, int y);
            virtual void MoveAndResize(int x, int y, int width, int height);

            virtual int GetWidth() const;
            virtual int GetHeight() const;
            virtual int GetContentHeight() const;
            virtual int GetContentWidth() const;
            virtual int GetX() const;
            virtual int GetY() const;
            virtual int GetAbsoluteX() const;
            virtual int GetAbsoluteY() const;
            virtual int GetId() const;

            virtual void SetFrameTitle(const std::string& title);
            virtual std::string GetFrameTitle() const;

            virtual void BringToTop();
            virtual void SendToBottom();

            virtual void ProcessMessage(musik::core::runtime::IMessage &message);

            virtual WINDOW* GetFrame() const;
            virtual WINDOW* GetContent() const;

            virtual int GetFocusOrder();
            virtual void SetFocusOrder(int order = -1);

            virtual bool IsVisible();
            virtual bool IsFocused();
            virtual bool IsTop();

            virtual IWindow* GetParent() const;
            virtual void OnParentVisibilityChanged(bool visible);
            virtual void OnChildVisibilityChanged(bool visible, IWindow* child);

            bool HasBadBounds() { return this->badBounds; }

            bool MouseEvent(const IMouseHandler::Event& mouseEvent);

            static bool WriteToScreen(IInput* input);
            static void InvalidateScreen();
            static void Freeze();
            static void Unfreeze();

            static void SetNavigationKeys(std::shared_ptr<INavigationKeys> keys);

            static musik::core::runtime::IMessageQueue& MessageQueue();

        protected:

            void BroadcastMessage(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);
            void PostMessage(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);
            void DebounceMessage(int messageType, int64_t user1 = 0, int64_t user2 = 0, int64_t delay = 0);
            void RemoveMessage(int messageType);
            bool FocusInParent();

            static INavigationKeys& NavigationKeys();

            virtual void Create();
            virtual void Destroy();
            virtual void DecorateFrame();

            void Recreate();
            void Clear();
            void DrawFrameAndTitle();
            void RepaintBackground();
            void RecreateForUpdatedDimensions();
            void DestroyIfBadBounds();
            bool IsParentVisible();

            bool CheckForBoundsError();

            virtual void OnDimensionsChanged();
            virtual void OnVisibilityChanged(bool visible);
            virtual void OnFocusChanged(bool focused);
            virtual void OnRedraw();
            virtual void OnAddedToParent(IWindow* newParent);
            virtual void OnRemovedFromParent(IWindow* oldParent);

        private:
            IWindow* parent;
            PANEL* framePanel;
            WINDOW* frame;
            PANEL* contentPanel;
            WINDOW* content;
            bool badBounds;
            bool drawFrame;
            bool isVisibleInParent, isFocused, isDirty;
            int focusOrder;
            int id;
            int64_t contentColor, frameColor;
            int64_t focusedContentColor, focusedFrameColor;
            std::string title;
            int width, height, x, y;
            int lastAbsoluteX, lastAbsoluteY;
    };
}
