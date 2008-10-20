//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright  2007, Casey Langen
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
#include <win32cpp/TabView.hpp>
#include <win32cpp/MemoryDC.hpp>
#include <win32cpp/Application.hpp>
#include <win32cpp/Color.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Default constructor.
/*ctor*/    TabView::TabView()
: base()
, visibleChild(NULL)
{
}

HWND        TabView::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    DWORD styleEx = NULL;
    //
    HWND hwnd = CreateWindowEx(
        styleEx,                // ExStyle
        WC_TABCONTROL,          // Class name
        _T(""),                 // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        120,                    // Width
        100,                     // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    return hwnd;
}

void        TabView::OnPaint()
{
    PAINTSTRUCT paintStruct;
    RECT clientRect = this->ClientRect();   // using paintStruct.rcPaint gives redraw artifacts!
    HDC hdc = ::BeginPaint(this->Handle(), &paintStruct);
    //
    {
        MemoryDC memDC(hdc, clientRect);

        HBRUSH backBrush = ::CreateSolidBrush(this->BackgroundColor());
        ::FillRect(memDC, &(RECT) clientRect, backBrush);
        DeleteObject(backBrush);

        this->DefaultWindowProc(WM_PAINT, (WPARAM) (HDC) memDC, NULL);
    }
    //
    ::EndPaint(this->Handle(), &paintStruct);
}

void        TabView::OnEraseBackground(HDC hdc)
{
    // do nothing!
}

LRESULT     TabView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NOTIFY:
        {
            NMHDR* notifyHeader = reinterpret_cast<NMHDR*>(lParam);
            switch (notifyHeader->code)
            {
            case TCN_SELCHANGE:
                this->OnTabSelected();
                return 0;
            }
        }
        return 0;   // stack overflow if we don't return that we handled this!
    }

    return this->DefaultWindowProc(message, wParam, lParam);
}

void        TabView::OnTabSelected()
{
    int tabIndex = TabCtrl_GetCurSel(this->Handle());
    Window* window = this->WindowForTabIndex(tabIndex);

    if (window)
    {
        if (this->visibleChild)
        {
            this->visibleChild->SetVisible(false);
        }

        window->SetVisible(true);
        this->visibleChild = window;
    }

    this->Layout();
}

void        TabView::OnResized(const Size& newSize)
{
    this->Layout();
}

void        TabView::Layout()
{
    if ( ! this->visibleChild)
    {
        return;
    }

    // calculate the display rect
    RECT windowRect = this->WindowRect();
    TabCtrl_AdjustRect(this->Handle(), FALSE, &windowRect);
    //
    POINT topLeft;
    topLeft.x = windowRect.left;
    topLeft.y = windowRect.top;
    //
    ::ScreenToClient(this->Handle(), &topLeft);
    // 
    Rect normalizedWindowRect(windowRect);
    normalizedWindowRect.location.x = topLeft.x;
    normalizedWindowRect.location.y = topLeft.y;
    
    this->visibleChild->SetRectangle(normalizedWindowRect);
}

void        TabView::OnChildAdded(Window* child)
{
    WindowToTabIndexMap::iterator it = this->windowToTabMap.find(child);
    if (it == this->windowToTabMap.end())
    {
        // child wasn't added via AddTab, throw.
        throw AddTabNotUsedException();
    }

    // if we already have a child displayed, hide this one!
    if (this->visibleChild)
    {
        child->SetVisible(false);
    }
}

void        TabView::SelectFirstChild()
{
    TabCtrl_SetCurSel(this->Handle(), 0);
    this->OnTabSelected(); // grumble. we really have to do this manually?
}

Window*     TabView::WindowForTabIndex(int tabIndex)
{
    // boost::bimap would be nice...
    WindowToTabIndexMap::iterator it = this->windowToTabMap.begin();
    WindowToTabIndexMap::iterator end = this->windowToTabMap.end();

    for ( ; it != end; it++)
    {
        if (it->second == tabIndex)
        {
            return it->first;
        }
    }

    return NULL;
}

void        TabView::OnGainedFocus()
{
    // don't do anything! we can be focused.
}

Window* TabView::ActiveWindow()
{ 
    return this->WindowForTabIndex(TabCtrl_GetCurSel(this->Handle())); 
}

