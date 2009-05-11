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
#include <win32cpp/VirtualWindow.hpp>
#include <win32cpp/Color.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

#define CLASS_NAME _T("VirtualWindow")

/*ctor*/    VirtualWindow::VirtualWindow(HWND handle, int padding, LayoutFlags flags)
: base()
, padding(padding, padding, padding, padding)
, realHWND(handle)
{
    this->SetLayoutFlags(flags);
    this->SetBackgroundColor(Color(255, 0, 255));
}

///\brief Set the Child's padding.
///
///\param padding
///The new padding parameters to use.
void        VirtualWindow::SetPadding(const WindowPadding& padding)
{
    this->padding = padding;
    this->OnResized(this->WindowSize());
}

///\brief Set the Child's padding.
///
///\param padding
///The amount of padding, in pixels, for all edges.
void        VirtualWindow::SetPadding(int padding)
{
    this->SetPadding(WindowPadding(padding, padding, padding, padding));
}

void        VirtualWindow::OnCreated()
{
    if (this->realHWND)
    {
        ::SetParent(this->realHWND, this->Handle());
    }
}

bool        VirtualWindow::AddChildWindow(Window* window)
{
    throw TooManyChildWindowsException();
}

void        VirtualWindow::OnResized(const Size& newSize)
{
    if (realHWND)
    {
        Size clientSize = this->ClientSize();

        ::SetWindowPos(
            this->realHWND,
            HWND_TOP,
            0, 0, 
            clientSize.width, clientSize.height,
            NULL);
    }
}

LRESULT     VirtualWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCCALCSIZE:
        {
            if (wParam && lParam)
            {
                NCCALCSIZE_PARAMS* params =
                    reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);

                (*params->rgrc).left += this->padding.left;
                (*params->rgrc).right -= this->padding.right;
                (*params->rgrc).top += this->padding.top;
                (*params->rgrc).bottom -= this->padding.bottom;
            }
        }
        return 0;

    case WM_NCPAINT:
        {
            HDC hdc = ::GetWindowDC(this->Handle());

            RECT windowRect = Rect(Point(0, 0), this->WindowSize());
            Point clientLoc(this->padding.left, this->padding.top);
            RECT clientRect = Rect(clientLoc, this->ClientSize());

            ::ExcludeClipRect(
                hdc,
                clientRect.left, clientRect.top,
                clientRect.right, clientRect.bottom);

            HBRUSH backBrush = ::CreateSolidBrush(Color::SystemColor(COLOR_BTNFACE));
            ::FillRect(hdc, &windowRect, backBrush);
            ::DeleteObject(backBrush);

            ReleaseDC(this->Handle(), hdc);
        }
        return 0;
    }

    return base::WindowProc(message, wParam, lParam);
}

HWND        VirtualWindow::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    if ( ! VirtualWindow::RegisterWindowClass())
    {
        return NULL;
    }

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    //
    HWND hwnd = CreateWindowEx(
        NULL,                   // ExStyle
        CLASS_NAME,             // Class name
        _T(""),                 // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        120,                    // Width
        36,                     // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    return hwnd;
}

bool        VirtualWindow::RegisterWindowClass()
{
    static bool registered  = false;

    if ( ! registered)
    {
        WNDCLASSEX wc = { 0 };

        // use STATIC window class as our base
        ::GetClassInfoEx(NULL, _T("STATIC"), &wc);
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpszClassName = CLASS_NAME;

        registered = (::RegisterClassEx(&wc) != 0);
    }

    return registered;
}