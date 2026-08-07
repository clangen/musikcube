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
 * @file TopLevelWindow.hpp
 * @brief Top-level application window with a frame.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. TopLevelWindow is
 * a Container that is hosted directly by the operating system as a frame
 * window with a title bar, menu and minimize/maximize/close boxes. It can
 * house an arbitrary number of child windows.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Container.hpp>
#include <win32cpp/TrayIconManager.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief A Window with a title bar, menu, and minimize/maximize/close boxes.
 *  @details TopLevelWindow is the base class for top-level frames. It can
 *           house an arbitrary number of child windows, enforces a minimum
 *           size, and supports modal display and closing.
 *  @see Window, Container */
class TopLevelWindow: public Container
{
private: // types
    typedef Container base;
    class WindowAlreadyClosedException : public Exception { }; /**< thrown when operating on a closed window */

public: // ctor, dtor
    /** @brief Constructs a top-level window with the given title.
     *  @param windowTitle the text shown in the title bar */
    /*ctor*/    TopLevelWindow(const uichar* windowTitle);
    /** @brief Destroys the window. */
    /*dtor*/    virtual ~TopLevelWindow();

public: // methods
    /** @brief Sets the minimum size of the window.
     *  @param minSize the minimum width and height */
    void    SetMinimumSize(const Size& minSize);
    /** @brief Returns the current minimum size.
     *  @return the minimum size */
    Size    MinimumSize() const;
    /** @brief Shows the window as a modal dialog.
     *  @param parent the parent window that disables during the modal loop */
    void    ShowModal(TopLevelWindow* parent);
    /** @brief Closes the window. */
    void    Close();

    /** @brief Finds the top-level window ancestor of the given window.
     *  @param window the window to search from
     *  @return the top-level ancestor, or NULL */
    static TopLevelWindow* FindFromAncestor(Window* window);

protected: // methods
    /** @brief Creates the underlying HWND. */
    virtual HWND        Create(Window* parent);
    /** @brief Processes window messages. */
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

    virtual void        OnRequestFocusNext();
    virtual void        OnRequestFocusPrev();
    /** @brief Fills the background of the window. */
    virtual void        OnEraseBackground(HDC hdc);
    /** @brief Paints the window. */
    virtual void        OnPaint();
    /** @brief Handles focus changes. */
    virtual void        OnGainedFocus();

    static bool         RegisterWindowClass();

private: // instance data
    uistring windowTitle;    /**< title bar text */
    bool closed;             /**< whether the window has been closed */
    Size minSize;            /**< minimum window size */
    TopLevelWindow* modalChild; /**< child shown modally */
    Window* parentWindow;    /**< the window's parent */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
