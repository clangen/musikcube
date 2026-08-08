//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
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

/**
 * @file Window.hpp
 * @brief Abstract base class for every Win32 control in win32cpp.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Window is a
 * thin C++ object wrapper around an HWND that follows the
 * observer/notifier pattern via sigslot signals. It also provides
 * automatic double buffering (through MemoryDC and RedrawLock) so
 * controls do not flicker, and it stabilizes mouse enter/exit events.
 */

#pragma once


#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Types.hpp>
#include <win32cpp/Exception.hpp>
#include <win32cpp/Font.hpp>
#include <win32cpp/Menu.hpp>
#include <win32cpp/WindowPadding.hpp>

#include <vector>
#include <map>
#include <set>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class Window;

///\brief Flags used for control layout
enum LayoutFlags
{
    LayoutFillParent = -1,      /*!< Width/height fills the parent container */
    LayoutWrapContent = -2,     /*!< Width/height wraps the control's content */
    LayoutWrapWrap = -3,        /*!< Wraps content on both axes */
    LayoutFillWrap = -4,        /*!< Fills parent, wraps content */
    LayoutWrapFill = -5,        /*!< Wraps content, fills parent */
    LayoutFillFill = -6         /*!< Fills the parent on both axes */
};

///\brief Specifies the alignment of a Layout's child
enum LayoutAlignFlag
{
    LayoutAlignLeft = 0,     /*!< Aligns children to the left */
    LayoutAlignRight = 1,    /*!< Aligns children to the right */
    LayoutAlignCenter = 2,   /*!< Centers children horizontally */
    LayoutAlignTop = 0,      /*!< Aligns children to the top */
    LayoutAlignBottom = 1,   /*!< Aligns children to the bottom */
    LayoutAlignMiddle = 2,   /*!< Centers children vertically */
};

///\brief Flags used for mouse related events emitted by Window.
///\details Values mirror the Win32 MK_* mouse-key state flags.
enum MouseEventFlags
{
    MouseShiftKey = MK_SHIFT,       /*!< Shift key is down */
    MouseCtrlKey = MK_CONTROL,      /*!< Control key is down */
    MouseLeftButton = MK_LBUTTON,   /*!< Left mouse button is down */
    MouseMiddleButton = MK_MBUTTON, /*!< Middle mouse button is down */
    MouseRightButton = MK_RBUTTON,  /*!< Right mouse button is down */
    MouseX1Button = MK_XBUTTON1,    /*!< First X (back) mouse button is down */
    MouseX2Button = MK_XBUTTON2     /*!< Second X (forward) mouse button is down */
};

/** @brief Identifies a virtual key (Win32 VK_* value). */
typedef DWORD VirtualKeyCode;
/** @brief Flags describing keyboard modifier/state (Win32 keystroke flags). */
typedef DWORD KeyEventFlags;

/** @brief Signal emitted when a Window is about to be destroyed. */
typedef sigslot::signal1<Window*> DestroyedEvent;
/** @brief Signal emitted once a Window's HWND has been created. */
typedef sigslot::signal1<Window*> CreatedEvent;
/** @brief Signal emitted when a Window is moved to a new location. */
typedef sigslot::signal2<Window*, Point> MovedEvent;
/** @brief Signal emitted when a Window is resized. */
typedef sigslot::signal2<Window*, Size> ResizedEvent;
/** @brief Signal emitted when the cursor enters the Window. */
typedef sigslot::signal1<Window*> MouseEnterEvent;
/** @brief Signal emitted when the cursor leaves the Window. */
typedef sigslot::signal1<Window*> MouseExitEvent;
/** @brief Signal emitted when the cursor moves over the Window. */
typedef sigslot::signal3<Window*, MouseEventFlags, Point> MouseMovedEvent;
/** @brief Signal emitted on mouse button press/release/double-click. */
typedef sigslot::signal3<Window*, MouseEventFlags, Point> MouseButtonEvent;
/** @brief Signal emitted when the desktop theme changes. */
typedef sigslot::signal1<Window*> ThemeChangedEvent;
/** @brief Signal emitted when a Window gains or loses focus. */
typedef sigslot::signal1<Window* /*this*/> FocusEvent;
/** @brief Signal emitted when a Window's parent changes. */
typedef sigslot::signal2<Window* /*old*/, Window* /*new*/> ParentChangedEvent;
/** @brief Signal emitted when a timer associated with the Window times out. */
typedef sigslot::signal1<unsigned int> TimerEvent;
/** @brief Signal emitted for keyboard key press/release events. */
typedef sigslot::signal3<Window*, VirtualKeyCode, KeyEventFlags> KeyEvent;
/** @brief Signal emitted when a Window's visibility changes. */
typedef sigslot::signal2<Window*, bool> VisibilityChangedEvent;
/** @brief Signal emitted when a Window's layout parameters change. */
typedef sigslot::signal1<Window*> LayoutParamsChangedEvent;

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
    ///\brief
    ///This exception is thrown if an invalid (negative) LayoutWeight is specified.
    class InvalidLayoutWeightException: public Exception { };
    ///\brief
    ///This exception is thrown if invalid layout flags are specified.
    class InvalidLayoutFlagsException: public Exception { };

protected: // types
    typedef WNDPROC WindowProcFunc;
    typedef std::vector<Window*> WindowList;
    typedef std::map<HWND, Window*> HandleToWindowMap;
    typedef sigslot::_signal_base<sigslot::SIGSLOT_DEFAULT_MT_POLICY> SignalBase;
    typedef std::set<SignalBase*> SignalList;
    typedef std::unique_ptr<Color> ColorRef;

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
    ///\brief Emitted when a Window's layout parameters have changed.
    LayoutParamsChangedEvent    LayoutParametersChanged;

public: // ctor, dtor
    /*ctor*/            Window();
    /*ctor*/            Window(LayoutFlags layoutFlags);
    /*dtor*/ virtual    ~Window();

public: // methods
    ///\brief Creates the underlying HWND, attaching it to the given parent.
    ///\param parent the parent window, or NULL for a top-level window
    ///\note Windows are lazily created; this is called by layouts automatically.
    void            Initialize(Window* parent = NULL);
    ///\brief Shows or hides the Window using a Win32 show command.
    ///\param showCommand a SW_* value (e.g. SW_SHOW, SW_HIDE)
    ///\return true if the window was successfully shown
    bool            Show(int showCommand);
    ///\brief Moves the Window's top-left corner to the given coordinates.
    ///\param x the new left coordinate in pixels
    ///\param y the new top coordinate in pixels
    ///\return true on success
    bool            MoveTo(int x, int y);
    ///\brief Moves the Window relative to its current position.
    ///\param x the horizontal offset in pixels
    ///\param y the vertical offset in pixels
    ///\return true on success
    bool            MoveRelativeTo(int x, int y);
    ///\brief Moves the Window to the given location.
    ///\param location the new position of the top-left corner
    ///\return true on success
    bool            MoveTo(const Point& location);
    ///\brief Resizes the Window to the given width and height.
    ///\param width the new width in pixels
    ///\param height the new height in pixels
    ///\return true on success
    bool            Resize(int width, int height);
    ///\brief Resizes the Window to the given size.
    ///\param size the new size
    ///\return true on success
    bool            Resize(const Size& size);
    ///\brief Sets both the position and size of the Window in one call.
    ///\param rectangle the new bounds of the Window
    ///\return true on success
    bool            SetRectangle(const Rect& rectangle);
    ///\brief Enables or disables the Window for user input.
    ///\param enable true to enable, false to disable
    ///\return true on success
    bool            Enable(bool enable);
    ///\brief Returns the Window's caption (title/text).
    ///\return the current caption string
    uistring        Caption() const;
    ///\brief Sets the Window's caption (title/text).
    ///\param caption the new caption string
    ///\return true on success
    bool            SetCaption(const uistring& caption);
    ///\brief Returns the Window's client area rectangle.
    ///\return the client rect in screen coordinates
    Rect            ClientRect() const;
    ///\brief Returns the Window's outer rectangle including borders.
    ///\return the window rect in screen coordinates
    Rect            WindowRect() const;
    ///\brief Returns the Window's outer size including borders.
    ///\return the window size in pixels
    Size            WindowSize() const;
    ///\brief Returns the Window's client area size.
    ///\return the client size in pixels
    virtual Size    ClientSize() const;
    ///\brief Returns the Window's position relative to its parent.
    ///\return the location of the top-left corner
    Point           Location() const;
    ///\brief Returns the current cursor position in client coordinates.
    ///\return the cursor position within the Window
    Point           CursorPosition() const;
    ///\brief Sends a Win32 message to the Window and waits for the result.
    ///\param message the window message identifier
    ///\param wParam the WPARAM payload
    ///\param lParam the LPARAM payload
    ///\return the LRESULT returned by the window procedure
    LRESULT         SendMessage(UINT message, WPARAM wParam, LPARAM lParam);
    ///\brief Posts a Win32 message to the Window without waiting.
    ///\param message the window message identifier
    ///\param wParam the WPARAM payload
    ///\param lParam the LPARAM payload
    ///\return true if the message was posted successfully
    bool            PostMessage(UINT message, WPARAM wParam, LPARAM lParam);
    ///\brief Returns the underlying Win32 window handle.
    ///\return the HWND of this Window
    HWND            Handle() const;
    ///\brief Destroys the underlying HWND.
    ///\return true if the window was destroyed
    virtual bool    Destroy();
    ///\brief Returns the Window's parent.
    ///\return pointer to the parent Window, or NULL
    Window*         Parent() const;
    ///\brief Repaints the Window immediately.
    void            Redraw() const;
    ///\brief Sets the font used by the Window.
    ///\param font the shared font to apply
    void            SetFont(FontRef font);
    ///\brief Returns the font currently used by the Window.
    ///\return the shared font, or the system default
    FontRef         Font() const;
    ///\brief Sets the menu associated with the Window.
    ///\param menu the menu to attach
    void            SetMenu(MenuRef menu);
    ///\brief Returns the menu associated with the Window.
    ///\return the shared menu, or NULL
    MenuRef         Menu();
    ///\brief Sets the Window's visibility.
    ///\param visible true to show the Window
    void            SetVisible(bool visible = true);
    ///\brief Returns whether the Window is currently visible.
    ///\return true if the Window is visible
    bool            Visible();
    ///\brief Sets the Window's background brush color.
    ///\param color the background color to apply
    void            SetBackgroundColor(const Color& color);
    ///\brief Returns the Window's background color.
    ///\return the background color
    Color           BackgroundColor() const;
    ///\brief Attempts to give keyboard focus to the Window.
    ///\return true if focus was granted
    bool            SetFocus();
    ///\brief Returns whether the Window is a tab stop.
    ///\return true if the Window participates in tab navigation
    bool            TabStop();
    ///\brief Enables or disables tab-stop participation.
    ///\param enabled true to make the Window a tab stop
    void            SetTabStop(bool enabled);
    ///\brief Returns the horizontal layout flag.
    ///\return the LayoutWidth flag
    LayoutFlags     LayoutWidth() const;
    ///\brief Returns the vertical layout flag.
    ///\return the LayoutHeight flag
    LayoutFlags     LayoutHeight() const;
    ///\brief Returns the alignment used within its parent layout.
    ///\return the alignment flag
    LayoutAlignFlag LayoutAlignment() const;
    ///\brief Sets the width and height layout flags.
    ///\param widthFlag the horizontal sizing flag
    ///\param heightFlag the vertical sizing flag
    void            SetLayoutFlags(LayoutFlags widthFlag, LayoutFlags heightFlag);
    ///\brief Sets both layout flags to the same value.
    ///\param flags the layout flag to apply on both axes
    void            SetLayoutFlags(LayoutFlags flags);
    ///\brief Sets the alignment of the Window in its parent layout.
    ///\param alignment the alignment flag to apply
    void            SetLayoutAlignment(LayoutAlignFlag alignment);
    ///\brief Returns the weight used to distribute extra layout space.
    ///\return the weight factor
    float           LayoutWeight() const;
    ///\brief Sets the weight used to distribute extra layout space.
    ///\param weight the weight factor (>= 0)
    void            SetLayoutWeight(float weight);

    ///\brief Looks up the Window wrapper for a given HWND.
    ///\param hwnd the raw handle to resolve
    ///\return pointer to the associated Window, or NULL if not found
    static Window*  SubclassedWindowFromHWND(HWND hwnd);

