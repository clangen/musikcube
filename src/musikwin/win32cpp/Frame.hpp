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
 * @file Frame.hpp
 * @brief Container that adds padding around a single child window.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Frame is a Panel
 * that adds a WindowPadding border around exactly one child Window. If the
 * child is resized the Frame resizes itself to accommodate it; if the Frame
 * is resized the child is resized to fit the new client area.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Panel.hpp>
#include <win32cpp/WindowPadding.hpp>
#include <win32cpp/Application.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief Frame is a Container that adds a padding (border) to a single child.
 *  @details If the child is resized, Frame will automatically resize itself
 *           to accommodate the child and respect the specified
 *           WindowPadding. If the Frame is resized, it will automatically
 *           resize the child window to fit its new ClientSize.
 *
 *           Attempting to add more than 1 child Window to a Frame will
 *           result in a TooManyChildWindowsException.
 *  @see Container, Panel */
class Frame: public Panel
{
private: //types
    typedef Panel base;

public: // constructors
    /** @brief Constructs a frame around the given child.
     *  @param child the child window (may be NULL)
     *  @param padding the padding in pixels on all sides */
    /*ctor*/    Frame(Window* child = NULL, int padding = 0);
    /** @brief Constructs a frame around the given child.
     *  @param child the child window (may be NULL)
     *  @param padding the per-side padding */
    /*ctor*/    Frame(Window* child, const WindowPadding& padding);

public: // methods
    /** @brief Sets the per-side padding.
     *  @param padding the new padding values */
    void SetPadding(const WindowPadding& padding);
    /** @brief Sets a uniform padding.
     *  @param padding the padding in pixels on all sides */
    void SetPadding(int padding);
    /** @brief Returns the client size minus the padding.
     *  @return the usable client size */
    virtual Size ClientSize() const;

protected: // methods
    /** @brief Resizes the frame to fit the child and padding. */
    void ResizeFromChild();
    /** @brief Resizes the frame when the child changes size.
     *  @param window the resized child
     *  @param newSize the child's new size */
    void OnChildResized(Window* window, Size newSize);

    virtual bool AddChildWindow(Window* window);
    virtual void OnChildAdded(Window* newChild);
    virtual void OnCreated();
    virtual void OnResized(const Size& newSize);
    virtual HWND Create(Window* parent);

    static bool RegisterWindowClass();

private: // instance data
    WindowPadding padding;  /**< space between frame and child */
    Window* child;          /**< the single child window */
    bool isResizingHACK;    /**< re-entrancy guard during resize */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
