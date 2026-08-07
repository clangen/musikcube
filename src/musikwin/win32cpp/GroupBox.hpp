//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Casey Langen, Andr� W�sten
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
 * @file GroupBox.hpp
 * @brief Captioned group box container.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. GroupBox wraps the
 * Win32 BUTTON control in BS_GROUPBOX mode to draw a titled frame and hosts
 * child controls inside it.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Container.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief A group box container with a title.
 *  @details Wraps the Win32 BUTTON control in BS_GROUPBOX mode, which draws
 *           a bordered frame with a caption. Child controls are added via
 *           the inherited Container interface and are clipped to the frame. */
class GroupBox: public Container
{
public: // types
    typedef Container base;

public: // constructors
    /** @brief Constructs a group box with the given title.
     *  @param caption the title text */
    /*ctor*/    GroupBox(const uichar* caption = _T(""));

protected: // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND    Create(Window* parent);
    /** @brief Fills the background of the group box. */
    virtual void    OnEraseBackground(HDC hdc);
    /** @brief Ensures children are clipped to the frame. */
    virtual void    OnChildAdded(Window* newChild);

protected: // instance data
    uistring caption; /**< the group box title */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
