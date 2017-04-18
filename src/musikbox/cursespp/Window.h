//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
            Window(IWindow* parent = NULL);
            virtual ~Window();

            virtual void SetParent(IWindow* parent);

            virtual void Show();
            virtual void Hide();
            virtual void Redraw();
            virtual void Invalidate();

            virtual void SetFrameVisible(bool visible);
            virtual bool IsFrameVisible();

            virtual void Focus();
            virtual void Blur();

            virtual void SetContentColor(musik_int64 color);
            virtual void SetFrameColor(musik_int64 color);
            virtual void SetFocusedContentColor(musik_int64 color);
            virtual void SetFocusedFrameColor(musik_int64 color);

            virtual musik_int64 GetContentColor() { return this->contentColor; }
            virtual musik_int64 GetFrameColor() { return this->frameColor; }
            virtual musik_int64 GetFocusedContentColor() { return this->focusedContentColor; }
            virtual musik_int64 GetFocusedFrameColor() { return this->focusedFrameColor; }

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

            virtual void OnParentVisibilityChanged(bool visible);

            bool HasBadBounds() { return this->badBounds; }

            static bool WriteToScreen(IInput* input);
            static void InvalidateScreen();
            static void Freeze();
            static void Unfreeze();

            static musik::core::runtime::IMessageQueue& MessageQueue();

        protected:
            IWindow* GetParent() const;

            void PostMessage(int messageType, musik_int64 user1 = 0, musik_int64 user2 = 0, musik_int64 delay = 0);
            void DebounceMessage(int messageType, musik_int64 user1 = 0, musik_int64 user2 = 0, musik_int64 delay = 0);
            void RemoveMessage(int messageType);

            void Create();
            void Destroy();
            void Recreate();
            void Clear();
            void RepaintBackground();
            void RecreateForUpdatedDimensions();

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
            bool isVisible, isFocused, isDirty;
            int focusOrder;
            int id;
            musik_int64 contentColor, frameColor;
            musik_int64 focusedContentColor, focusedFrameColor;
            int width, height, x, y;
            int lastAbsoluteX, lastAbsoluteY;
    };
}
