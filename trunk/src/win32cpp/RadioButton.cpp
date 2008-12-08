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
#include <win32cpp/RadioButton.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param caption
///The caption beside the radio button
/*ctor*/    RadioButton::RadioButton(const uichar* caption, RadioButton* prev, LayoutFlags layoutFlags)
: base(layoutFlags)
, caption(caption)
, prev(prev)
, next(NULL)
{
}

///\brief
///Constructor.
///
///\param caption
///The caption beside the radio button
/*ctor*/    RadioButton::RadioButton(const uichar* caption, LayoutFlags layoutFlags)
: base(layoutFlags)
, caption(caption)
, prev(NULL)
, next(NULL)
{
}


/*dtor*/    RadioButton::~RadioButton()
{
    // fix linked list after deletion of an item
    if(this->prev) {
        if(this->next) {
            this->prev->next = this->next;
        } else {
            this->prev->next = NULL;
        }
    }

    if(this->next) {
        if(this->prev) {
            this->next->prev = this->prev;
        } else {
            this->next->prev = NULL;
        }
    }
}

HWND        RadioButton::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | BS_NOTIFY | BS_AUTORADIOBUTTON;
    if(this->prev == NULL) {
        style |= WS_GROUP;
    } else {
        this->prev->next = this;
    }
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

LRESULT     RadioButton::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
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

void        RadioButton::OnPressed()
{
    this->Pressed(this);
}

void        RadioButton::PaintToHDC(HDC hdc, const Rect& rect)
{
    this->WindowProc(WM_PAINT, (WPARAM) (HDC) hdc, PRF_CLIENT);

    if (::GetFocus() == this->Handle())
    {
        RECT focusRect = rect;
        ::InflateRect(&focusRect, -4, -4);
        ::DrawFocusRect(hdc, &focusRect);
    }
}

RadioButton*    RadioButton::GetCheckedInGroup(void)
{
    RadioButton* traverser = const_cast<RadioButton*>(this);

    // check this item first
    if(traverser->IsChecked()) {
        return traverser;
    }

    // traverse to first item on list
    while(traverser->prev != NULL) {
        traverser = traverser->prev;
    }

    // traverse through whole list to find active icon
    while(traverser != NULL) {
        if(traverser->SendMessage(BM_GETCHECK, 0, 0) == BST_CHECKED) {
            return traverser;
        }
        traverser = traverser->next;
    }

    return NULL;
}

bool        RadioButton::IsChecked(void)
{
    return (this->SendMessage(BM_GETCHECK, 0, 0) == BST_CHECKED);
}

void        RadioButton::Check(void)
{
    // we need to emulate a click. otherwise the other radio buttons don't get unchecked :-)
    SendMessage(BM_CLICK, 0, 0);
}