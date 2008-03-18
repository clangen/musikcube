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

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Container.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/*! */
enum SplitDirection
{
    /*! */ SplitVertical,
    /*! */ SplitHorizontal
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
///Splitter* splitter = new Splitter(SplitHorizontal, button1, button2);
///Application::Instance().MainWindow()->AddChild(splitter);
///splitter->Resize(400, 300);
///\endcode
///
///Typically when a Splitter is resized it's preferable to keep one of the
///children stationary or "anchored," while allowing the other to be resized
///to fill the new space. The anchor can be set via Splitter::SetAnchor by
///specifying a win32cpp::AnchorDirection.
class Splitter: public Panel
{
private:    typedef Panel base;

public:     /*ctor*/            Splitter(SplitDirection direction, Window* child1 = NULL, Window* child2 = NULL);

        // public api
public:     bool                IsSizable();
public:     void                SetSizable(bool sizable);
public:     Window*             SetChild1(Window* window);
public:     const Window*       Child1();
public:     Window*             SetChild2(Window* window);
public:     const Window*       Child2();
public:     AnchorDirection     Anchor();
public:     void                SetAnchor(AnchorDirection newAnchor);
public:     void                AnchorConstraints(int& minSize, int& maxSize);
public:     void                SetAnchorConstraints(int minSize = DisableConstraint, int maxSize = DisableConstraint);
public:     void                SetAnchorSize(int anchorSize);
public:     void                SetGripperSize(int pixels);

        // private api
protected:  void                SetSizeCursor();
protected:  Rect                SplitterRect();
protected:  int                 AnchorSizeFromMouse(int splitPosition, const Size& newSize);
protected:  void                BeginMouseCapture();
protected:  void                EndMouseCapture();
        // overrides
protected:  virtual bool        AddChildWindow(Window* window);
protected:  virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
protected:  virtual void        Layout(int child1Size = -1);
protected:  virtual void        OnResized(const Size& newSize);
protected:  virtual void        OnMouseMoved(MouseEventFlags flags, const Point& location);
protected:  virtual void        OnMouseButtonDown(MouseEventFlags flags, const Point& location);
protected:  virtual void        OnMouseButtonUp(MouseEventFlags flags, const Point& location);
protected:  virtual void        OnMouseExit();
protected:  virtual void        OnMouseEnter();
protected:  virtual void        OnCreated();

        // instance data
private:    Window *child1, *child2;
private:    Frame *child1Frame, *child2Frame;
private:    int gripperSize, anchorSize;
private:    int minAnchorSize, maxAnchorSize;
private:    bool isDragging, isSizable;
private:    SplitDirection direction;
private:    AnchorDirection anchor;
        // class data
private:    static HCURSOR sHSizeCursor, sVSizeCursor, sArrowCursor;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp