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
#include "IDisplayable.h"
#include "IOrderable.h"
#include "IMessageTarget.h"

namespace cursespp {
    class IMessage;

    class IWindow : public IOrderable, public IDisplayable, public IMessageTarget {
        public:
            virtual ~IWindow() { }
            virtual void Repaint() = 0;
            virtual void SetParent(IWindow* parent) = 0;
            virtual void Show() = 0;
            virtual void Hide() = 0;
            virtual void Focus() = 0;
            virtual void Blur() = 0;
            virtual void SetContentColor(int64 color) = 0;
            virtual void SetFrameColor(int64 color) = 0;
            virtual void SetSize(int width, int height) = 0;
            virtual void SetPosition(int x, int y) = 0;
            virtual void MoveAndResize(int x, int y, int width, int height) = 0;
            virtual int GetWidth() const = 0;
            virtual int GetHeight() const = 0;
            virtual int GetContentHeight() const = 0;
            virtual int GetContentWidth() const = 0;
            virtual int GetX() const = 0;
            virtual int GetY() const = 0;
            virtual int GetId() const = 0;
            virtual int GetFocusOrder() = 0;
            virtual void SetFocusOrder(int order = -1) = 0;
            virtual WINDOW* GetFrame() const = 0;
            virtual WINDOW* GetContent() const = 0;
    };

    typedef std::shared_ptr<IWindow> IWindowPtr;
}
