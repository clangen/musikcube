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
#include <win32cpp/Types.hpp>
#include <win32cpp/TopLevelWindow.hpp>
#include <win32cpp/MemoryDC.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief Constructor.
///
///\param windowTitle
///The title that will be displayed in the title bar..
/*ctor*/    TopLevelWindow::TopLevelWindow(const uichar* windowTitle)
: base()
, className(uistring(windowTitle) + _T(" TopLevelWindow"))
, windowTitle(windowTitle)
, minSize(0, 0)
{
}

HWND        TopLevelWindow::Create(Window* parent)
{
    // parent is ignored here! top level windows don't have parents!

    HINSTANCE hInstance = Application::Instance();
    WNDCLASSEX wc;

    // register the window class
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = 0;
    wc.lpfnWndProc   = Window::StaticWindowProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInstance;
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) (COLOR_WINDOW+1);
    wc.lpszMenuName  = NULL;
    wc.lpszClassName = this->className.c_str();
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

    if( ! ::RegisterClassEx(&wc))
    {
        return NULL;
    }

    // create the window
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    DWORD styleEx = NULL;
    //
    HWND hwnd = ::CreateWindowEx(
        styleEx,                    // ExStyle
        this->className.c_str(),    // Class name
        this->windowTitle.c_str(),  // Window name
        style,                      // Style
        CW_USEDEFAULT,              // X
        CW_USEDEFAULT,              // Y
        240,                        // Width
        120,                        // Height
        NULL,                       // Parent
        NULL,                       // Menu
        hInstance,                  // Instance
        NULL);                      // lParam

    return hwnd;
}

///\brief Sets the minimum size of the window.
///\param size The minimium size.
void        TopLevelWindow::SetMinimumSize(const Size& size)
{
    this->minSize = size;

    Size currentSize = this->WindowSize();
    if (currentSize.width < size.width) currentSize.width = size.width;
    if (currentSize.height < size.height) currentSize.height = size.height;

    this->Resize(currentSize);
}

///\brief Returns the minimum size of the window.
Size        TopLevelWindow::MinimumSize() const
{
    return this->minSize;
}

LRESULT     TopLevelWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        {
            Size size(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            this->OnResizedBase(size);
        }
        return 0;

    case WM_PRINTCLIENT:
        {
            HDC hdc = (HDC) wParam;
            RECT rcClient = this->ClientRect();

            HBRUSH bgBrush = ::CreateSolidBrush(this->BackgroundColor());
            ::FillRect(hdc, &rcClient, bgBrush);
            ::DeleteObject(bgBrush);
        }
        return 0;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
            minMaxInfo->ptMinTrackSize.x = this->minSize.width;
            minMaxInfo->ptMinTrackSize.y = this->minSize.height;
        }
        return 0;
    }

    return base::WindowProc(message, wParam, lParam);
}

void        TopLevelWindow::OnRequestFocusNext()
{
    bool focusChildSuccess = false;

    // if we're on the last focused item, loop back around to the first.
    if ((this->focusedWindow != NULL)
    &&  (this->focusedWindow == this->childWindows.back()))
    {
        focusChildSuccess = this->FocusFirstChild();
    }

    // we weren't able to focus the next child, so notify the parent.
    if ( ! focusChildSuccess)
    {
        base::OnRequestFocusNext();
    }
}

void        TopLevelWindow::OnRequestFocusPrev()
{   
    bool focusChildSuccess = false;

    // if we're on the first focused item, jump to the last.
    if ((this->focusedWindow != NULL)
    &&  (this->focusedWindow == this->childWindows.front()))
    {
        focusChildSuccess = this->FocusLastChild();
    }

    // we weren't able to focus the next child, so notify the parent.
    if ( ! focusChildSuccess)
    {
        base::OnRequestFocusPrev();
    }
}

void        TopLevelWindow::OnEraseBackground(HDC hdc)
{
    // do nothing.
}

void        TopLevelWindow::OnPaint()
{
    PAINTSTRUCT paintStruct;
    RECT clientRect = this->ClientRect();
    HDC hdc = ::BeginPaint(this->Handle(), &paintStruct);
    //
    {
        MemoryDC memDC(hdc, clientRect);

        HBRUSH backBrush = ::CreateSolidBrush(this->BackgroundColor());
        ::FillRect(memDC, &clientRect, backBrush);
        DeleteObject(backBrush);

        this->DefaultWindowProc(WM_PAINT, (WPARAM) (HDC) memDC, NULL);
    }
    //
    ::EndPaint(this->Handle(), &paintStruct);
}