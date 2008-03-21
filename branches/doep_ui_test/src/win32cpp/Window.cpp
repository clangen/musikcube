//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2007, Casey Langen
//
// Sources and Binaries of: mC2, win32cpp
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
#include <win32cpp/Font.hpp>
#include <win32cpp/Window.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////
// Window
//////////////////////////////////////////////////////////////////////////////

Window::WindowList          Window::sAllChildWindows;
Window::HandleToWindowMap   Window::sHandleToWindowMap;
FontRef                     Window::sDefaultFont(new win32cpp::Font());

//////////////////////////////////////////////////////////////////////////////
// Window::Window
//////////////////////////////////////////////////////////////////////////////

/*ctor*/    Window::Window()
: windowHandle(NULL)
, defaultWindowProc(NULL)
, font(Window::sDefaultFont)
, usesDefaultFont(true)
, backgroundBrush(NULL)
{
}

/*dtor*/    Window::~Window()
{
    ::DeleteObject(this->backgroundBrush);
}

///\brief
///Used to force creation of a control. Although this can be called manually
///it's generally recommended to let Container controls automatically
///intialize their children.
///
///Unless creating a TopLevelWindow, parent should always be non-NULL
///and initialized.
///
///\throws WindowAlreadyCreatedException, WindowCreationFailedException
///
///\param parent
///The Window's parent.
void        Window::Initialize(Window* parent)
{
    if (this->windowHandle != NULL)
    {
        throw WindowAlreadyCreatedException();
    }

    HWND result = this->Create(parent);
    //
    if ( ! result)
    {
        throw WindowCreationFailedException();
    }

    Window::sHandleToWindowMap[result] = this;
    this->windowHandle = result;
    this->SetFont(this->font);
    this->SetMenu(this->menu);

    Window::ManageWindow(this);

    this->OnCreatedBase();
}

///\brief
///Used to modify the visibility of a Window. Commonly used flags are SW_SHOW and SW_HIDE.
///
///\param showCommand
///One of the flags listed here: http://msdn2.microsoft.com/en-us/library/ms633548.aspx
///
///\returns
///true if the operation was successful, false otherwise..
bool        Window::Show(int showCommand)
{
    return (::ShowWindow(this->windowHandle, showCommand) == TRUE);
}

///\brief
///Destroy the associated HWND.
///
///If this Window is a contained within a Container this method will be
///called automatically.
///
///\returns
///true if successful, false otherwise.
///\see
///Container
bool        Window::Destroy()
{
    HWND windowHandle = this->Handle();
    //
    if (windowHandle)
    {
        BOOL result = ::DestroyWindow(windowHandle);
        //
        if (result)
        {
            Window::sHandleToWindowMap.erase(windowHandle);
            this->windowHandle = NULL;
        }

        return (result == TRUE);
    }

    return false;
}

bool        Window::WindowHasParent(Window* window)
{
    return (Window::sAllChildWindows.find(window) != Window::sAllChildWindows.end());
}

Window*     Window::WindowUnderCursor(HWND* targetHwnd)
{
    POINT cursorPos;
    ::GetCursorPos(&cursorPos);

    HWND windowUnderMouse = ::WindowFromPoint(cursorPos);

    if ( ! windowUnderMouse)
    {
        return NULL;
    }

    // find child-most window under mouse
    POINT ptCopy;
    while (true)
    {
        ptCopy.x = cursorPos.x;
        ptCopy.y = cursorPos.y;

        ::ScreenToClient(windowUnderMouse, &ptCopy);

        HWND temp = ::RealChildWindowFromPoint(windowUnderMouse, ptCopy);

        // http://msdn2.microsoft.com/en-us/library/ms632676.aspx
        if ((temp == NULL) || (temp == windowUnderMouse))
        {
            break;
        }

        windowUnderMouse = temp;
    }

    if (targetHwnd != NULL)
    {
        *targetHwnd = windowUnderMouse;
    }

    HandleToWindowMap::iterator it = Window::sHandleToWindowMap.find(windowUnderMouse);
    //
    return (it == Window::sHandleToWindowMap.end() ? NULL : it->second);

}

//////////////////////////////////////////////////////////////////////////////
// HACK 1/2: necessary to generate reliable OnMouseEnter/Leave() events.
//////////////////////////////////////////////////////////////////////////////

