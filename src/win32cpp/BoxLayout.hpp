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

#include <win32cpp/Panel.hpp>
#include <win32cpp/Frame.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief Specifies the orientation of a BoxLayout
enum BoxOrientation
{
    BoxVertical,    /*!< */
    BoxHorizontal   /*!< */
};

///\brief Specifies the layout alignment of a BoxLayout's child
enum ChildAlignment
{
    ChildAlignLeft = 0,     /*!< */
    ChildAlignRight = 1,    /*!< */
    ChildAlignCenter = 2,   /*!< */
    ChildAlignTop = 0,      /*!< */
    ChildAlignBottom = 1,   /*!< */
    ChildAlignMiddle = 2,   /*!< */
};

/*! */
typedef sigslot::signal1<Size> MinimumSizeChangedEvent;

///\brief A Container that is used to stack controls horizontally or
///vertically.
///
///Children are added to and removed from a BoxLayout using
///Container::AddChild and Container::RemoveChild, respectively. The order
///in which the children are arranged is determined by the order in
///which they are added to the Container. First in, first out.
///
///The orientation (horizontal or vertical) is determined at construction
///time by specifying a win32cpp::BoxOrientation.
///
///If a child is resized, BoxLayout will adjust its internal layout and raise
///BoxLayout::MinimumSizeChanged.
///
///\code
/// // create a new Horizontal BoxLayout with 4 buttons. After adding
/// // the buttons resize the Box to its minimum size (the sum of all
/// // the child controls, plus spacing).
/// BoxLayout* boxLayout = new BoxLayout(BoxHorizontal);
/// boxLayout->AddChild(new Button(_T("Prev")));
/// boxLayout->AddChild(new Button(_T("Play")));
/// boxLayout->AddChild(new Button(_T("Stop")));
/// boxLayout->AddChild(new Button(_T("Next")));
/// boxLayout->ResizeToMinimumSize();
///\endcode
///
///Children specified as "filled" may be automatically resized in the direction
///opposite of the BoxLayout's orientation. Setting a child's "Fill" property
///can is done via BoxLayout::SetChildFill. Consider the following illustration
///of a horizontal BoxLayout with 4 children:
///
///\code
///|-----------------------------------------------| <-- Horizontal BoxLayout
///|                                  |-----------||
///||--------|                        |           ||
///|| Child1 ||----------||----------||  Child4   ||
///||        ||  Child2  ||  Child3  ||           ||
///||--------||----------||----------||-----------||
///|-----------------------------------------------|
///\endcode
///
///Specifying Child2 as "Filled" will make the layout looks as follows:
///
///\code
///|-----------------------------------------------|
///|          |----------|            |-----------||
///||--------||          |            |           ||
///|| Child1 ||  Child2  ||----------||  Child4   ||
///||        ||          ||  Child3  ||           ||
///||--------||----------||----------||-----------||
///|-----------------------------------------------|
///\endcode
///
///A BoxLayout can optionally include a single "flexible" child. The flexible child
///will grow or shrink if the BoxLayout is resized manually by the user. Consider
///the following example where a BoxLayout that has been resized:
///
///\code
///|------------------------------------------------------------| <-- Horizontal BoxLayout
///||--------||--------||--------||---------|                   |
///|| Child1 || Child2 || Child3 || Child4  |                   |
///||--------||--------||--------||---------|                   |
///|------------------------------------------------------------|
///\endcode
///
///Setting Child2 as "flexible" will result in the following layout:
///
///\code
///|------------------------------------------------------------|
///||--------||-------------------------||----------||---------||
///|| Child1 || Child2 (flexible)       ||  Child3  || Child4  ||
///||--------||-------------------------||----------||---------||
///|------------------------------------------------------------|
///\endcode
///
///Child controls can also be aligned in a BoxLayout via BoxLayout::SetChildAlignment
///and win32cpp::ChildAlignment
///
///\code
///Vertical BoxLayout alignment styles
///|--------------------|
///|   ChildAlignTop    |
///|                    |
///|  ChildAlignMiddle  |
///|                    |
///|  ChildAlignBottom  |
///|--------------------|
///
///Horizontal BoxLayout alignment styles
///|---------------------------------------------------------|
///|                                                         |
///| ChildAlignLeft     ChildAlignCenter     ChildAlignRight |
///|                                                         |
///|---------------------------------------------------------|
///\endcode
class BoxLayout: public Panel, public EventHandler
{
private:    typedef Panel base;
private:    typedef std::vector<Window*> OrderedWindowList;
private:    typedef std::map<Window*, ChildAlignment> ChildAlignmentMap;

public:     MinimumSizeChangedEvent MinimumSizeChanged;

public:     /*ctor*/    BoxLayout(BoxOrientation orientation = BoxHorizontal);

        // public api
public:     void        SetSpacing(int spacing);
public:     int         Spacing() const;
public:     Size        MinimumSize();
public:     void        ResizeToMinimumSize();
public:     void        SetChildFill(Window* child, bool enabled = true);
public:     void        SetDefaultChildFill(bool enabled = true);
public:     void        SetFlexibleChild(Window* child);
public:     void        SetChildAlignment(Window* child, ChildAlignment alignment);
public:     void        SetDefaultChildAlignment(ChildAlignment alignment);
public:     void        SetAutoResizeToMinimumSize(bool autoResize = true);

        // private api
protected:  void        Layout();
protected:  void        OnChildAdded(Window* newChild);
protected:  void        OnChildRemoved(Window* oldChild);
protected:  void        OnChildResized(Size newSize);
protected:  Size        ChildFillSize(Window* child);
protected:  Size        FlexibleChildSize();
protected:  Size        MinimumSize(bool includeFilled);
protected:  void        ThrowIfNotChild(Window* child);
protected:  bool        ShouldFillChild(Window* child);
protected:  void        AlignChildInRect(Window* child, Rect rect);
protected:  ChildAlignment  AlignmentForChild(Window* child);
        // overrides
protected:  virtual void    OnResized(const Size& newSize);

        // instance data
private:    BoxOrientation orientation;
private:    int spacing;
private:    OrderedWindowList windowOrder;
private:    WindowList fillWindows;
private:    bool childIsResizing, autoResize, defaultChildFill;
private:    Window* flexibleChild;
private:    ChildAlignmentMap childAlignment;
private:    ChildAlignment defaultChildAlignment;
};

/////////////////////////////////////////////////////////////////////////////

}   // win32cpp