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

#pragma once

#include <set>
#include <map>

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

/*! */ typedef sigslot::signal0<> DestroyedEvent;
/*! */ typedef sigslot::signal0<> CreatedEvent;
/*! */ typedef sigslot::signal1<Point> MovedEvent;
/*! */ typedef sigslot::signal1<Size> ResizedEvent;
/*! */ typedef sigslot::signal0<> MouseEnterEvent;
/*! */ typedef sigslot::signal0<> MouseExitEvent;
/*! */ typedef sigslot::signal2<MouseEventFlags, Point> MouseMovedEvent;
/*! */ typedef sigslot::signal2<MouseEventFlags, Point> MouseButtonEvent;
/*! */ typedef sigslot::signal0<> ThemeChangedEvent;
/*! */ typedef sigslot::signal0<> FocusEvent;
/*! */ typedef sigslot::signal2<Window* /*old*/, Window* /*new*/> ParentChangedEvent;
/*! */ typedef sigslot::signal1<unsigned int> TimerEvent;

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
/*abstract*/ class Window
{
            ///\brief
            ///This exception will be thrown if Initialize() is called on a
            ///Window that has already been created.
public:		class WindowAlreadyCreatedException: public Exception { };
            ///\brief
            ///This exception may be thrown if operations are attempted on a
            ///Window that has not been created.
public:     class WindowNotCreatedException: public Exception { };
            ///\brief
            ///This exception is thrown if a Window fails to be created.
public:     class WindowCreationFailedException: public Exception { };
            ///\brief
            ///This exception is thrown if a Window parameter is NULL where it shouldn't be.
public:     class WindowIsNullException: public Exception { };

protected:  typedef WNDPROC WindowProcFunc;
protected:  typedef std::set<Window*> WindowList;
protected:  typedef std::map<HWND, Window*> HandleToWindowMap;
protected:  typedef sigslot::_signal_base<sigslot::SIGSLOT_DEFAULT_MT_POLICY> SignalBase;
protected:  typedef std::set<SignalBase*> SignalList;
protected:  typedef boost::scoped_ptr<Color> ColorRef;

            ///\brief Emitted when the WM_DESTROY message is received.
public:     DestroyedEvent              Destroyed;
            ///\brief Emitted when the Window is created. This is not necessarily
            ///at construction time, as Windows are lazily created.
public:     CreatedEvent                Created;
            ///\brief Emitted when the Window is repositioned.
public:     MovedEvent                  Moved;
            ///\brief Emitted when the Window is resized.
public:     ResizedEvent                Resized;
            ///\brief Emitted when the cusror is moved over the Window.
public:     MouseMovedEvent             MouseMoved;
            ///\brief Emitted when a mouse button has been pressed over the Window.
public:     MouseButtonEvent            MouseButtonDown;
            ///\brief Emitted when a mouse button has been released over the Window.
public:     MouseButtonEvent            MouseButtonUp;
            ///\brief Emitted when a mouse button has been double clicked over the Window.
public:     MouseButtonEvent            MouseButtonDoubleClicked;
            ///\brief Emitted when the cursor has entered the Window.
public:     MouseEnterEvent             MouseEnter;
            ///\brief Emitted when the cursor has left the Window.
public:     MouseExitEvent              MouseExit;
            ///\brief Emitted when the desktop theme has changed.
public:     ThemeChangedEvent           ThemeChanged;
            ///\brief Emitted when the Window has gained focus.
public:     FocusEvent                  GainedFocus;
            ///\brief Emitted when the Window has lost focus.
public:     FocusEvent                  LostFocus;
            ///\brief Emitted when the Window's parent has changed.
public:     ParentChangedEvent          ParentChanged;
            ///\brief Emitted when a timer gets a timeout.
public:     TimerEvent                  TimerTimeout;

public:     /*ctor*/            Window();
public:     /*dtor*/ virtual    ~Window();

public:     operator bool() { return (this->windowHandle != NULL); }

        // instance
public:     void                Initialize(Window* parent = NULL);
public:     bool                Show(int showCommand);
public:     bool                MoveTo(int x, int y);
public:     bool                MoveTo(const Point& location);
public:     bool                Resize(int width, int height);
public:     bool                Resize(const Size& size);
public:     bool                SetRectangle(const Rect& rectangle);
public:     bool                Enable(bool enable);
public:     uistring            Caption() const;
public:     bool                SetCaption(const uistring& caption);
public:     Rect                ClientRect() const;
public:     Rect                WindowRect() const;
public:     Size                WindowSize() const;
public:     Size                ClientSize() const;
public:     Point               Location() const;
public:     Point               CursorPosition() const;
public:     LRESULT             SendMessage(UINT message, WPARAM wParam, LPARAM lParam);
public:     bool                PostMessage(UINT message, WPARAM wParam, LPARAM lParam);
public:     HWND                Handle() const;
public:     virtual bool        Destroy();
public:     Window*             Parent() const;
public:     void                Redraw() const;
public:     void                SetFont(FontRef font);
public:     FontRef             Font() const;
public:     void                SetMenu(MenuRef menu);
public:     MenuRef             Menu();
public:     void                SetVisible(bool visible = true);
public:     bool                Visible();
public:     void                SetBackgroundColor(const Color& color);
public:     Color               BackgroundColor() const;

        // pure virtuals
protected:  virtual HWND        Create(Window* parent = NULL) = 0;
        // child/parent management
protected:  static void         ManageWindow(Window* window);
protected:  static void         UnManageWindow(Window* window);
protected:  static bool         IsWindowManaged(Window* window);
protected:  static Window*      ManagedWindowFromHWND(HWND hwnd);
protected:  static Window*      WindowUnderCursor(HWND* targetHwnd = NULL);
protected:  static void         BeginCapture(Window* window);
protected:  static void         EndCapture(Window* window);
protected:  static void         SetParent(Window* child, Window* newParent);
protected:  static void         ForceMeasureItem(const Window* window);
protected:  static Window*      Capture();
        // win32 event wrappers (template pattern, effective c++ item 35)
protected:  void                OnDestroyedBase();
protected:  void                OnCreatedBase();
protected:  void                OnMovedBase(const Point& location);
protected:  void                OnResizedBase(const Size& newSize);
protected:  void                OnMouseMovedBase(MouseEventFlags flags, const Point& location);
protected:  void                OnMouseButtonDownBase(MouseEventFlags flags, const Point& location);
protected:  void                OnMouseButtonUpBase(MouseEventFlags flags, const Point& location);
protected:  void                OnMouseButtonDoubleClickedBase(MouseEventFlags flags, const Point& location);
protected:  void                OnMouseEnterBase();
protected:  void                OnMouseExitBase();
protected:  void                OnGainedFocusBase();
protected:  void                OnLostFocusBase();
protected:  void                OnThemeChangedBase();
protected:  void                OnParentChangedBase(Window* oldParent, Window* newParent);
        // win32 event wrappers (virtual methods, for derived class use)
protected:  virtual void        OnDestroyed() { }
protected:  virtual void        OnCreated() { }
protected:  virtual void        OnMoved(const Point& location) { }
protected:  virtual void        OnResized(const Size& newSize) { }
protected:  virtual void        OnMouseMoved(MouseEventFlags flags, const Point& location) { }
protected:  virtual void        OnMouseButtonDown(MouseEventFlags flags, const Point& location) { }
protected:  virtual void        OnMouseButtonUp(MouseEventFlags flags, const Point& location) { }
protected:  virtual void        OnMouseButtonDoubleClicked(MouseEventFlags flags, const Point& location) { }
protected:  virtual void        OnMouseEnter() { }
protected:  virtual void        OnMouseExit() { }
protected:  virtual void        OnGainedFocus() { }
protected:  virtual void        OnLostFocus() { }
protected:  virtual void        OnThemeChanged();
protected:  virtual void        OnFontChanged() { }
protected:  virtual void        OnMeasureItem(MEASUREITEMSTRUCT* measureItemStruct) { }
protected:  virtual void        OnParentChanged(Window* oldParent, Window* newParent) { }
protected:  virtual void        OnCaptionChanged() { }
protected:  virtual void        OnEraseBackground(HDC hdc);
protected:  virtual void        OnPaint();

            // window proc related
protected:  virtual LRESULT             PreWindowProc(UINT message, WPARAM wParam, LPARAM lParam, bool& discardMessage);
protected:  void                        PostWindowProcBase(UINT message, WPARAM wParam, LPARAM lParam);
protected:  virtual void                PostWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
protected:  virtual LRESULT             WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
protected:  LRESULT                     DefaultWindowProc(UINT message, WPARAM wParam, LPARAM lParam);
protected:  static LRESULT CALLBACK     StaticWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

            // misc
protected:  virtual void        PaintToHDC(HDC hdc, const Rect& rect);
protected:  void                SuppressSignal(SignalBase& signal);
protected:  static bool         WindowHasParent(Window* window);

    // instance data
protected:  WindowProcFunc defaultWindowProc;
protected:  HWND windowHandle;
protected:  HBRUSH backgroundBrush;
protected:  FontRef font;
protected:  MenuRef menu;
protected:  ColorRef backgroundColor;
protected:  bool usesDefaultFont;
protected:  SignalList suppressedSignals;
    // class data
protected:  static HandleToWindowMap sHandleToWindowMap;
protected:  static WindowList sAllChildWindows;
protected:  static FontRef sDefaultFont;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
