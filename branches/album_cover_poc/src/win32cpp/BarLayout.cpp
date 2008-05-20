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

#include "pch.hpp"
#include <win32cpp/BarLayout.hpp>

#include <algorithm>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param orientation
///A win32cpp::BarLayoutOrientation.
/*ctor*/    BarLayout::BarLayout(BarLayoutOrientation orientation)
: Panel()
, orientation(orientation)
, spacing(4)
, childIsResizing(false)
, constraints(LayoutFillParent, LayoutFillParent)
{
}

///\brief
///Sets the amount spacing between children, in pixels.
///
///\param spacing
///The amount of spacing, in pixels.
void        BarLayout::SetSpacing(int spacing)
{
    if (this->spacing == spacing)
    {
        return;
    }

    this->spacing = spacing;
    this->Layout();
}

///\brief
///Returns the amount of spacing between children (in pixels)
int         BarLayout::Spacing() const
{
    return this->spacing;
}

void        BarLayout::OnChildAdded(Window* newChild)
{
    newChild->Resized.connect(this, &BarLayout::OnChildResized);
    this->Layout();
}

void        BarLayout::OnChildRemoved(Window* oldChild)
{
    oldChild->Resized.disconnect(this);
    this->Layout();
}

void        BarLayout::OnChildResized(Window* window, Size newSize)
{
    if ( ! this->childIsResizing)
    {
        this->Layout();
    }
}

inline void BarLayout::ThrowIfNotChild(Window* child)
{
    WindowList::iterator it =
        std::find(this->childWindows.begin(), this->childWindows.end(), child);

    if (it == this->childWindows.end())
    {
        throw InvalidChildWindowException();
    }
}

void        BarLayout::OnResized(const Size& newSize)
{
    this->Layout();
}

void        BarLayout::SetSizeConstraints(int width , int height)
{
    constraints.width = width;
    constraints.height = height;

    this->Layout();
}

void        BarLayout::Layout()
{
    if ( ! this->childWindows.size())
    {
        return;
    }

    RedrawLock stopRedraw(this);

    // calculate the total available size
    Size layoutSize = this->WindowSize();
    //
    if ((this->constraints.width == LayoutFillParent)
    || (this->constraints.height == LayoutFillParent))
    {
        Window* parent = this->Parent();

        if ( ! parent)
        {
            return;
        }

        Size parentSize = parent->WindowSize();

        this->constraints.width == LayoutFillParent
            ? layoutSize.width = parentSize.width : 0;

        this->constraints.height == LayoutFillParent
            ? layoutSize.height = parentSize.height : 0;
    }

    // calculate the size for each child
    bool isColumnLayout = (this->orientation == BarColumnLayout);
    Size childSize;
    int childCount = (int) this->childWindows.size();
    int spacing = this->spacing;
    //
    if (isColumnLayout)
    {
        // column layout means height is always the same
        childSize.height = layoutSize.height;
        childSize.width = (layoutSize.width - ((childCount - 1) * spacing)) / childCount;
    }
    else
    {
        // row layout means width is always the same
        childSize.width = layoutSize.width;
        childSize.height = (layoutSize.height - ((childCount - 1) * spacing)) / childCount;
    }

    // iterate over all the children and lay them out!
    int currentPosition = 0;
    WindowList::iterator it = this->childWindows.begin();
    WindowList::iterator end = this->childWindows.end();
    for ( ; it != end; it++)
    {
        Window* current = (*it);

        this->childIsResizing = true;

        // last child may have a slightly different size due to integer
        // rounding errors in the childSize calculations above.
        if ((it + 1) == end)
        {
            if (isColumnLayout) childSize.width = (layoutSize.width - currentPosition);
            else childSize.height = (layoutSize.height - currentPosition);
        }

        // resize        
        current->Resize(childSize);

        // reposition
        Point location(
            isColumnLayout ? currentPosition : 0,
            isColumnLayout ? 0 : currentPosition);

        current->MoveTo(location);

        this->childIsResizing = false;

        currentPosition += isColumnLayout ? childSize.width : childSize.height;
        currentPosition += spacing;
    }
}