static Window* sLastWindowUnderMouse = NULL;
//
void        Window::BeginCapture(Window* window)
{
    if ((window) && (window->Handle() != ::GetCapture()))
    {
        ::SetCapture(window->Handle());
    }
}
//
Window*     Window::Capture()
{
    HandleToWindowMap::iterator it = Window::sHandleToWindowMap.find(::GetCapture());
    if (it != Window::sHandleToWindowMap.end())
    {
        return it->second;
    }

    return NULL;
}
//
void        Window::EndCapture(Window* window)
{
    if ((window) && (::GetCapture() == window->Handle()))
    {
        ::ReleaseCapture();
    }
}

#define DISPATCH_MOUSE_EVENT(eventName)                                             \
    {                                                                               \
        Point mousePos = Point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));         \
        Window* windowToNotify = Window::Capture();                                 \
        if ( ! windowToNotify) windowToNotify = sLastWindowUnderMouse;              \
        if ( ! windowToNotify) windowToNotify = Window::WindowUnderCursor();        \
        if (windowToNotify)                                                         \
        {                                                                           \
            windowToNotify->eventName((MouseEventFlags) wParam, mousePos);          \
        }                                                                           \
    }                                                                               \

//////////////////////////////////////////////////////////////////////////////
// ENDHACK!
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// HACK 2/2: remember last Window that whose measure was forced
//////////////////////////////////////////////////////////////////////////////

static HWND sMeasureItemHwnd = NULL;

void    ForceMeasureItem(HWND handle)
{
    sMeasureItemHwnd = handle;

    // Hack to make Window call WM_MEASUREITEM again
    RECT windowRect;
    ::GetWindowRect(handle, &windowRect);
    //
    WINDOWPOS windowPos;
    windowPos.hwnd = handle;
    windowPos.cx = windowRect.right - windowRect.left;
    windowPos.cy = windowRect.bottom - windowRect.top;
    windowPos.flags = SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER;
    //
    ::SendMessage(handle, WM_WINDOWPOSCHANGED, 0, (LPARAM)&windowPos);
}

HWND    LastMeasureItemHandle()
{
    HWND handle = sMeasureItemHwnd;
    sMeasureItemHwnd = NULL;
    //
    return handle;
}

void        Window::ForceMeasureItem(const Window* window)
{
    if (window && window->Handle())
    {
        ::ForceMeasureItem(window->Handle());
    }
}

//////////////////////////////////////////////////////////////////////////////
// ENDHACK
//////////////////////////////////////////////////////////////////////////////

void        Window::PostWindowProcBase(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_MOUSELEAVE:
        {
            if (sLastWindowUnderMouse == this)
            {
                this->OnMouseExitBase();
            }
        }
        break;

    case WM_MOUSEMOVE:
        {
            Point mousePos = Point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

            // If a non-Window (an unknown HWND) is capturing. just let it be.
            Window* currentCapture = Window::Capture();
            if ( ! currentCapture && ::GetCapture())
            {
                break;
            }

            Window* windowToNotify = currentCapture;

            if (currentCapture)
            {
                windowToNotify = currentCapture;
            }
            else
            {
                // The window under the mouse is different than the last window
                // that was under the mouse, so notify the new one.
                Window* underMouse = Window::WindowUnderCursor();
                //
                if ((underMouse) && (underMouse != sLastWindowUnderMouse))
                {
                    windowToNotify = underMouse;
                }
                // The current window under the mouse is the same as the
                // last window under the mouse, so notify it.
                else
                {
                    windowToNotify = sLastWindowUnderMouse;
                }
            }

            if (windowToNotify)
            {
                windowToNotify->OnMouseMovedBase((MouseEventFlags) wParam, mousePos);
            }            
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        {
            DISPATCH_MOUSE_EVENT(OnMouseButtonDownBase)
        }
        break;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        {
            DISPATCH_MOUSE_EVENT(OnMouseButtonUpBase)
        }
        break;
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
        {
            DISPATCH_MOUSE_EVENT(OnMouseButtonDoubleClickedBase)
        }
        break;
    case WM_DESTROY:
        this->OnDestroyedBase();
        break;
    }

    this->PostWindowProc(message, wParam, lParam);
}

LRESULT     Window::PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool& discardMessage)
{
    return 0;
}

void        Window::PostWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
}

LRESULT     Window::DefaultWindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (this->defaultWindowProc)
    {
        return ::CallWindowProc(this->defaultWindowProc, this->Handle(), message, wParam, lParam);
    }

    return ::DefWindowProc(this->Handle(), message, wParam, lParam);
}

