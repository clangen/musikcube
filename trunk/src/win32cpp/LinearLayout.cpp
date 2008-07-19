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
#include <win32cpp/LinearLayout.hpp>
#include <win32cpp/RedrawLock.hpp>

#include <algorithm>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

///\brief
///Constructor.
///
///\param orientation
///A win32cpp::LinearLayout.
/*ctor*/    LinearLayout::LinearLayout(LinearLayoutOrientation orientation)
: base()
, orientation(orientation)
, spacing(4)
, childIsResizing(false)
, flexibleChild(NULL)
, defaultChildFill(true)
, defaultChildAlignment(ChildAlignLeft)
, constraints(LayoutFillParent, LayoutFillParent)
{
}

///\brief
///Sets the amount spacing between children, in pixels.
///
///\param spacing
///The amount of spacing, in pixels.
void        LinearLayout::SetSpacing(int spacing)
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
int         LinearLayout::Spacing() const
{
    return this->spacing;
}

void        LinearLayout::OnChildAdded(Window* newChild)
{
    newChild->Resized.connect(this, &LinearLayout::OnChildResized);

    // If the child being added is larger than the layout in the
    // direction opposite of the orientation, make the child's height
    // the new layout size.
    Size childSize = newChild->WindowSize();
    //
    if (this->orientation == LinearColumnLayout)
    {
        this->layoutSize.height = 
            max(this->layoutSize.height, childSize.height);
    }
    else
    {
        this->layoutSize.width = 
            max(this->layoutSize.width, childSize.width);
    }

    // keep track of the child's size. it may get changed when
    // its fill state changes, so remember its original size.
    this->childSizeMap[newChild] = childSize;

    this->Layout();
}

void        LinearLayout::OnChildRemoved(Window* oldChild)
{
    oldChild->Resized.disconnect(this);

     // don't care about its fill status anymore
    this->childFillMap.erase(oldChild);
    this->childSizeMap.erase(oldChild);

    this->Layout();
}

void        LinearLayout::OnChildResized(Window* window, Size newSize)
{
    if ( ! this->childIsResizing)
    {
        // if a child was added that causes the layout to expand in the direction
        // opposite of its orientation, then resize the layoutSize.
        if (this->orientation == LinearColumnLayout)
        {
            newSize.height > this->layoutSize.height
                ? this->layoutSize.height = newSize.height : 0;
        }
        else
        {
            newSize.width > this->layoutSize.width
                ? this->layoutSize.width = newSize.width : 0;
        }

        this->childSizeMap[window] = newSize;

        this->Layout();
    }
}

inline void LinearLayout::ThrowIfNotChild(Window* child)
{
    WindowList::iterator it =
        std::find(this->childWindows.begin(), this->childWindows.end(), child);

    if (it == this->childWindows.end())
    {
        throw InvalidChildWindowException();
    }
}

inline bool LinearLayout::ShouldFillChild(Window* child)
{
    ChildFillMap::iterator it = this->childFillMap.find(child);
    if (it != this->childFillMap.end())
    {
        return it->second;
    }

    return this->defaultChildFill;
}

Size        LinearLayout::CheckedChildFillSize(Window* child)
{
    if ( ! this->ShouldFillChild(child))
    {
        return this->childSizeMap[child];
    }

    return this->ChildFillSize(child);
}

Size        LinearLayout::ChildFillSize(Window* child)
{
    Size parentSize;
    Window* parent = this->Parent();
    parentSize = parent ? parent->WindowSize() : Size(0, 0);

    Size childSize = child->WindowSize();
    Size layoutSize = this->layoutSize;

    // apply constraints if LayoutFillParent is NOT specified
    if (this->constraints.width != LayoutFillParent)
    {
        layoutSize.width = constraints.width;
    }
    else if (this->orientation == LinearColumnLayout)
    {
        layoutSize.width = parentSize.width;
    }
    //
    if (this->constraints.height != LayoutFillParent)
    {
        layoutSize.height = constraints.height;
    }
    else if (this->orientation == LinearRowLayout)
    {
        layoutSize.height = parentSize.height;
    }

    (this->orientation == LinearColumnLayout)
        ? childSize.height = layoutSize.height
        : childSize.width = layoutSize.width;

    return childSize;
}

///\brief
///Sets whether or not child controls are filled by default
///
///\param enabled
///Whether or not to fill child controls by default
///
///\see
///LinearLayout::SetChildFill
void        LinearLayout::SetDefaultChildFill(bool enabled)
{
    if (enabled != this->defaultChildFill)
    {
        this->defaultChildFill = enabled;
        this->Layout();
    }
}

///\brief
///Sets whether or not the child will be automatically resized in the
///direction opposite the orientation. See LinearLayout for an example.
///
///\param child
///The child whose property to modify
///
///\param enabled
///Whether or not to enable filling
//
///\throws Container::InvalidChildWindowException
///if the specified child is not a child of this LinearLayout.
void        LinearLayout::SetChildFill(Window* child, bool enabled)
{
    this->ThrowIfNotChild(child);

    this->childFillMap[child] = enabled;

    this->Layout();
}

///\brief
///Set the LinearLayout's flexible child.
///
///The flexible child will grow or shrink when the LinearLayout is resized.
///
///\param child
///The child to set as flexible.
///
///\throws Container::InvalidChildWindowException
///if the specified child is not a child of this LinearLayout.
void        LinearLayout::SetFlexibleChild(Window* child)
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
///Set the alignment of the specified child within the LinearLayout.
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
void        LinearLayout::SetChildAlignment(Window* child, ChildAlignment alignment)
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
void        LinearLayout::SetDefaultChildAlignment(ChildAlignment alignment)
{
    if (alignment != this->defaultChildAlignment)
    {
        this->defaultChildAlignment = alignment;
        this->Layout();
    }
}

ChildAlignment  LinearLayout::AlignmentForChild(Window* child)
{
    ChildAlignmentMap::iterator it = this->childAlignment.find(child);
    if (it == this->childAlignment.end())
    {
        return this->defaultChildAlignment;
    }

    return it->second;
}

void        LinearLayout::OnResized(const Size& newSize)
{
    if (this->layoutSize != newSize)
    {
        this->layoutSize = newSize;
        this->Layout();
    }
}

Point        LinearLayout::AlignChildInRect(ChildAlignment alignment, Size childSize, Rect alignmentRect)
{
    bool isColumnLayout = (this->orientation == LinearColumnLayout);

    Point result = alignmentRect.location;

    int childLayoutSize = isColumnLayout
        ? childSize.height : childSize.width;

    int rectLayoutSize = isColumnLayout
        ? alignmentRect.size.height : alignmentRect.size.width;

    int& location = isColumnLayout
        ? result.y : result.x;

    switch (alignment)
    {
    case ChildAlignCenter:      // && ChildAlignMiddle
        location += (rectLayoutSize - childLayoutSize) / 2;
        break;

    case ChildAlignRight:       // && ChildAlignBottom
        location += (rectLayoutSize - childLayoutSize);
        break;

    case ChildAlignLeft:        // && ChildAlignTop
    default:
        // don't need to do anything
        break;
    }

    return result;
}

void        LinearLayout::SetSizeConstraints(int width , int height)
{
    constraints.width = width;
    constraints.height = height;

    this->Layout();
}

void        LinearLayout::Layout()
{
    if ( ! this->childWindows.size())
    {
        return;
    }

    RedrawLock stopRedraw(this);

    std::map<Window*, Size> childLayoutSizes;
    bool isColumnLayout = (this->orientation == LinearColumnLayout);
    int totalWidth = 0, totalHeight = 0;

    // iterate over all non-fill child windows and calculate their
    // final sizes and positions.
    WindowList::iterator it = this->childWindows.begin();
    for ( ; it != this->childWindows.end(); it++)
    {
        Window* current = (*it);

        Size finalSize = this->CheckedChildFillSize(current);

        childLayoutSizes[current] = finalSize;

        if (current != this->flexibleChild)
        {
            totalWidth += finalSize.width + this->spacing;
            totalHeight += finalSize.height + this->spacing;
        }
    }

    // apply constraints if LayoutFillParent is NOT specified
    if (this->constraints.width != LayoutFillParent)
    {
        totalWidth = constraints.width;
    }
    //
    if (this->constraints.height != LayoutFillParent)
    {
        totalHeight = constraints.height;
    }

    // figure out how big the flexible child should be, based on
    // the totalWidth and totalHeight calculated last step
    if (this->flexibleChild)
    {
        Size flexibleSize = childLayoutSizes[this->flexibleChild];

        if (isColumnLayout)
        {
            int width = this->layoutSize.width - totalWidth;
            flexibleSize.width = max(width, 0);
        }
        else
        {
            int height = this->layoutSize.height - totalHeight;
            flexibleSize.height = max(height, 0);
        }

        childLayoutSizes[this->flexibleChild] = flexibleSize;
    }

    // iterate over all the windows and actually lay them out
    int currentX = 0, currentY = 0;
    for (it = this->childWindows.begin(); it != this->childWindows.end(); it++)
    {
        Window* current = (*it);

        Size finalSize = childLayoutSizes[current];
        Point finalLocation = Point(0, 0);

        if (isColumnLayout)
        {
            finalLocation.x = currentX;
            currentX += finalSize.width + this->spacing;

            currentY = max(currentY, finalSize.height);
        }
        else
        {
            finalLocation.y = currentY;
            currentY += finalSize.height + this->spacing;

            currentX = max(currentX, finalSize.width);
        }

        this->childIsResizing = true;

        // resize if necessary
        Size currentSize = current->WindowSize();
        Size calculatedSize = childLayoutSizes[current];
        if (currentSize != calculatedSize)
        {
            current->Resize(calculatedSize);
        }

        // reposition
        finalLocation = this->AlignChildInRect(
            this->AlignmentForChild(current),
            calculatedSize,
            Rect(finalLocation, this->ChildFillSize(current)));

        current->MoveTo(finalLocation);

        this->childIsResizing = false;
    }

    (isColumnLayout ? currentX : currentY) -= this->spacing;
    Size newSize = Size(currentX, currentY);

    if (newSize != this->WindowSize())
    {
        this->Resize(newSize);
    }
}
