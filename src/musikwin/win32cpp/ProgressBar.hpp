//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
//
// Sources and Binaries of: win32cpp
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
 * @file ProgressBar.hpp
 * @brief Progress bar control.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. ProgressBar wraps
 * the Win32 common controls progress bar (msctls_progress32) and supports
 * standard, smooth, vertical and marquee styles, plus range/position
 * management.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class ProgressBar; // forward decl

/** @brief A progress bar.
 *  @details Wraps the Win32 progress bar control. Supports the standard,
 *           smooth and vertical styles, as well as an animated marquee
 *           mode, and lets the caller set the range, position and step
 *           increment. */
class ProgressBar: public Window
{
private: // types
    typedef Window base;

public:     // constructors, methods
    /** @brief Constructs a progress bar with a fixed size.
     *  @param width the initial width in pixels
     *  @param height the initial height in pixels */
    /*ctor*/        ProgressBar(int width, int height);
    /** @brief Destroys the progress bar. */
    /*dtor*/        ~ProgressBar();

    /** @brief Switches the bar to the animated marquee style. */
    void SetMarqueeStyle();
    /** @brief Switches the bar to the smooth (solid fill) style. */
    void SetSmoothStyle();
    /** @brief Switches the bar to the vertical style. */
    void SetVerticalStyle();
    /** @brief Starts or stops the marquee animation.
     *  @param set true to start, false to stop
     *  @param delay the animation interval in milliseconds */
    void StartMarquee(bool set, unsigned int delay);
    /** @brief Sets the range of the bar.
     *  @param min the minimum value
     *  @param max the maximum value */
    void SetRange(unsigned int min, unsigned int max);
    /** @brief Sets the current position of the bar.
     *  @param pos the position value */
    void SetPos(int pos);
    /** @brief Sets the step increment used by Step().
     *  @param inc the step amount */
    void SetStepIncrement(int inc);
    /** @brief Advances the bar by the current step increment. */
    void Step();

protected:  // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND       Create(Window* parent);
    /** @brief Processes window messages. */
    virtual LRESULT    WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected:  // instance data
    uistring caption;  /**< reserved caption text */
    int width;         /**< initial width in pixels */
    int height;        /**< initial height in pixels */
    DWORD styleEx;     /**< extended style flags */
};

} //win32cpp