public: // operators
    operator bool() { return (this->windowHandle != NULL); }

protected: // methods
    // pure virtuals
    virtual HWND        Create(Window* parent = NULL) = 0;

    // subclassing, mouse capture
    static void         SubclassWindowProc(Window* window);
    static void         UnSubclassWindowProc(Window* window);
    static bool         IsWindowSubclassed(Window* window);
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
    void    OnLayoutParametersChangedBase();

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
    virtual void    OnLayoutParametersChanged() { }
    virtual bool    OnKeyDown(VirtualKeyCode keyCode, KeyEventFlags flags) { return false; }
    virtual bool    OnKeyUp(VirtualKeyCode keyCode, KeyEventFlags flags) { return false; }
    virtual bool    OnChar(VirtualKeyCode keyCode, KeyEventFlags flags) { return false; }
    virtual void    OnEraseBackground(HDC hdc);
    virtual void    OnPaint();
    virtual void    OnRequestFocusPrev();
    virtual void    OnRequestFocusNext();
    virtual HBRUSH  OnControlColor(HDC hdc);
    virtual void    OnVisibilityChanged(bool visible) { }

    virtual LRESULT DrawItem(DRAWITEMSTRUCT& item) { return 0; }

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
    static bool     WindowIsValid(Window* window);

private:
    void InitializeInstance();

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
    LayoutAlignFlag layoutAlignment;
    LayoutFlags layoutWidth, layoutHeight;
    float layoutWeight;

protected: // class data
    static HandleToWindowMap sHandleToWindowMap;
    static WindowList sAllChildWindows;
    static FontRef sDefaultFont;
    static FocusDirection sFocusDirection;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp
