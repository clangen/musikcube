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
#include <win32cpp/BoxLayout.hpp>

#include <algorithm>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param orientation
///A win32cpp::BoxOrientation.
/*ctor*/    BoxLayout::BoxLayout(BoxOrientation orientation)
: base()
, orientation(orientation)
, spacing(4)
, childIsResizing(false)
, flexibleChild(NULL)
, autoResize(false)
, defaultChildFill(false)
, defaultChildAlignment(ChildAlignCenter)
{
}

///\brief
///Sets the amount spacing between children, in pixels.
///
///\param spacing
///The amount of spacing, in pixels.
void        BoxLayout::SetSpacing(int spacing)
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
int         BoxLayout::Spacing() const
{
    return this->spacing;
}

void        BoxLayout::OnChildAdded(Window* newChild)
{
    this->windowOrder.push_back(newChild);
    newChild->Resized.connect(this, &BoxLayout::OnChildResized);

    this->Layout();

    if (autoResize)
    {
        this->ResizeToMinimumSize();
    }
}

void        BoxLayout::OnChildRemoved(Window* oldChild)
{
    oldChild->Resized.disconnect(this);

    this->windowOrder.erase(
        std::find(this->windowOrder.begin(), this->windowOrder.end(), oldChild));

    this->Layout();

    if (autoResize)
    {
        this->ResizeToMinimumSize();
    }
}

void        BoxLayout::OnChildResized(Size newSize)
{
    if ( ! this->childIsResizing)
    {
        this->Layout();
        this->MinimumSizeChanged(this->MinimumSize());

        if (autoResize)
        {
            this->ResizeToMinimumSize();
        }
    }
}

void        BoxLayout::OnResized(const Size& newSize)
{
    this->Layout();
}

inline void BoxLayout::ThrowIfNotChild(Window* child)
{
    if (this->childWindows.find(child) == this->childWindows.end())
    {
        throw InvalidChildWindowException();
    }
}

inline bool BoxLayout::ShouldFillChild(Window* child)
{
    return (this->defaultChildFill || this->fillWindows.find(child) != this->fillWindows.end());
}

Size        BoxLayout::ChildFillSize(Window* child)
{
    Size minSize = this->ClientSize();
    Size childSize = child->WindowSize();

    this->orientation == BoxHorizontal
        ? childSize.height = minSize.height
        : childSize.width = minSize.width;

    return childSize;
}

Size        BoxLayout::FlexibleChildSize()
{
    Size flexSize;

    this->ShouldFillChild(this->flexibleChild)
        ? flexSize = this->ChildFillSize(this->flexibleChild)
        : flexSize = this->flexibleChild->WindowSize();

    Size minSize = this->MinimumSize(true);
    Size thisSize = this->WindowSize();

    this->orientation == BoxHorizontal
        ? flexSize.width = (thisSize.width - minSize.width)
        : flexSize.height = (thisSize.height - minSize.height);

    return flexSize;
}

///\brief
///Returns the smallest Size the BoxLayout can occupy hiding
///any children.
Size        BoxLayout::MinimumSize()
{
    return this->MinimumSize(true);
}

Size        BoxLayout::MinimumSize(bool includeFilled)
{
    int totalHeight = 0, totalWidth = 0;
    bool isHoriz = (this->orientation == BoxHorizontal);

    OrderedWindowList::iterator it = this->windowOrder.begin();
    for ( ; it != this->windowOrder.end(); it++)
    {
        Size windowSize = (*it)->WindowSize();

        if (( ! includeFilled) && this->ShouldFillChild(*it))
        {
            continue;
        }

        if (isHoriz)
        {
            if (*it != this->flexibleChild) totalWidth += windowSize.width;
            totalHeight = max(totalHeight, windowSize.height);
        }
        else
        {
            if (*it != this->flexibleChild) totalHeight += windowSize.height;
            totalWidth = max(totalWidth, windowSize.width);
        }
    }

    int totalSpacing = (this->spacing * ((int) this->windowOrder.size() - 1));
    isHoriz ? totalWidth += totalSpacing : totalHeight += totalSpacing;

    return Size(totalWidth, totalHeight);
}

///\brief
///Sets whether or not child controls are filled by default
///
///\param enabled
///Whether or not to fill child controls by default
///
///\see
///BoxLayout::SetChildFill
void        BoxLayout::SetDefaultChildFill(bool enabled)
{
    if (enabled != this->defaultChildFill)
    {
        this->defaultChildFill = enabled;
        this->Layout();
    }
}

///\brief
///Resizes the BoxLayout to its smallest possible size without clipping
///any child controls.
void        BoxLayout::ResizeToMinimumSize()
{
    this->Resize(this->MinimumSize(false));
}