LRESULT CALLBACK Window::StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Window::HandleToWindowMap::iterator it = Window::sHandleToWindowMap.find(hwnd);
    //
    if (it != Window::sHandleToWindowMap.end())
    {
        // sometimes messages, such as WM_COMMAND, are automatically sent to
        // parent controls. handling these messages here allows derived classes 
        // to recieve their messages so they can raise events.
        switch (message)
        {
        case WM_COMMAND:
            {
                // control message
                Window* sender = Window::sHandleToWindowMap[(HWND) lParam];
                if (sender)
                {
                    sender->WindowProc(message, wParam, lParam);
                    return 0;
                }

                // menu message
                if (HIWORD(wParam) == 0)
                {
                    Menu::ItemActivated(LOWORD(wParam));
                    return 0;
                }
            }
            break;

        // forward message to sender, instead of parent
        case WM_NOTIFY:
            {
                if (lParam)
                {
                    LPNMHDR notifyHeader = reinterpret_cast<LPNMHDR>(lParam);
                    Window* sender = Window::sHandleToWindowMap[notifyHeader->hwndFrom];
                    if (sender)
                    {
                        return sender->WindowProc(message, wParam, lParam);
                    }

                    sender = Window::sHandleToWindowMap[hwnd];
                    if (sender)
                    {
                        return sender->WindowProc(message, wParam, lParam);
                    }
                }
            }
            break;

        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORDLG:
        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLORSCROLLBAR:
        case WM_CTLCOLORSTATIC:
            {
                Window* sender = Window::sHandleToWindowMap[(HWND) lParam];

                if (sender && sender->backgroundColor)
                {
                    Color bgColor = sender->BackgroundColor();
                    if ( ! sender->backgroundBrush)
                    {
                        sender->backgroundBrush = ::CreateSolidBrush(bgColor);
                    }

                    return reinterpret_cast<LRESULT>(sender->backgroundBrush);
                }
            }
            break;

        // forward message to sender, instead of parent
        case WM_MEASUREITEM:
            {
                // This message is awful.  You can only artifically generate it by repositioning
                // the window, and then it doesn't even contain a handle to the window that
                // generated it. ::ForceMeasureItem() remembers the HWND of the item that triggered
                // the event, and ::MeasureItemHandle() retrieves it.
                HWND hwndToMeasure = ::LastMeasureItemHandle();
                //
                if (hwndToMeasure && lParam)
                {
                    Window* sender = Window::sHandleToWindowMap[hwndToMeasure];
                    if (sender)
                    {
                        sender->OnMeasureItem(reinterpret_cast<MEASUREITEMSTRUCT*>(lParam));
                    }
                }
            }
            break;

        case WM_ERASEBKGND:
            {
                it->second->OnEraseBackground((HDC) wParam);
            }
            return 1;

        case WM_PAINT:
            {
                it->second->OnPaint();
            }
            return 0;

        case WM_SETFOCUS:
            {
                it->second->OnGainedFocusBase();
            }
            return 0;

        case WM_KILLFOCUS:
            {
                it->second->OnLostFocusBase();
            }
            return 0;

        case WM_THEMECHANGED:
            {
                it->second->OnThemeChangedBase();
            }
            break;
        }

        bool discardMessage = false;
        LRESULT preResult = it->second->PreWindowProc(message, wParam, lParam, discardMessage);

        if (discardMessage)
        {
            return preResult;
        }

        LRESULT result = it->second->WindowProc(message, wParam, lParam);
        it->second->PostWindowProcBase(message, wParam, lParam);

        return result;
    }

    return ::DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT     Window::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    return this->DefaultWindowProc(message, wParam, lParam);
}

///\brief
///Moves and resizes the Window to rectangle.location and rectangle.size.
///
///Emits Window::Moved and Window::Resized. Note this method sets the
///WindowRect, and not the ClientRect. The ClientRect cannot be set
///explicitly.
///
///\param rectangle
///Specifies the Window's new location and size.
///
///\returns
///true if successful, false otherwise.
///
///\see
///Window::MoveTo, Window::Resize, Window::WindowRect, Window::ClientRect
bool        Window::SetRectangle(const Rect& rectangle)
{
    return (this->MoveTo(rectangle.location) && this->Resize(rectangle.size));
}

