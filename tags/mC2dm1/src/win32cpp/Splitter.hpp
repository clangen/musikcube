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

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Container.hpp>
#include <win32cpp/ILayout.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/*! */
enum SplitDirection
{
    /*! */ SplitRow,
    /*! */ SplitColumn
};

/*! */
enum AnchorDirection
{
    /*! */ AnchorTop,
    /*! */ AnchorBottom,
    /*! */ AnchorLeft,
    /*! */ AnchorRight
};

///\brief Used to disable anchor constraints in Splitter
///\see Splitter::SetAnchorConstraints
const int DisableConstraint = -1;

///\brief A Container with two children that can be resized with the mouse.
///
///Splitter works with any two Window derived instances. Adding more
///than two children will result in a TooManyChildWindowsException.
///
///Splitter will automatically expand its children
///
///\code
///Button* button1 = new Button(_T("button1"));
///Button* button2 = new Button(_T("button2"));
///Splitter* splitter = new Splitter(SplitColumn, button1, button2);
///Application::Instance().MainWindow()->AddChild(splitter);
///splitter->Resize(400, 300);
///\endcode
///
///Typically when a Splitter is resized it's preferable to keep one of the
///children stationary or "anchored," while allowing the other to be resized
///to fill the new space. The anchor can be set via Splitter::SetAnchor by
///specifying a win32cpp::AnchorDirection.
class Splitter: public Panel, public ILayout
{
private: // types
    typedef Panel base;

public: // constructors
    /*ctor*/            Splitter(SplitDirection direction, Window* child1 = NULL, Window* child2 = NULL);

public: // methods
    bool                IsSizable();
    void                SetSizable(bool sizable);
    Window*             SetChild1(Window* window);
    const Window*       Child1();
    Window*             SetChild2(Window* window);
    const Window*       Child2();
    AnchorDirection     Anchor();
    void                SetAnchor(AnchorDirection newAnchor);
    void                AnchorConstraints(int& minSize, int& maxSize);
    void                SetAnchorConstraints(int minSize = DisableConstraint, int maxSize = DisableConstraint);
    void                SetAnchorSize(int anchorSize);
    void                SetGripperSize(int pixels);

protected: // methods
    void    SetSizeCursor();
    int     AnchorSizeFromMouse(int splitPosition, const Size& newSize);
    void    BeginMouseCapture();
    void    EndMouseCapture();

    // overrides
    virtual bool        AddChildWindow(Window* window);
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void        Layout();
    virtual void        LayoutFromMouse();
    virtual void        OnResized(const Size& newSize);
    virtual void        OnMouseMoved(MouseEventFlags flags, const Point& location);
    virtual void        OnMouseButtonDown(MouseEventFlags flags, const Point& location);
    virtual void        OnMouseButtonUp(MouseEventFlags flags, const Point& location);
    virtual void        OnMouseExit();
    virtual void        OnMouseEnter();
    virtual void        OnCreated();

private: // instance data
    Window *child1, *child2;
    Frame *child1Frame, *child2Frame;
    int gripperSize, anchorSize;
    int minAnchorSize, maxAnchorSize;
    int sizeFromMouse;
    bool isDragging, isSizable;
    SplitDirection direction;
    AnchorDirection anchor;
    Rect splitRect;

private: // class data
    static HCURSOR sHSizeCursor, sVSizeCursor, sArrowCursor;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
