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

#include "pch.hpp"
#include <win32cpp/Button.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param caption
///The caption that will be drawn on the button.
/*ctor*/    Button::Button(const uichar* caption)
: base()
, caption(caption)
{
}

HWND        Button::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_NOTIFY | BS_TEXT;
    //
    HWND hwnd = CreateWindowEx(
        NULL,                   // ExStyle
        _T("BUTTON"),           // Class name
        this->caption.c_str(),  // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        64,                     // Width
        32,                     // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    return hwnd;
}

LRESULT     Button::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
            this->OnPressed();
            return 0;
        }
    }

    return this->DefaultWindowProc(message, wParam, lParam);
}

void        Button::OnPressed()
{
    this->Pressed(this);
}

void        Button::PaintToHDC(HDC hdc, const Rect& rect)
{
    this->WindowProc(WM_PAINT, (WPARAM) (HDC) hdc, PRF_CLIENT);

    if (::GetFocus() == this->Handle())
    {
        RECT focusRect = rect;
        ::InflateRect(&focusRect, -4, -4);
        ::DrawFocusRect(hdc, &focusRect);
    }
}