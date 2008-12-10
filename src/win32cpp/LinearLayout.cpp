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
#include <limits>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

#define CLASS_NAME _T("LinearLayout")

///\brief
///Constructor.
///
///\param orientation
///The LinearLayout's orientation (LayoutHorizontal or LayoutVertical)
///
///\param layoutFlags
///The size flags to be used when positioning this control within a parent
/*ctor*/    LinearLayout::LinearLayout(LayoutOrientation orientation, LayoutFlags layoutFlags)
: base(layoutFlags)
, orientation(orientation)
, spacing(4)
, childIsResizing(false)
, isResizing(false)
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
    newChild->LayoutParametersChanged.connect(this, &LinearLayout::OnChildLayoutParametersChanged);
    this->childSizeMap[newChild] = newChild->WindowSize();

    this->ResizeWrapContent();
    this->Layout();
}

void        LinearLayout::OnChildRemoved(Window* oldChild)
{
    oldChild->Resized.disconnect(this);
    oldChild->LayoutParametersChanged.disconnect(this);
    oldChild->Resize(childSizeMap[oldChild]);

    this->childSizeMap.erase(oldChild);

    this->ResizeWrapContent();
    this->Layout();
}

void        LinearLayout::OnChildLayoutParametersChanged(Window* window)
{
    this->ResizeWrapContent();
    this->Layout();
}

