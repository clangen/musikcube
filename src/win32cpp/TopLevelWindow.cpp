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
#include <win32cpp/Win32Config.hpp>
#include <win32cpp/TopLevelWindow.hpp>
#include <win32cpp/Types.hpp>
#include <win32cpp/MemoryDC.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/Color.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

#define CLASS_NAME _T("win32cpp TopLevelWindow")

///\brief Constructor.
///
///\param windowTitle
///The title that will be displayed in the title bar..
/*ctor*/    TopLevelWindow::TopLevelWindow(const uichar* windowTitle)
: base()
, windowTitle(windowTitle)
, minSize(0, 0)
, closed(false)
, modalChild(NULL)
, parentWindow(NULL)
{
}

/*dtor*/    TopLevelWindow::~TopLevelWindow()
{
}

bool        TopLevelWindow::RegisterWindowClass()
{
    static bool registered  = false;

    if ( ! registered)
    {
        WNDCLASSEX wc = { 0 };

        // register the window class
        wc.cbSize        = sizeof(WNDCLASSEX);
        wc.style         = 0;
        wc.lpfnWndProc   = Window::StaticWindowProc;
        wc.cbClsExtra    = 0;
        wc.cbWndExtra    = 0;
        wc.hInstance     = Application::Instance();
        wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH) (COLOR_WINDOW+1);
        wc.lpszMenuName  = NULL;
        wc.lpszClassName = CLASS_NAME;
        wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);

        registered = (::RegisterClassEx(&wc) != 0);
    }

    return registered;
}

HWND        TopLevelWindow::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();
    HWND parentHWND = (parent ? parent->Handle() : NULL);

    if ( ! TopLevelWindow::RegisterWindowClass())
    {
        return NULL;
    }

    // create the window
    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    DWORD styleEx = NULL;
    //
    HWND hwnd = ::CreateWindowEx(
        styleEx,                    // ExStyle
        CLASS_NAME,                 // Class name
        this->windowTitle.c_str(),  // Window name
        style,                      // Style
        CW_USEDEFAULT,              // X
        CW_USEDEFAULT,              // Y
        (this->minSize.width) ? this->minSize.width : 240,  // Width
        (this->minSize.height) ? this->minSize.height: 120, // Height
        parentHWND,                 // Parent
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

///\brief Closes the TopLevelWindow
void        TopLevelWindow::Close()
{
    this->SendMessage(WM_CLOSE, NULL, NULL);
}

LRESULT     TopLevelWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if(this == Application::Instance().MainWindow()) {
        Application::Instance().SysTrayManager()->WindowProc(this->Handle(), message, wParam, lParam);
    }

    switch (message)
    {
    case WM_SIZE:
        {
            Size size(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            this->OnResizedBase(size);
        }
        return 0;

    case WM_DESTROY:
        {
            // Destroy handler is used together with ShowModal
            // According to http://msdn.microsoft.com/en-us/library/ms646291.aspx
            // we need to do:
            // "A window must be enabled before it can be activated. For example, 
            //  if an application is displaying a modeless dialog box and has disabled 
            //  its main window, the application must enable the main window before 
            //  destroying the dialog box. Otherwise, another window will receive the 
            //  keyboard focus and be activated."
            if(this->parentWindow) {
                this->parentWindow->Enable(true);   
                SetForegroundWindow(this->parentWindow->Handle());
                BringWindowToTop(this->parentWindow->Handle());
                
                this->parentWindow = NULL;
            }
        }
        break;

    case WM_CLOSE:
        {
            this->closed = true;
        }
        break;

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

void        TopLevelWindow::ShowModal(TopLevelWindow* parent)
{
    if (this->closed)
    {
        throw WindowAlreadyClosedException();
    }

    if ( ! this->Handle())
    {
        this->Initialize(parent);
    }

    if (parent)
    {
        parent->modalChild = this;
        
        // Disable keyboard/mouse input on the parent window,
        // this also means it gets deactivated (NOT in foreground)
        parent->Enable(false);

        // Remember the parent window
        this->parentWindow = parent;
    }

    try
    {
        // center the child dialog over the parent
        if (parent)
        {
            Point location(0, 0);
            RECT parentRect;
            ::GetWindowRect(parent->windowHandle, &parentRect);

            Size thisSize = this->WindowSize();
            location.x = (parentRect.left + parentRect.right) / 2 - thisSize.width / 2;
            location.y = (parentRect.top + parentRect.bottom) / 2 - thisSize.height / 2;

            this->MoveTo(location);
        }

        // removes the dialog from the titlebar
        ::SetWindowLong(this->Handle(), GWL_STYLE, WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
        this->Show(SW_SHOW);

        bool shouldQuit = false;
        MSG msg;
        while (( ! shouldQuit) && (::GetMessage(&msg, NULL, 0, 0) > 0))
        {
            if (msg.message == WM_QUIT)
            {
                shouldQuit = true;
                ::PostQuitMessage(0);
                break;
            }

            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);

            shouldQuit |= this->closed;
        }
    }
    catch (Exception e)
    {
        // TODO: log me
    }

    if (parent) 
    {
        parent->modalChild = NULL;
    }

    if (this->parentWindow) {
        this->parentWindow = NULL;
    }
}

void                TopLevelWindow::OnGainedFocus()
{
    if (this->modalChild)
    {
        this->modalChild->SetFocus();
    }
}

TopLevelWindow*     TopLevelWindow::FindFromAncestor(Window* window)
{
    Window* topLevelWindow = window->Parent(), *scratch = NULL;
    while ((topLevelWindow) && (scratch = topLevelWindow->Parent()))
    {
        topLevelWindow = scratch;
    }

    return dynamic_cast<TopLevelWindow*>(topLevelWindow);
}
