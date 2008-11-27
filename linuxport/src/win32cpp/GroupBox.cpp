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

#include <pch.hpp>
#include <win32cpp/GroupBox.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Default constructor.
/*ctor*/    GroupBox::GroupBox(const uichar* caption)
: base()
, caption(caption)
{
}

HWND        GroupBox::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    // create the window
    // (use WS_CLIPSIBLINGS to avoid unnecessary repainting of controls inside)
    DWORD style = WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_CLIPSIBLINGS;
    //
    HWND hwnd = CreateWindowEx(
        NULL,                   // ExStyle
        _T("BUTTON"),           // Class name
        this->caption.c_str(),  // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        100,                    // Width
        100,                    // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    return hwnd;
}

void        GroupBox::OnChildAdded(Window* newChild)
{
    SIZE idealSize;

    idealSize.cx = 0;
    idealSize.cy = 0;

    // all child elements should be resized to their ideal size
    if(newChild->SendMessageW(BCM_GETIDEALSIZE, 0, (LPARAM)(SIZE*)&idealSize)) {
        newChild->Resize(idealSize.cx, idealSize.cy);
    }

    // all child elements should be WITHIN the groupbox frame,
    // so we need to try to position them perfectly
    RECT childRect = newChild->ClientRect();
    newChild->MoveTo(
        childRect.left + GetSystemMetrics(SM_CXDLGFRAME) + GetSystemMetrics(SM_CXBORDER),
        childRect.top  + GetSystemMetrics(SM_CYDLGFRAME) + GetSystemMetrics(SM_CYBORDER) + GetSystemMetrics(SM_CYCAPTION)
    );
}

void        GroupBox::OnEraseBackground(HDC hdc)
{
    HBRUSH bgbrush;
    RECT bgrect;

    // repaint background (since its white due to its parent controls)
    bgbrush = ::CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    ::SelectObject(hdc, bgbrush);
    
    ::GetClientRect(this->windowHandle, &bgrect);
    ::FillRect(hdc, &bgrect, bgbrush);

    ::DeleteObject(bgbrush);
}
