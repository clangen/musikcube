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

#pragma once

#include <set>
#include <map>

#include <win32cpp/Win32Config.hpp>          // Must be first!
#include <win32cpp/Font.hpp>
#include <win32cpp/Color.hpp>
#include <win32cpp/Menu.hpp>
#include <win32cpp/Types.hpp>
#include <win32cpp/Exception.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class Window;

///\brief Flags used for mouse related events emitted by Window.
enum MouseEventFlags
{
    MouseShiftKey = MK_SHIFT,       /*!< */ 
    MouseCtrlKey = MK_CONTROL,      /*!< */ 
    MouseLeftButton = MK_LBUTTON,   /*!< */ 
    MouseMiddleButton = MK_MBUTTON, /*!< */ 
    MouseRightButton = MK_RBUTTON,  /*!< */ 
    MouseX1Button = MK_XBUTTON1,    /*!< */ 
    MouseX2Button = MK_XBUTTON2     /*!< */ 
};

/*! */ typedef DWORD VirtualKeyCode;
/*! */ typedef DWORD KeyEventFlags;

/*! */ typedef sigslot::signal1<Window*> DestroyedEvent;
/*! */ typedef sigslot::signal1<Window*> CreatedEvent;
/*! */ typedef sigslot::signal2<Window*, Point> MovedEvent;
/*! */ typedef sigslot::signal2<Window*, Size> ResizedEvent;
/*! */ typedef sigslot::signal1<Window*> MouseEnterEvent;
/*! */ typedef sigslot::signal1<Window*> MouseExitEvent;
/*! */ typedef sigslot::signal3<Window*, MouseEventFlags, Point> MouseMovedEvent;
/*! */ typedef sigslot::signal3<Window*, MouseEventFlags, Point> MouseButtonEvent;
/*! */ typedef sigslot::signal1<Window*> ThemeChangedEvent;
/*! */ typedef sigslot::signal1<Window* /*this*/> FocusEvent;
/*! */ typedef sigslot::signal2<Window* /*old*/, Window* /*new*/> ParentChangedEvent;
/*! */ typedef sigslot::signal1<unsigned int> TimerEvent;
/*! */ typedef sigslot::signal3<Window*, VirtualKeyCode, KeyEventFlags> KeyEvent;
/*! */ typedef sigslot::signal2<Window*, bool> VisibilityChangedEvent;

