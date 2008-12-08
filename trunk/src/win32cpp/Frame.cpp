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
#include <win32cpp/Frame.hpp>
#include <win32cpp/Color.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

#define CLASS_NAME _T("Frame")

///\brief Constructor
///
///\remarks 
///If child is not specified (NULL) it can be added later by using the
///Container::AddChild method.
///
///\param child
///The Window to be housed within the frame.
///
///\param padding
///The amount of padding, in pixels, for all edges.
/*ctor*/    Frame::Frame(Window* child, int padding)
: base()
, padding(padding, padding, padding, padding)
, child(child)
, isResizingHACK(false)
{
}

///\brief Constructor
///
///\remarks 
///If child is not specified (NULL) it can be added later by using the
///Container::AddChild method.
///
///\param child
///The Window to be housed within the frame.
///
///\param padding
///The padding parameters to use.
/*ctor*/    Frame::Frame(Window* child, const FramePadding& padding)
: base()
, padding(padding)
, child(child)
, isResizingHACK(false)
{
}

///\brief Set the Child's padding.
///
///\param padding
///The new padding parameters to use.
void        Frame::SetPadding(const FramePadding& padding)
{
    this->padding = padding;
    this->OnResized(this->WindowSize());
}

///\brief Set the Child's padding.
///
///\param padding
///The amount of padding, in pixels, for all edges.
void        Frame::SetPadding(int padding)
{
    this->SetPadding(FramePadding(padding, padding, padding, padding));
}

void        Frame::OnCreated()
{
    if (this->child)
    {
        this->AddChild(child);
    }
}

bool        Frame::AddChildWindow(Window* window)
{
    if (this->childWindows.size())
    {
        throw TooManyChildWindowsException();
    }

    return base::AddChildWindow(window);
}

void        Frame::OnChildAdded(Window* newChild)
{
    this->child = newChild;

    this->ResizeFromChild();
    this->child->Resized.connect(this, &Frame::OnChildResized);

    newChild->MoveTo(0, 0);
}

void        Frame::OnResized(const Size& newSize)
{
    if (this->child && (! isResizingHACK))
    {
        this->child->Resize(this->ClientSize());
        this->child->MoveTo(0, 0);
    }
}

void        Frame::OnChildResized(Window* window, Size newSize)
{
    // If we were resized then we had to change our child's size. Doing 
    // so will cause a Resize event to fire, resulting in and endless
    // recursive loop. Don't field this event if WE did the resizing!
    if ( ! this->isResizingHACK)
    {
        this->ResizeFromChild();
    }
}

void        Frame::ResizeFromChild()
{
    Size size = this->child->WindowSize();
 
    size.width += (this->padding.left + this->padding.right);
    size.height += (this->padding.top + this->padding.bottom);
    //
    this->isResizingHACK = true;
    this->Resize(size);
    this->isResizingHACK = false;

    this->child->MoveTo(0, 0);
}

LRESULT     Frame::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
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

HWND        Frame::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    if ( ! Frame::RegisterWindowClass())
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

bool        Frame::RegisterWindowClass()
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