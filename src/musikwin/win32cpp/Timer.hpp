//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Daniel �nnerby
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

/**
 * @file Timer.hpp
 * @brief Repeating timer bound to a window's message loop.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Timer wraps the
 * Win32 SetTimer/KillTimer APIs; it fires the OnTimeout sigslot signal on
 * the UI thread whenever the configured timeout elapses.
 */

#pragma once

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>
#include <sigslot/sigslot.h>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief A repeating timer that fires on the main thread.
 *  @details Uses the Win32 SetTimer API on an associated window handle so
 *           the timeout callback runs on the UI thread. The OnTimeout
 *           signal is emitted each time the interval elapses.
 *  @note The timer must be connected to a Window before Start() is called. */
class Timer : public EventHandler
{
public: // types
    /** @brief Signal fired each time the timer interval elapses. */
    typedef sigslot::signal0<> TimeoutEvent;

public: // events
    /** @brief Emitted each time the timer times out. */
    TimeoutEvent OnTimeout;

public: // ctor, dtor
    /** @brief Constructs a timer with the given interval.
     *  @param timeout the interval in milliseconds */
    Timer(unsigned timeout);
    /** @brief Stops and destroys the timer. */
    ~Timer();

public: // methods
    /** @brief Associates the timer with the given window.
     *  @param window the window that owns the message loop */
    void ConnectToWindow(Window *window);
    /** @brief Starts the timer.
     *  @return true if the timer was started successfully */
    bool Start();
    /** @brief Stops the timer.
     *  @return true if the timer was stopped successfully */
    bool Stop();

private: // methods
    /** @brief Internal handler invoked when the Win32 timer fires. */
    void OnTimerTimeout(unsigned int timeoutId);

private: // instance data
    unsigned timerId; /**< Win32 timer identifier */
    unsigned timeout; /**< interval in milliseconds */
    HWND wnd;         /**< window owning the message loop */
};

//////////////////////////////////////////////////////////////////////////////

}   // namespace

//////////////////////////////////////////////////////////////////////////////

