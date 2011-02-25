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

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Panel.hpp>
#include <win32cpp/ILayout.hpp>
#include <win32cpp/Application.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief Specifies the orientation of a LinearLayout
enum LayoutOrientation
{
    VerticalLayout,    /*!< */
    HorizontalLayout   /*!< */
};

///\brief A Container that is used to stack controls horizontally or
///vertically.
///
///Children are added to and removed from a LinearLayout using
///Container::AddChild and Container::RemoveChild, respectively. The order
///in which the children are arranged is determined by the order in
///which they are added to the Container. First in, first out.
///
///The orientation (VerticalLayout or HorizontalLayout) is determined at
///construction time by specifying a win32cpp::LinearLayoutOrientation.
///
///If a child is resized, LinearLayout will adjust itself accordingly.
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
///Children are positioned and sized according to their LayoutWidth(),
///LayoutHeight(), LayoutAlignment(), and LayoutWeight() properties.
///\endcode
class LinearLayout: public Panel, public ILayout
{
private: // types
    typedef Panel base;
    typedef std::map<Window*, Size> ChildSizeMap;

public: // constructors
    /*ctor*/    LinearLayout(
                    LayoutOrientation orientation,
                    LayoutFlags layoutFlags = LayoutWrapWrap);

public: // methods
    void SetSpacing(int spacing);
    int Spacing() const;

public: // ILayout
    virtual void Layout();

protected: // methods
    static bool RegisterWindowClass();
    virtual HWND Create(Window* parent);

    virtual void OnResized(const Size& newSize);
    void OnChildAdded(Window* newChild);
    void OnChildLayoutParametersChanged(Window* newChild);
    void OnChildRemoved(Window* oldChild);
    void OnChildResized(Window* window, Size newSize);
    void ThrowIfNotChild(Window* child);
    void ResizeWrapContent();
    Point AlignChildInRect(LayoutAlignFlag alignment, Size childSize, Rect alignmentRect);

private: // instance data
    LayoutOrientation orientation;
    int spacing;
    ChildSizeMap childSizeMap;
    bool childIsResizing, isResizing;
};

/////////////////////////////////////////////////////////////////////////////

}   // win32cpp