///\brief
///Moves the Window to the specified coordinates.
///
///The coordinates specified are interpreted as relative to the
///control's parent window. Will emit the Window::Moved event
///if successful.
///
///\param x
///location of the window, in pixels, relative to the parent
///
///\param y
///location of the window, in pixels, relative to the parent
///
///\returns
///true if succesful, false otherwise.
bool        Window::MoveTo(int x, int y)
{
    return this->MoveTo(Point(x, y));
}

///\brief
///Moves the Window to the specified coordinates.
///
///The coordinates specified are interpreted as relative to the
///control's parent window. Will emit the Window::Moved event
///if successful.
///
///\param location
///location of the window, in pixels, relative to the parent
///
///\returns
///true if succesful, false otherwise.
bool        Window::MoveTo(const Point& location)
{
    RECT windowRect;
    if (::GetWindowRect(this->windowHandle, &windowRect))
    {
        BOOL result =::MoveWindow(
            this->windowHandle,
            location.x,
            location.y,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            TRUE);

        if (result)
        {
            this->OnMovedBase(location);
        }

        return (result == TRUE);
    }

    return false;
}

///\brief
///Resizes the window to the specified size.
///
///\param width
///width of the window, in pixels.
///
///\param height
///height of the window, in pixels.
///
///\returns
///true on success, false otherwise.
bool        Window::Resize(int width, int height)
{
    return this->Resize(Size(width, height));
}

///\brief
///Resizes the window to the specified size.
///
///\param size
///size of the window, in pixels.
///
///\returns
///true on success, false otherwise.
bool        Window::Resize(const Size& size)
{
    RECT windowRect;
    if (::GetWindowRect(this->windowHandle, &windowRect))
    {
        POINT topLeft;
        topLeft.x = windowRect.left;
        topLeft.y = windowRect.top;

        ::ScreenToClient(::GetParent(this->windowHandle), &topLeft);

        BOOL result = ::MoveWindow(
            this->windowHandle,
            topLeft.x,
            topLeft.y,
            size.width,
            size.height,
            TRUE);

        if (result)
        {
            this->OnResizedBase(size);
        }

        return (result == TRUE);
    }

    return false;
}

///\brief
///Changes the enabled state of the Window.
///
///\param enable
///whether or not the Window is enabled.
///
///\returns
///true if successful, false otherwise.
bool        Window::Enable(bool enable)
{
    return (::EnableWindow(this->windowHandle, enable) == TRUE);
}

///\brief
///Returns the caption of the Window.
///
///Internally this method uses Win32's GetWindowText
uistring    Window::Caption() const
{
    static uichar text[1024];
    //
    if ((this->windowHandle) && (::GetWindowText(this->windowHandle, text, 1024)))
    {
        return uistring(text);
    }

    return uistring();
}

///\brief
///Sets the caption of the Window.
///
///Internally this method uses Win32's SetWindowText
///
///\param caption
///The desired Window caption.
///
///\returns
///true if successful, false otherwise.
bool        Window::SetCaption(const uistring& caption)
{
    if (caption == this->Caption())
    {
        return true;
    }

    if ((this->windowHandle) && (::SetWindowText(this->windowHandle, caption.c_str())))
    {
        this->OnCaptionChanged();
        return true;
    }

    return false;
}

///\brief
///Returns the Window's client rectangle.
///
///The client rectangle is the portion of the Window that content can
///be displayed. The Window::WindowRect is the bounding rectangle of
///the entire Window, including borders.
///
///\returns
///the Window's client rectangle.
///
///\see
///Window::WindowRect, Window::SetRectangle
Rect        Window::ClientRect() const
{
    static RECT clientRect;
    //
    if ((this->windowHandle) && (::GetClientRect(this->windowHandle, &clientRect)))
    {
        return Rect(clientRect);
    }

    return Rect();
}

///\brief
///Returns the Window's rectangle.
///
///The Window::WindowRect is the bounding rectangle of the entire Window
///including borders. Not to be confused with Windodw::ClientRect.
///
///\returns
///the Window's rectangle.
///
///\see
///Window::ClientRect, Window::SetRectangle
Rect        Window::WindowRect() const
{
    static RECT windowRect;
    //
    if ((this->windowHandle) && (::GetWindowRect(this->windowHandle, &windowRect)))
    {
        return Rect(windowRect);
    }

    return Rect();
}

