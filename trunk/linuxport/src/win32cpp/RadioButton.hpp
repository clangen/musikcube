//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Casey Langen, André Wösten
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

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class RadioButton;

///\brief
///The type of event used when the CheckBox is pressed.
///\see
///RadioButton.
typedef sigslot::signal1<RadioButton*> RadioButtonPressedEvent;

///\brief
///A standard RadioButton.
///They are organised as linked list to enforce the grouping behaviour
class RadioButton : public Window
{
private: // types
    typedef Window base;

public: // events
    ///\brief This event is emitted when the user presses the RadioButton
    RadioButtonPressedEvent  Pressed;

public: // constructors
    /*ctor*/            RadioButton(const uichar* caption = _T(""), RadioButton* attach = NULL);
    /*dtor*/            ~RadioButton();

protected: // methods
    virtual HWND        Create(Window* parent);
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void        OnPressed();
    virtual void        PaintToHDC(HDC hdc, const Rect& rect);

public:
    void                Check(void);
    bool                IsChecked(void);
    
    RadioButton*        GetCheckedInGroup(void);

    // for testing
    uistring            Caption(void) const { return caption; }

protected: // instance data
    RadioButton* prev; // previous item, NULL if group begin
    RadioButton* next; // next item, NULL if group ends
    uistring caption;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
