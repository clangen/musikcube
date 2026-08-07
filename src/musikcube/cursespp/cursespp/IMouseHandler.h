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

/** @file IMouseHandler.h @brief Interface for objects that process mouse events. */
#pragma once

#include <cursespp/curses_config.h>

namespace cursespp {
    class IWindow;

    /** @brief Contract for objects that can process mouse input.
     *
     *  @details In order to receive mouse events on a curses terminal, the
     *  underlying ncurses/PDCurses mouse mask must be enabled (typically
     *  ALL_MOUSE_EVENTS) by the application. When a raw mouse event arrives it
     *  is wrapped into an IMouseHandler::Event and dispatched to the window
     *  under the cursor; the window returns true if it consumed the event.
     */
    class IMouseHandler {
        public:
            /** @brief A single mouse event translated from a curses MEVENT.
             *
             *  @details The Event aggregates the cell coordinates and the raw
             *  curses mouse state mask. It provides convenience predicates for
             *  the common click/double-click gestures and, on supported
             *  platforms, wheel scroll detection. The extra constructors allow
             *  an event to be re-homed relative to a child window's coordinate
             *  space so that children can process events that originally
             *  targeted their parent.
             */
            struct Event {
                /** @brief Re-homes the event coordinates relative to a point in the parent's space. */
                Event(const Event& original, int childX, int childY);

                /** @brief Re-homes the event coordinates relative to the given parent window. */
                Event(const Event& original, IWindow* parent = nullptr);

                /** @brief Wraps a raw curses MEVENT, optionally relative to a parent window. */
                Event(const MEVENT& original, IWindow* parent = nullptr);

                /** @brief Returns true if the left mouse button was clicked. */
                bool Button1Clicked() const { return state & BUTTON1_CLICKED; }

                /** @brief Returns true if the middle mouse button was clicked. */
                bool Button2Clicked() const { return state & BUTTON2_CLICKED; }

                /** @brief Returns true if the right mouse button was clicked. */
                bool Button3Clicked() const { return state & BUTTON3_CLICKED; }

                /** @brief Returns true if the left mouse button was double-clicked. */
                bool Button1DoubleClicked() const { return state & BUTTON1_DOUBLE_CLICKED; }

                /** @brief Returns true if the middle mouse button was double-clicked. */
                bool Button2DoubleClicked() const { return state & BUTTON2_DOUBLE_CLICKED; }

                /** @brief Returns true if the right mouse button was double-clicked. */
                bool Button3DoubleClicked() const { return state & BUTTON3_DOUBLE_CLICKED; }

                /** @brief Returns true if the mouse wheel was scrolled up. */
#ifdef WIN32
                bool MouseWheelUp() const { return MOUSE_WHEEL_UP; }
#else
    #if NCURSES_MOUSE_VERSION > 1
                bool MouseWheelUp() const { return state & BUTTON4_PRESSED; }
    #else
                bool MouseWheelUp() const { return state & (BUTTON4_PRESSED | REPORT_MOUSE_POSITION); }
    #endif
#endif

                /** @brief Returns true if the mouse wheel was scrolled down. */
#ifdef WIN32
                bool MouseWheelDown() const { return MOUSE_WHEEL_DOWN; }
#else
    #if NCURSES_MOUSE_VERSION > 1
                bool MouseWheelDown() const { return state & BUTTON5_PRESSED; }
    #else
                bool MouseWheelDown() const { return state & (BUTTON2_PRESSED | REPORT_MOUSE_POSITION); }
    #endif
#endif

                /** @brief The horizontal position of the event in terminal cells. */
                int x;

                /** @brief The vertical position of the event in terminal cells. */
                int y;

                /** @brief The raw curses mouse mask describing the event. */
                mmask_t state;
            };

            virtual ~IMouseHandler() { }

            /** @brief Invoked to deliver a mouse event to this handler.
             *  @param mouseEvent the translated mouse event.
             *  @return true if the event was consumed and should not propagate.
             */
            virtual bool ProcessMouseEvent(const Event& mouseEvent) = 0;
    };
}