///\brief
///Returns the size of the Window.
///
///Not to be confused with Window::ClientSize, Window::WindowSize returns
///the size of the entire window, not just the content area.
///
///\returns
///the size of the Window.
///
///\see
///Window::ClientSize
Size        Window::WindowSize() const
{
    Rect windowRect = this->WindowRect();
    return windowRect.size;
}

///\brief
///Returns the client (content) size of the Window. 
///
///Not to be confused with Window::WindowSize, Window::ClientSize returns
///the size of the content area, and doesn't include any borders.
///
///\returns
///the size of the client area of the Window.
///
///\see
///Window::WindowSize
Size        Window::ClientSize() const
{
    Rect clientRect = this->ClientRect();
    return clientRect.size;
}

///\brief
///Returns the location of the window, relative to its parent Window.
///
///\returns
///the location of the window, relative to its parent Window.
Point       Window::Location() const
{
    Rect windowRect = this->WindowRect();
    return windowRect.location;
}

///\brief
///Returns the position of the cursor, relative to the Window.
///
///\returns
///the position of the cursor, relative to the Window.
Point       Window::CursorPosition() const
{
    POINT cursorPos;
    ::GetCursorPos(&cursorPos);
    ::ScreenToClient(this->Handle(), &cursorPos);

    return cursorPos;
}

///\brief
///Returns a pointer to the Window's parent.
///
///This method will throw a WindowNotCreatedException if the Window has
///not been created yet. It may also return NULL if the Window is
///created but currently has no parent.
///
///\throws WindowNotCreatedException
///
///\returns
///Write description of return value here.
///
///\throws WindowNotCreatedException
Window*     Window::Parent() const
{
    if ( ! this->windowHandle)
    {
        throw WindowNotCreatedException();
    }

    HandleToWindowMap::iterator it = Window::sHandleToWindowMap.find(this->windowHandle);
    //
    if (it != Window::sHandleToWindowMap.end())
    {
        HWND parentHwnd = ::GetParent(this->windowHandle);
        it = Window::sHandleToWindowMap.find(parentHwnd);
        //
        if (it != Window::sHandleToWindowMap.end())
        {
            return it->second;
        }
    }

    return NULL;
}

///\brief
///Synchronously invokes a message on the Window.
///
///See: http://msdn2.microsoft.com/en-us/library/ms644950.aspx
///
///\param message
///The message ID
///
///\param wParam
///The wParam value
///
///\param lParam
///The lParam value.
///
///\returns
///Value varies based on the message. See MSDN for more information.
///
///\see
///Window::PostMessage
LRESULT     Window::SendMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    if ( ! this->windowHandle)
    {
        return 0;
    }

    return ::SendMessage(this->windowHandle, message, wParam, lParam);
}

///\brief
///Asynchronously invokes a message on the Window.
///
///See: http://msdn2.microsoft.com/en-us/library/ms644944.aspx
///
///\param message
///The message ID
///
///\param wParam
///The wParam value
///
///\param lParam
///The lParam value.
///
///\returns
///Value varies based on the message. See MSDN for more information.
///
///\see
///Window::SendMessage
bool        Window::PostMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    if ( ! this->windowHandle)
    {
        return 0;
    }

    return (::PostMessage(this->windowHandle, message, wParam, lParam) == TRUE);
}

///\brief
///Returns the HWND that the Window wraps.
///
///\returns
///the HWND that the Window wraps.
///
///\see
///Window::SendMessage(), Window::PostMessage
HWND        Window::Handle() const
{
    return this->windowHandle;
}

///\brief
///Redraws the Window.
///
///Internally this method calls ::InvalidateRect(Handle(), NULL, FALSE)
void        Window::Redraw() const
{
    ::InvalidateRect(this->Handle(), NULL, FALSE);
}

///\brief
///Sets the Font the Window uses to draw its caption.
///
///\param font
///A shared pointer to a Font object. A FontRef can be shared amongst
///different controls.
///
///\see
///Font, FontRef
void        Window::SetFont(FontRef font)
{
    this->usesDefaultFont = (font == Window::sDefaultFont);

    bool isNewFont = (this->font != font);
    if (isNewFont) this->font = font;

    if (this->Handle())
    {
        ::SendMessage(this->Handle(), WM_SETFONT, (WPARAM) font->GetHFONT(), 1); // 1 = redraw
    }

    if (isNewFont) this->OnFontChanged();
}

