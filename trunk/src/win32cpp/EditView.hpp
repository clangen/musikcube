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

#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class EditView; // forward decl

///\brief
///The type of event used when a the edit control is changed
///\see
///Button.
typedef sigslot::signal1<EditView*> EditViewChangedEvent;

///\brief
///A standard edit control.

class EditView: public Window
{
private:    // typedefs
    typedef Window base;

public:     // events
    EditViewChangedEvent    Changed;

public:     // constructors, methods
    /*ctor*/        EditView(int width, int height);
    /*ctor*/        EditView(const uistring& caption = _T(""));
    /*dtor*/        ~EditView();

    void    SetReadOnly(bool setting);
    void    ShowCaret();
    void    SetTooltip(uistring text);
    void    LimitText(int chars);
    void    SetSelection(int first, int second);
    void    SelectAll();

    void    Undo();

protected:  // methods
     virtual HWND       Create(Window* parent);
     virtual LRESULT    PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool &discardMessage);
     virtual LRESULT    WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
     virtual void       OnChanged();

protected:  // instance data
    uistring caption;
    int width;
    int height;
    uistring editText;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp