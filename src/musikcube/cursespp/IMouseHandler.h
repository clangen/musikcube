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

#include <stdafx.h>
#include "curses_config.h"

namespace cursespp {
    class IWindow;

    class IMouseHandler {
        public:
            struct Event {
                Event(const Event& original, int childX, int childY);
                Event(const Event& original, IWindow* parent = nullptr);
                Event(const MEVENT& original, IWindow* parent = nullptr);

                bool Button1Clicked() const { return state & BUTTON1_CLICKED; }
                bool Button2Clicked() const { return state & BUTTON2_CLICKED; }
                bool Button3Clicked() const { return state & BUTTON3_CLICKED; }

                bool Button1DoubleClicked() const { return state & BUTTON1_DOUBLE_CLICKED; }
                bool Button2DoubleClicked() const { return state & BUTTON2_DOUBLE_CLICKED; }
                bool Button3DoubleClicked() const { return state & BUTTON3_DOUBLE_CLICKED; }

#ifdef WIN32
                bool MouseWheelUp() const { return MOUSE_WHEEL_UP; }
                bool MouseWheelDown() const { return MOUSE_WHEEL_DOWN; }
#else
                bool MouseWheelUp() const { return false; }
                bool MouseWheelDown() const { return false; }
#endif

                int x, y;
                mmask_t state;
            };

            virtual ~IMouseHandler() { }
            virtual bool MouseEvent(const Event& mouseEvent) = 0;
    };
}
