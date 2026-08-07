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
 * @file Panel.hpp
 * @brief Basic concrete Container implementation.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Panel is the most
 * basic concrete implementation of Container; it offers no special layout
 * behaviour and imposes no limit on the number of child controls.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Container.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief Panel is the most basic concrete implementation of Container.
 *  @details Panel does not offer any special layout functionality, and has
 *           no limitations as to the number of child controls that can be
 *           added. Panel is the base class for most more advanced Container
 *           implementations, including Splitter and BoxLayout.
 *  @see BoxLayout, Splitter */
class Panel: public Container
{
public: // types
    typedef Container base;

public: // constructors
    /** @brief Constructs an empty panel. */
    /*ctor*/    Panel();
    /** @brief Constructs a panel with layout flags.
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/    Panel(LayoutFlags layoutFlags);

protected: // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND        Create(Window* parent);
    /** @brief Fills the background with the panel's background color. */
    virtual void        OnEraseBackground(HDC hdc);
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
