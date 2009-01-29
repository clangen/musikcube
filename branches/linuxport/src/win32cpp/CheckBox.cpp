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

#include <pch.hpp>
#include <win32cpp/CheckBox.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param caption
///The caption that beside the checkbox
/*ctor*/    CheckBox::CheckBox(const uichar* caption, LayoutFlags layoutFlags, int style)
: base(layoutFlags)
, caption(caption)
, style(style)
{
}

HWND        CheckBox::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | this->style | BS_NOTIFY;
    //
    HWND hwnd = CreateWindowEx(
        NULL,                   // ExStyle
        _T("BUTTON"),           // Class name
        this->caption.c_str(),  // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        64,                     // Width
        28,                     // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    return hwnd;
}

LRESULT     CheckBox::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
            this->state = (int)SendMessage(BM_GETCHECK, 0, 0);
            this->OnPressed(this->state);

            return 0;
        }
    }

    return this->DefaultWindowProc(message, wParam, lParam);
}

void        CheckBox::OnPressed(int state)
{
    this->Pressed(this, state);
}

void        CheckBox::PaintToHDC(HDC hdc, const Rect& rect)
{
    this->WindowProc(WM_PAINT, (WPARAM) (HDC) hdc, PRF_CLIENT);

    if (::GetFocus() == this->Handle())
    {
        RECT focusRect = rect;
        ::InflateRect(&focusRect, -4, -4);
        ::DrawFocusRect(hdc, &focusRect);
    }
}

bool        CheckBox::IsChecked(void) const
{
    return (this->state == BST_CHECKED);
}

bool        CheckBox::IsUnchecked(void) const
{
    return (this->state == BST_UNCHECKED);
}

bool        CheckBox::IsIndeterminate(void) const
{
    return (this->state == BST_INDETERMINATE);
}

void        CheckBox::Check(void)
{
    SendMessage(BM_SETCHECK, BST_CHECKED, 0);
    this->state = BST_CHECKED;
}

void        CheckBox::Uncheck(void)
{
    SendMessage(BM_SETCHECK, BST_UNCHECKED, 0);
    this->state = BST_UNCHECKED;
}

void        CheckBox::SetIndeterminate(void)
{
    SendMessage(BM_SETCHECK, BST_INDETERMINATE, 0);
    this->state = BST_INDETERMINATE;
}