///\brief
///Sets the Window's Menu.
///
///\see
///Menu, MenuRef, Window::Menu
void        Window::SetMenu(MenuRef menu)
{
    this->menu = menu;

    if (this->Handle() && this->menu)
    {
        if ( ! ::SetMenu(this->Handle(), this->menu->Handle()))
        {
            throw Win32Exception();
        }
    }
}

///\brief
///Returns the Window's Menu.
///
///\see
///Menu, MenuRef, Window::SetMenu
MenuRef     Window::Menu()
{
    return this->menu;
}

///\brief
///Shows or hides the window.
///
///\param visible
///If true the window will be made visible, otherwise it will be hidden.
///
///\see
///Windowd::Visible
void        Window::SetVisible(bool visible)
{
    if (this->Handle())
    {
        ::ShowWindow(this->Handle(), visible ? SW_SHOW : SW_HIDE);
    }
}

///\brief
///Returns true if the window is visible.
///
///\see
///Windowd::SetVisible
bool        Window::Visible()
{
    if (this->Handle())
    {
        LONG style = ::GetWindowLong(this->Handle(), GWL_STYLE);
        return ((style & WS_VISIBLE) != 0);
    }

    return false;
}

///\brief
///Returns a shared pointer to the Font the Window uses to draw its text.
///
///\returns
///a shared pointer to the Font the Window uses to draw its text
///different controls.
///
///\see
///Font, FontRef
FontRef     Window::Font() const
{
    return this->font;
}

///\brief
///Sets the background color of the ListView.
///
///\param color
///The color to use.
///
///\see
///Window::BackgroundColor
void        Window::SetBackgroundColor(const Color& color)
{
    this->backgroundColor.reset(new Color(color));

    ::DeleteObject(this->backgroundBrush);
    this->backgroundBrush = ::CreateSolidBrush(*this->backgroundColor);
}

///\brief
///Returns the Window's background color.
///
///\see
///Window::SetBackgroundColor
Color       Window::BackgroundColor() const
{
    return this->backgroundColor
        ? *this->backgroundColor
        : Color::SystemColor(COLOR_BTNFACE);
}

bool        Window::IsWindowManaged(Window* window)
{
    Window::HandleToWindowMap& hwndToWindow = Window::sHandleToWindowMap;

    return (hwndToWindow.find(window->Handle()) != hwndToWindow.end());
}

Window*      Window::ManagedWindowFromHWND(HWND hwnd)
{
    Window::HandleToWindowMap& hwndToWindow = Window::sHandleToWindowMap;

    HandleToWindowMap::iterator it = hwndToWindow.find(hwnd);
    if (it != hwndToWindow.end())
    {
        return it->second;
    }

    return NULL;
}

#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4312)
void         Window::ManageWindow(Window* window)
{
    WindowProcFunc currentWindowProc = reinterpret_cast<WindowProcFunc>(
        ::GetWindowLongPtr(window->Handle(), GWLP_WNDPROC));

    if (currentWindowProc != &Window::StaticWindowProc)
    {
        // remember its old WndProc
        window->defaultWindowProc = currentWindowProc;

        // subclass the window
        ::SetWindowLongPtr(
            window->Handle(),
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(Window::StaticWindowProc));
    }
}
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4244)
#pragma warning(disable: 4312)
void        Window::UnManageWindow(Window* window)
{
    WindowProcFunc currentWindowProc = reinterpret_cast<WindowProcFunc>(
        ::GetWindowLongPtr(window->Handle(), GWLP_WNDPROC));

    if (currentWindowProc == &Window::StaticWindowProc)
    {
        // unsubclass the window
        ::SetWindowLongPtr(
            window->Handle(),
            GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(window->defaultWindowProc));

        window->defaultWindowProc = NULL;
    }
}
#pragma warning(pop)

//////////////////////////////////////////////////////////////////////////////
// Win32 event wrappers (using the template pattern)
//////////////////////////////////////////////////////////////////////////////

#define EMIT_SIGNAL_IF_NOT_SUPPRESSED(signal, ...)                                  \
    {                                                                               \
        if (this->suppressedSignals.find(&signal) == this->suppressedSignals.end()) \
            signal(__VA_ARGS__);                                                    \
        this->suppressedSignals.clear();                                            \
    }                                                                               \

void        Window::SuppressSignal(SignalBase& signal)
{
    this->suppressedSignals.insert(&signal);
}

void        Window::OnDestroyedBase()
{
    this->OnDestroyed();
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->Destroyed);
}

