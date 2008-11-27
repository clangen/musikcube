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
#include <win32cpp/Label.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief Constructor.
///\param caption The caption to draw.
/*ctor*/    Label::Label(const uichar* caption)
: base()
, caption(caption)
, autoResize(true)
{
}

HWND        Label::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance().Instance();

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | SS_NOTIFY | SS_ENDELLIPSIS;
    //
    HWND hwnd = CreateWindowEx(
        NULL,                   // ExStyle
        _T("STATIC"),           // Class name
        this->caption.c_str(),  // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        120,                    // Width
        36,                     // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    if (hwnd)
    {
        this->tabStop = false;
        this->windowHandle = hwnd;
        this->ResizeFromCaption();
    }

    return hwnd;
}

LRESULT     Label::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SETTEXT:
        {
            LRESULT result = base::WindowProc(message, wParam, lParam);
            if (result)
            {
                this->ResizeFromCaption();
            }
            return result;
        }
        break;
    }

    return base::WindowProc(message, wParam, lParam);
}

void        Label::OnFontChanged()
{
    this->ResizeFromCaption();
}

void        Label::OnCaptionChanged()
{
    if (this->autoResize)
    {
        this->ResizeFromCaption();
    }
}

///\brief Resizes the Label to the size of the caption.
///
///ResizeFromCaption will generate a Resized event.
void        Label::ResizeFromCaption()
{
    static uichar buffer[4096];
    if (::GetWindowText(this->Handle(), buffer, 4096))
    {
        HFONT font = this->Font()->GetHFONT();
        if (font)
        {
            HDC hdc = ::GetDC(this->Handle());
            HGDIOBJ oldObj = ::SelectObject(hdc, font);
            SIZE textExtent;
            int length = (int) wcslen(buffer);
            if (::GetTextExtentPoint32(hdc, buffer, length, &textExtent))
            {
                this->Resize(Size(textExtent));
            }
            ::SelectObject(hdc, oldObj);
            ::ReleaseDC(this->Handle(), hdc);
        }
    }
}

///\brief If enabled the Label will automatically resize itself
///when the caption is changed.
void        Label::EnableAutoResizeFromCaption(bool enable)
{
    this->autoResize = enable;

    if (this->autoResize)
    {
        this->ResizeFromCaption();
    }
}

///\brief Returns whether or not the label will be automatically resized
///when the caption is changed.
bool        Label::AutoResizeFromCaptionEnabled() const
{
    return this->autoResize;
}
