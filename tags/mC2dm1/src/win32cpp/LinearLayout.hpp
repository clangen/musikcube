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

#include <win32cpp/Panel.hpp>
#include <win32cpp/ILayout.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief Specifies the orientation of a LinearLayout
enum LinearLayoutOrientation
{
    LinearRowLayout,    /*!< */
    LinearColumnLayout   /*!< */
};

///\brief A Container that is used to stack controls horizontally or
///vertically.
///
///Children are added to and removed from a LinearLayout using
///Container::AddChild and Container::RemoveChild, respectively. The order
///in which the children are arranged is determined by the order in
///which they are added to the Container. First in, first out.
///
///The orientation (LinearRowLayout or LinearColumnLayout) is determined at 
///construction time by specifying a win32cpp::LinearLayoutOrientation.
///
///If a child is resized, LinearLayout will adjust its internal layout and sze.
///
///\code
/// // create a new Horizontal LinearLayout with 4 buttons.
/// LinearLayout* layout = new LinearLayout(LinearColumnLayout);
/// layout->AddChild(new Button(_T("Prev")));
/// layout->AddChild(new Button(_T("Play")));
/// layout->AddChild(new Button(_T("Stop")));
/// layout->AddChild(new Button(_T("Next")));
///\endcode
///
///Children specified as "filled" may be automatically resized in the direction
///opposite of the LinearLayout's orientation. Setting a child's "Fill" property
///can is done via LinearLayout::SetChildFill. Consider the following illustration
///of a horizontal LinearLayout with 4 children:
///
///\code
///|-----------------------------------------------| <-- Column LinearLayout
///|                                  |-----------||
///||--------|                        |           ||
///|| Child1 ||----------||----------||  Child4   ||
///||        ||  Child2  ||  Child3  ||           ||
///||--------||----------||----------||-----------||
///|-----------------------------------------------|
///\endcode
///
///Specifying Child2 as "Filled" will make the layout look as follows:
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
///A LinearLayout can optionally include a single "flexible" child. The flexible child
///will grow or shrink if the LinearLayout is resized manually by the user. Consider
///the following example where a LinearLayout that has been resized:
///
///\code
///|------------------------------------------------------------| <-- Column LinearLayout
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
///Child controls can also be aligned in a BoxLayout via LinearLayout::SetChildAlignment
///and win32cpp::ChildAlignment
///
///\code
///Column LinearLayout alignment styles
///|--------------------|
///|   ChildAlignTop    |
///|                    |
///|  ChildAlignMiddle  |
///|                    |
///|  ChildAlignBottom  |
///|--------------------|
///
///Row LinearLayout alignment styles
///|---------------------------------------------------------|
///|                                                         |
///| ChildAlignLeft     ChildAlignCenter     ChildAlignRight |
///|                                                         |
///|---------------------------------------------------------|
///\endcode
class LinearLayout: public Panel, public ILayout
{
private: // types
    typedef Panel base;
    typedef std::map<Window*, bool> ChildFillMap;
    typedef std::map<Window*, Size> ChildSizeMap;
    typedef std::map<Window*, ChildAlignment> ChildAlignmentMap;

public: // constructors
    /*ctor*/    LinearLayout(LinearLayoutOrientation orientation = LinearRowLayout);

public: // methods
    void    SetSpacing(int spacing);
    int     Spacing() const;
    void    SetChildFill(Window* child, bool enabled = true);
    void    SetDefaultChildFill(bool enabled = true);
    void    SetFlexibleChild(Window* child);
    void    SetChildAlignment(Window* child, ChildAlignment alignment);
    void    SetDefaultChildAlignment(ChildAlignment alignment);
    void    SetSizeConstraints(int width = LayoutFillParent, int height = LayoutFillParent);

public: // ILayout
    virtual void Layout();

protected: // methods
    void            OnChildAdded(Window* newChild);
    void            OnChildRemoved(Window* oldChild);
    void            OnChildResized(Window* window, Size newSize);
    Size            ChildFillSize(Window* child);
    Size            CheckedChildFillSize(Window* child);
    void            ThrowIfNotChild(Window* child);
    bool            ShouldFillChild(Window* child);
    Point           AlignChildInRect(ChildAlignment alignment, Size childSize, Rect alignmentRect);
    ChildAlignment  AlignmentForChild(Window* child);
    virtual void    OnResized(const Size& newSize);

private: // instance data
    LinearLayoutOrientation orientation;
    int spacing;
    ChildFillMap childFillMap;
    ChildSizeMap childSizeMap;
    bool childIsResizing, defaultChildFill;
    Window* flexibleChild;
    ChildAlignmentMap childAlignment;
    ChildAlignment defaultChildAlignment;
    Size layoutSize, constraints;
};

/////////////////////////////////////////////////////////////////////////////

}   // win32cpp