void        LinearLayout::OnChildResized(Window* window, Size newSize)
{
    if ( ! this->childIsResizing)
    {
        this->childSizeMap[window] = newSize;
        this->ResizeWrapContent();
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

void        LinearLayout::OnResized(const Size& newSize)
{
    if ( ! this->isResizing)
    {
        this->Layout();
    }
}

void        LinearLayout::ResizeWrapContent()
{
    bool wrapWidth = this->LayoutWidth() == LayoutWrapContent;
    bool wrapHeight = this->LayoutHeight() == LayoutWrapContent;

    if (( ! wrapWidth) && ( ! wrapHeight))
    {
        return;
    }

    bool isVertical = (this->orientation == VerticalLayout);
    Size size = this->ClientSize();
    int wrappedWidth = 0, wrappedHeight = 0;
    WindowList::iterator it = this->childWindows.begin();

    for ( ; it != this->childWindows.end(); it++)
    {
        Window* child = (*it);
        Size size = child->WindowSize();

        if (wrapWidth)
        {
            isVertical
                ? wrappedWidth = max(wrappedWidth, size.width)
                : wrappedWidth += size.width;
        }

        if (wrapHeight)
        {
            isVertical
                ? wrappedHeight += size.height
                : wrappedHeight = max(wrappedHeight, size.height);
        }
    }

    int spacing = this->spacing * ((int) this->childWindows.size() - 1);

    // adjust for spacing
    if ((wrapWidth) && ( ! isVertical))
    {
        wrappedWidth += spacing;
    }
    //
    if ((wrapHeight) && (isVertical))
    {
        wrappedHeight += spacing;
    }

    // keep current size in given orientation if not wrapping
    if ( ! wrapWidth) wrappedWidth = size.width;
    if ( ! wrapHeight) wrappedHeight = size.height;

    this->isResizing = true;
    this->Resize(wrappedWidth, wrappedHeight);
    this->isResizing = false;
}

Point       LinearLayout::AlignChildInRect(LayoutAlignFlag alignment, Size childSize, Rect alignmentRect)
{
    bool isVertical = (this->orientation == VerticalLayout);

    Point result = alignmentRect.location;

    int childLayoutSize = isVertical
        ? childSize.width
        : childSize.height;

    int rectLayoutSize = isVertical
        ? alignmentRect.size.width
        : alignmentRect.size.height;

    int& location = isVertical
        ? result.x
        : result.y;

    switch (alignment)
    {
    case LayoutAlignCenter:      // && LayoutAlignMiddle
        location += (rectLayoutSize - childLayoutSize) / 2;
        break;

    case LayoutAlignRight:       // && LayoutAlignBottom
        location += (rectLayoutSize - childLayoutSize);
        break;

    case LayoutAlignLeft:        // && LayoutAlignTop
    default:
        // don't need to do anything
        break;
    }

    return result;
}

HWND        LinearLayout::Create(Window* parent)
{
    HINSTANCE hInstance = Application::Instance();

    if ( ! LinearLayout::RegisterWindowClass())
    {
        return NULL;
    }

    // create the window
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    //
    HWND hwnd = CreateWindowEx(
        NULL,                   // ExStyle
        CLASS_NAME,             // Class name
        _T(""),                 // Window name
        style,                  // Style
        0,                      // X
        0,                      // Y
        120,                    // Width
        36,                     // Height
        parent->Handle(),       // Parent
        NULL,                   // Menu
        hInstance,              // Instance
        NULL);                  // lParam

    return hwnd;
}

bool        LinearLayout::RegisterWindowClass()
{
    static bool registered  = false;

    if ( ! registered)
    {
        WNDCLASSEX wc = { 0 };

        // use STATIC window class as our base
        ::GetClassInfoEx(NULL, _T("STATIC"), &wc);
        wc.cbSize = sizeof(WNDCLASSEX);
        wc.lpszClassName = CLASS_NAME;

        registered = (::RegisterClassEx(&wc) != 0);
    }

    return registered;
}

void        LinearLayout::Layout()
{
    if ( ! this->childWindows.size())
    {
        return;
    }

    RedrawLock stopRedraw(this);

    bool isVertical = (this->orientation == VerticalLayout);
    bool isHorizontal = ( ! isVertical);

    WindowList wrappedChildren;
    WindowList filledChildren;
    ChildSizeMap finalSizes;
    int wrappedSize = 0;
    float totalWeight = 0.0f;

    WindowList::iterator it, end;

    Size layoutSize = this->ClientSize();

    // PASS 1: figure out which windows are wrapped in direction of the
    // orientation, and which are filled. Also, calculate the correct
    // size of the child opposite to its orientation.
    it = this->childWindows.begin();
    end = this->childWindows.end();
    for ( ; it != end; it++)
    {
        Window* child = (*it);
        Size childSize = child->WindowSize();

        // map the child to its fill type
        if (((isVertical) && (child->LayoutHeight() == LayoutFillParent))
        || ((isHorizontal) && (child->LayoutWidth() == LayoutFillParent)))
        {
            filledChildren.push_back(child);
            totalWeight += child->LayoutWeight();
        }
        else
        {
            wrappedChildren.push_back(child);

            // figure out the space all of the wrapped children will occupy.
            wrappedSize += (isVertical ? childSize.height : childSize.width);
        }

        // fill direction opposite the orientation
        if ((isVertical) && (child->LayoutWidth() == LayoutFillParent))
        {
            childSize.width = layoutSize.width;
        }
        else if ((isHorizontal) && (child->LayoutHeight() == LayoutFillParent))
        {
            childSize.height = layoutSize.height;
        }
        //
        finalSizes[child] = childSize;
    }
    //
    wrappedSize += (this->spacing * ((int) this->childWindows.size() - 1));

    // PASS 2: determine final child size for the filled children based on
    // their specified LayoutWeight()'s.
    if (filledChildren.size())
    {
        int remainingSize = (isVertical)
            ? layoutSize.height - wrappedSize
            : layoutSize.width - wrappedSize;
        //
        int weightedSize = 0;
        //
        it = filledChildren.begin();
        end = filledChildren.end();
        for ( ; it != end; it++)
        {
            Window* child = (*it);
            Size childSize = finalSizes[child];
            float remainingRatio = (child->LayoutWeight() / totalWeight);
            int newSize = (int) ((float) remainingSize * remainingRatio);

            // using floating calculations may cause small rounding errors. give
            // give the last filled child whatever space may be left over
            weightedSize += newSize;
            if ((it + 1) == end)
            {
                newSize += (remainingSize - weightedSize);
            }

            isVertical
                ? childSize.height = newSize
                : childSize.width = newSize;

            finalSizes[child] = childSize;
        }
    }

    // PASS 3: layout everything out!
    it = this->childWindows.begin();
    end = this->childWindows.end();
    int x = 0, y = 0;
    for ( ; it != end; it++)
    {
        Window* child = (*it);

        Size size = finalSizes[child];

        Size alignSize(
            (isVertical) ? layoutSize.width : size.width,
            (isVertical) ? size.height : layoutSize.height);

        Point location = this->AlignChildInRect(
            child->LayoutAlignment(),
            size,
            Rect(Point(x, y), alignSize));

        this->childIsResizing = true;
        {
            if (child->WindowSize() != size)
            {
                child->Resize(size);
            }

            if (child->Location() != location)
            {
                child->MoveTo(location);
            }
        }
        this->childIsResizing = false;

        isVertical
            ? y += (size.height + this->spacing)
            : x += (size.width + this->spacing);
    }
}
