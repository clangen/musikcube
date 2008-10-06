//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
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

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Container.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///A TopLevelWindow is a Window with a title bar, menu, and minimize, maximize,
///close boxes,
///
///TopLevelWindows can house an arbitrary number of child windows.
///
///\see
///Window, Container.
class TopLevelWindow: public Container
{
private: // types
    typedef Container base;
    class WindowAlreadyClosedException : public Exception { };

public: // ctor, dtor
    /*ctor*/    TopLevelWindow(const uichar* windowTitle);
    /*dtor*/    virtual ~TopLevelWindow();

public: // methods
    void    SetMinimumSize(const Size& minSize);
    Size    MinimumSize() const;
    void    ShowModal(TopLevelWindow* parent);
    void    Close();

    static TopLevelWindow* FindFromAncestor(Window* window);

protected: // methods
    virtual HWND        Create(Window* parent);
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

    virtual void        OnRequestFocusNext();
    virtual void        OnRequestFocusPrev();
    virtual void        OnEraseBackground(HDC hdc);
    virtual void        OnPaint();
    virtual void        OnGainedFocus();

    static bool RegisterWindowClass();

private: // instance data
    uistring windowTitle;
    bool closed;
    Size minSize;
    TopLevelWindow* modalChild;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