void        Window::OnCreatedBase()
{
    this->OnCreated();
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->Created);
}

void        Window::OnMovedBase(const Point& location)
{
    this->OnMoved(location);
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->Moved, location);
}

void        Window::OnResizedBase(const Size& newSize)
{
    this->OnResized(newSize);
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->Resized, newSize);
}

void        Window::OnMouseMovedBase(MouseEventFlags flags, const Point& location)
{
    if (sLastWindowUnderMouse != this)
    {
        if (sLastWindowUnderMouse)
        {
            sLastWindowUnderMouse->OnMouseExitBase();
        }

        this->OnMouseEnterBase();
    }
    //
    sLastWindowUnderMouse = this;

    this->OnMouseMoved(flags, location);
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->MouseMoved, flags, location);
}

void        Window::OnMouseButtonDownBase(MouseEventFlags flags, const Point& location)
{
    this->OnMouseButtonDown(flags, location);
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->MouseButtonDown, flags, location);
}

void        Window::OnMouseButtonUpBase(MouseEventFlags flags, const Point& location)
{
    this->OnMouseButtonUp(flags, location);
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->MouseButtonUp, flags, location);
}

void        Window::OnMouseButtonDoubleClickedBase(MouseEventFlags flags, const Point& location)
{
    this->OnMouseButtonDoubleClicked(flags, location);
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->MouseButtonDoubleClicked, flags, location);
}

void        Window::OnMouseEnterBase()
{
#if 0 && _DEBUG
    uistring output = _T("mouse enter: ") + this->Caption() + _T("\n");
    OutputDebugString(output.c_str());
#endif

    this->OnMouseEnter();
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->MouseEnter);

    if (::GetCapture() != this->Handle())
    {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.dwHoverTime = 0;
        tme.hwndTrack = this->Handle();
        ::TrackMouseEvent(&tme);
    }
}

void        Window::OnMouseExitBase()
{
#if 0 && _DEBUG
    uistring output = _T("mouse exit: ") + this->Caption() + _T("\n");
    OutputDebugString(output.c_str());
#endif

    sLastWindowUnderMouse = NULL;

    this->OnMouseExit();
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->MouseExit);
}

void        Window::OnGainedFocusBase()
{
    this->OnGainedFocus();
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->GainedFocus);
}

void        Window::OnLostFocusBase()
{
    this->OnLostFocus();
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->LostFocus);
}

void        Window::OnThemeChangedBase()
{
    this->OnThemeChanged();
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->ThemeChanged);
}

void        Window::OnParentChangedBase(Window* oldParent, Window* newParent)
{
    this->OnParentChanged(oldParent, newParent);
    EMIT_SIGNAL_IF_NOT_SUPPRESSED(this->ParentChanged, oldParent, newParent);
}

void        Window::OnThemeChanged()
{
    // If we're the first window to notice the theme change then update
    // the default font.
    if (this->font == Window::sDefaultFont)
    {
        Window::sDefaultFont.reset(new win32cpp::Font());
    }

    if (this->usesDefaultFont)
    {
        this->SetFont(Window::sDefaultFont);
    }

    Window::ForceMeasureItem(this);
}


void        Window::OnPaint()
{
    PAINTSTRUCT paintStruct;
    HDC hdc = ::BeginPaint(this->Handle(), &paintStruct);
    //
    {
        MemoryDC memDC(hdc, paintStruct.rcPaint);
        //
        ::SetBkColor(memDC, this->BackgroundColor());
        this->PaintToHDC(memDC, paintStruct.rcPaint);
    }
    //
    ::EndPaint(this->Handle(), &paintStruct);
}

void        Window::OnEraseBackground(HDC hdc)
{
    ::InvalidateRect(this->Handle(), NULL, FALSE);
}

void        Window::PaintToHDC(HDC hdc, const Rect& rect)
{
    this->WindowProc(WM_PRINT, (WPARAM) (HDC) hdc, PRF_CLIENT);
}

void         Window::SetParent(Window* child, Window* newParent)
{
    // Only alow the reparent if both windows have been created
    // and the new parent is different than the current.
    if (child && newParent)
    {
        Window* oldParent = child->Parent();

        if (oldParent)
        {
            if ((child->Handle() && newParent->Handle()) && (oldParent != newParent))
            {
                if (::SetParent(child->Handle(), newParent->Handle()))
                {
                    child->OnParentChangedBase(oldParent, newParent);
                }
            }
        }
    }
}