///\brief
///Sets whether or not the child will be automatically resized in the
///direction opposite the orientation. See BoxLayout for an example.
///
///\param child
///The child whose property to modify
///
///\param enabled
///Whether or not to enable filling
//
///\throws Container::InvalidChildWindowException
///if the specified child is not a child of this BoxLayout.
void        BoxLayout::SetChildFill(Window* child, bool enabled)
{
    this->ThrowIfNotChild(child);

    if (enabled) this->fillWindows.insert(child);
    else this->fillWindows.erase(child);

    this->Layout();
}

///\brief
///Set the BoxLayout's flexible child.
///
///The flexible child will grow or shrink when the BoxLayout is resized.
///
///\param child
///The child to set as flexible.
///
///\throws Container::InvalidChildWindowException
///if the specified child is not a child of this BoxLayout.
void        BoxLayout::SetFlexibleChild(Window* child)
{
    if (this->flexibleChild == child)
    {
        return;
    }

    if ( ! child)
    {
        this->flexibleChild = NULL;
    }
    else
    {
        this->ThrowIfNotChild(child);
        this->flexibleChild = child;
    }

    this->Layout();
}

///\brief
///Set the alignment of the specified child within the BoxLayout.
///
///\param child
///The child to align.
///
///\param alignment
///The new alignment.
///
///\throws InvalidChildWindowException
///if child is not a child window
///
///\see
///ChildAlignment
void        BoxLayout::SetChildAlignment(Window* child, ChildAlignment alignment)
{
    this->ThrowIfNotChild(child);
    this->childAlignment[child] = alignment;
    this->Layout();
}

///\brief
///Sets the default alignment for child controls.
///
///\param alignment
///The new default alignment.
///
///\see
///ChildAlignment
void        BoxLayout::SetDefaultChildAlignment(ChildAlignment alignment)
{
    if (alignment != this->defaultChildAlignment)
    {
        this->defaultChildAlignment = alignment;
        this->Layout();
    }
}

///\brief
///Automatically resize the BoxLayout to the size reported by
///BoxLayout::MinimumSize when a child's size changes, or a child is
///added or removed.
///
///\param autoResize
///Specifies whether or not automatic resizing is enabled.
void        BoxLayout::SetAutoResizeToMinimumSize(bool autoResize)
{
    if (autoResize != this->autoResize)
    {
        this->autoResize = autoResize;
        this->ResizeToMinimumSize();
    }
}

ChildAlignment  BoxLayout::AlignmentForChild(Window* child)
{
    ChildAlignmentMap::iterator it = this->childAlignment.find(child);
    if (it == this->childAlignment.end())
    {
        return this->defaultChildAlignment;
    }

    return it->second;
}

void        BoxLayout::AlignChildInRect(Window* child, Rect rect)
{
    bool isHoriz = (this->orientation == BoxHorizontal);

    Rect childRect = child->WindowRect();
    childRect.location = rect.location;

    int rectSize = isHoriz ? rect.size.height : rect.size.width;
    int childSize = isHoriz ? childRect.size.height : childRect.size.width;

    int& location = isHoriz ? childRect.location.y : childRect.location.x;

    ChildAlignment alignment = this->AlignmentForChild(child);
    switch (alignment)
    {
    case ChildAlignCenter:      // && ChildAlignMiddle
        location += (rectSize - childSize) / 2;
        break;

    case ChildAlignRight:       // && ChildAlignBottom
        location += (rectSize - childSize);
        break;

    case ChildAlignLeft:        // && ChildAlignTop
    default:
        // don't need to do anything
        break;
    }

    child->MoveTo(childRect.location);
}

void        BoxLayout::Layout()
{
    RedrawLock stopRedraw(this);

    int currentX = 0, currentY = 0;
    int totalHeight = 0, totalWidth = 0;
    bool isHoriz = (this->orientation == BoxHorizontal);

    // loop through all of the children and stack them side by side
    // or top to bottom, depending on the orientation.
    bool needsResize = false;
    OrderedWindowList::iterator it = this->windowOrder.begin();
    for ( ; it != this->windowOrder.end(); it++)
    {
        needsResize = false;

        Size windowSize = (*it)->WindowSize();

        // "expand" the child if necessary. if a child filled AND flexible
        // this->FlexibleChildSize() will return the correct size.
        if (*it == this->flexibleChild)
        {
            windowSize = this->FlexibleChildSize();
            needsResize = true;
        }
        // fill the child, if necessary
        else if (this->ShouldFillChild(*it))
        {
            windowSize = this->ChildFillSize(*it);
            needsResize = true;
        }

        if (needsResize)
        {
            this->childIsResizing = true;
            (*it)->Resize(windowSize);
            this->childIsResizing = false;
        }

        Rect layoutRect(Point(currentX, currentY), this->ChildFillSize(*it));
        this->AlignChildInRect(*it, layoutRect);

        if (isHoriz)
        {
            currentX += (windowSize.width + this->spacing);

            totalWidth = currentX;
            totalHeight = max(totalHeight, windowSize.height);
        }
        else
        {
            currentY += (windowSize.height + this->spacing);

            totalHeight = currentY;
            totalWidth = max(totalWidth, windowSize.width);
        }
    }
}