///\brief
///Window is the abstract base class for all controls.
///
///Based on sigslot signals, Window follows the observer/notifier pattern
///and is able to "emit" notifications to listeners that are not
///necessarily GUI objects. This promotes use of the model/view/controller
///design pattern, which is highly recommended. Window also attempts to
///automatically double buffer all drawing, so controls don't flicker when 
///they are moved or resized. This is accomplished using the MemoryDC
///and RedrawLock classes.
///
///The Window class also provides "stable" mouse events for derived
///classes, including consistant cursor enter and exit notifications.
/*abstract*/ class Window: public EventHandler
{
public: // types
    ///\brief
    ///This exception will be thrown if Initialize() is called on a
    ///Window that has already been created.
    class WindowAlreadyCreatedException: public Exception { };
    ///\brief
    ///This exception may be thrown if operations are attempted on a
    ///Window that has not been created.
    class WindowNotCreatedException: public Exception { };
    ///\brief
    ///This exception is thrown if a Window fails to be created.
    class WindowCreationFailedException: public Exception { };
    ///\brief
    ///This exception is thrown if a Window parameter is NULL where it shouldn't be.
    class WindowIsNullException: public Exception { };

protected: // types
    typedef WNDPROC WindowProcFunc;
    typedef std::vector<Window*> WindowList;
    typedef std::map<HWND, Window*> HandleToWindowMap;
    typedef sigslot::_signal_base<sigslot::SIGSLOT_DEFAULT_MT_POLICY> SignalBase;
    typedef std::set<SignalBase*> SignalList;
    typedef boost::scoped_ptr<Color> ColorRef;

    enum FocusDirection
    {
        FocusForward,
        FocusBackward
    };

public:     // events
    ///\brief Emitted when the WM_DESTROY message is received.
    DestroyedEvent              Destroyed;
    ///\brief Emitted when the Window is created. This is not necessarily
    ///at construction time, as Windows are lazily created.
    CreatedEvent                Created;
    ///\brief Emitted when the Window is repositioned.
    MovedEvent                  Moved;
    ///\brief Emitted when the Window is resized.
    ResizedEvent                Resized;
    ///\brief Emitted when the cursor is moved over the Window.
    MouseMovedEvent             MouseMoved;
    ///\brief Emitted when a mouse button has been pressed over the Window.
    MouseButtonEvent            MouseButtonDown;
    ///\brief Emitted when a mouse button has been released over the Window.
    MouseButtonEvent            MouseButtonUp;
    ///\brief Emitted when a mouse button has been double clicked over the Window.
    MouseButtonEvent            MouseButtonDoubleClicked;
    ///\brief Emitted when the cursor has entered the Window.
    MouseEnterEvent             MouseEnter;
    ///\brief Emitted when the cursor has left the Window.
    MouseExitEvent              MouseExit;
    ///\brief Emitted when the desktop theme has changed.
    ThemeChangedEvent           ThemeChanged;
    ///\brief Emitted when the Window has gained focus.
    FocusEvent                  GainedFocus;
    ///\brief Emitted when the Window has lost focus.
    FocusEvent                  LostFocus;
    ///\brief Emitted when the Window's parent has changed.
    ParentChangedEvent          ParentChanged;
    ///\brief Emitted when a timer gets a timeout.
    TimerEvent                  TimerTimeout;
    ///\brief Emitted when a key on the keyboard is pressed down.
    KeyEvent                    KeyDown;
    ///\brief Emitted when a key on the keyboard is released.
    KeyEvent                    KeyUp;
    ///\brief Emitted when a key on the keyboard is pressed.
    KeyEvent                    Char;
    ///\brief Emitted when a control has requested to focus the next control. This
    ///generally shouldn't be handled explicitly unless absolutely necessary.
    FocusEvent                  RequestFocusNext;
    ///\brief Emitted when a control has requested to focus the previous control. This
    ///generally shouldn't be handled explicitly unless absolutely necessary.
    FocusEvent                  RequestFocusPrev;
    ///\brief Emitted when a Window's visibility has changed
    VisibilityChangedEvent      VisibilityChanged;

public: // ctor, dtor
    /*ctor*/            Window();
    /*dtor*/ virtual    ~Window();

public: // methods
    void            Initialize(Window* parent = NULL);
    bool            Show(int showCommand);
    bool            MoveTo(int x, int y);
    bool            MoveTo(const Point& location);
    bool            Resize(int width, int height);
    bool            Resize(const Size& size);
    bool            SetRectangle(const Rect& rectangle);
    bool            Enable(bool enable);
    uistring        Caption() const;
    bool            SetCaption(const uistring& caption);
    Rect            ClientRect() const;
    Rect            WindowRect() const;
    Size            WindowSize() const;
    Size            ClientSize() const;
    Point           Location() const;
    Point           CursorPosition() const;
    LRESULT         SendMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool            PostMessage(UINT message, WPARAM wParam, LPARAM lParam);
    HWND            Handle() const;
    virtual bool    Destroy();
    Window*         Parent() const;
    void            Redraw() const;
    void            SetFont(FontRef font);
    FontRef         Font() const;
    void            SetMenu(MenuRef menu);
    MenuRef         Menu();
    void            SetVisible(bool visible = true);
    bool            Visible();
    void            SetBackgroundColor(const Color& color);
    Color           BackgroundColor() const;
    bool            SetFocus();
    bool            TabStop();
    void            SetTabStop(bool enabled);

public: // operators
    operator bool() { return (this->windowHandle != NULL); }

protected: // methods
    // pure virtuals
    virtual HWND        Create(Window* parent = NULL) = 0;

    // management, mouse capture
    static void         ManageWindow(Window* window);
    static void         UnManageWindow(Window* window);
    static bool         IsWindowManaged(Window* window);
    static Window*      ManagedWindowFromHWND(HWND hwnd);
    static Window*      WindowUnderCursor(HWND* targetHwnd = NULL);
    static void         BeginCapture(Window* window);
    static void         EndCapture(Window* window);
    static void         SetParent(Window* child, Window* newParent);
    static void         ForceMeasureItem(const Window* window);
    static Window*      Capture();

    // win32 event wrappers (template pattern, effective c++ item 35)
    void    OnDestroyedBase();
    void    OnCreatedBase();
    void    OnMovedBase(const Point& location);
    void    OnResizedBase(const Size& newSize);
    void    OnMouseMovedBase(MouseEventFlags flags, const Point& location);
    void    OnMouseButtonDownBase(MouseEventFlags flags, const Point& location);
    void    OnMouseButtonUpBase(MouseEventFlags flags, const Point& location);
    void    OnMouseButtonDoubleClickedBase(MouseEventFlags flags, const Point& location);
    void    OnMouseEnterBase();
    void    OnMouseExitBase();
    void    OnGainedFocusBase();
    void    OnLostFocusBase();
    void    OnThemeChangedBase();
    void    OnParentChangedBase(Window* oldParent, Window* newParent);
    bool    OnKeyDownBase(VirtualKeyCode keyCode, KeyEventFlags flags);
    bool    OnKeyUpBase(VirtualKeyCode keyCode, KeyEventFlags flags);
    bool    OnCharBase(VirtualKeyCode keyCode, KeyEventFlags flags);
    void    OnVisibilityChangedBase(bool visible);

    // win32 event wrappers (virtual methods, for derived class use)
    virtual void    OnDestroyed() { }
    virtual void    OnCreated() { }
    virtual void    OnMoved(const Point& location) { }
    virtual void    OnResized(const Size& newSize) { }
    virtual void    OnMouseMoved(MouseEventFlags flags, const Point& location) { }
    virtual void    OnMouseButtonDown(MouseEventFlags flags, const Point& location) { }
    virtual void    OnMouseButtonUp(MouseEventFlags flags, const Point& location) { }
    virtual void    OnMouseButtonDoubleClicked(MouseEventFlags flags, const Point& location) { }
    virtual void    OnMouseEnter() { }
    virtual void    OnMouseExit() { }
    virtual void    OnGainedFocus() { }
    virtual void    OnLostFocus() { }
    virtual void    OnThemeChanged();
    virtual void    OnFontChanged() { }
    virtual void    OnMeasureItem(MEASUREITEMSTRUCT* measureItemStruct) { }
    virtual void    OnParentChanged(Window* oldParent, Window* newParent) { }
    virtual void    OnCaptionChanged() { }
    virtual bool    OnKeyDown(VirtualKeyCode keyCode, KeyEventFlags flags) { return false; }
    virtual bool    OnKeyUp(VirtualKeyCode keyCode, KeyEventFlags flags) { return false; }
    virtual bool    OnChar(VirtualKeyCode keyCode, KeyEventFlags flags) { return false; }
    virtual void    OnEraseBackground(HDC hdc);
    virtual void    OnPaint();
    virtual void    OnRequestFocusPrev();
    virtual void    OnRequestFocusNext();
    virtual HBRUSH  OnControlColor(HDC hdc);
    virtual void    OnVisibilityChanged(bool visible) { }

    // window proc related
    virtual LRESULT             PreWindowProcBase(UINT message, WPARAM wParam, LPARAM lParam, bool& discardMessage);
    virtual LRESULT             PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool& discardMessage);
    void                        PostWindowProcBase(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void                PostWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual LRESULT             WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT                     DefaultWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK     StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    // misc
    virtual void    PaintToHDC(HDC hdc, const Rect& rect);
    void            SuppressSignal(SignalBase& signal);
    static bool     WindowHasParent(Window* window);

protected: // instance data
    WindowProcFunc defaultWindowProc;
    HWND windowHandle;
    HBRUSH backgroundBrush;
    FontRef font;
    MenuRef menu;
    ColorRef backgroundColor;
    bool usesDefaultFont;
    SignalList suppressedSignals;
    bool tabStop;

protected: // class data
    static HandleToWindowMap sHandleToWindowMap;
    static WindowList sAllChildWindows;
    static FontRef sDefaultFont;
    static FocusDirection sFocusDirection;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
