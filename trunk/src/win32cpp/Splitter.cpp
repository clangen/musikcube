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
#include <win32cpp/Panel.hpp>
#include <win32cpp/Splitter.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

HCURSOR Splitter::sHSizeCursor = ::LoadCursor(0, MAKEINTRESOURCE(IDC_SIZEWE));
HCURSOR Splitter::sVSizeCursor = ::LoadCursor(0, MAKEINTRESOURCE(IDC_SIZENS));
HCURSOR Splitter::sArrowCursor = ::LoadCursor(0, MAKEINTRESOURCE(IDC_ARROW));

///\brief
///Constructor.
///
///\param direction
///The split direction, SplitHorizontal or SplitVertical
///
///\param child1
///The left (horizontal) or top (vertical) child.
///
///\param child2
///The right (horizontal) or bottom (vertical) child.
/*ctor*/    Splitter::Splitter(SplitDirection direction, Window* child1, Window* child2)
: base()
, child1(child1)
, child2(child2)
, child1Frame(new Frame(child1, 0))
, child2Frame(new Frame(child2, 0))
, gripperSize(4)
, anchorSize(-1)
, direction(direction)
, isDragging(false)
, isSizable(true)
, anchor(direction == SplitHorizontal ? AnchorLeft : AnchorTop)
, minAnchorSize(-1)
, maxAnchorSize(-1)
{
}

LRESULT     Splitter::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    return this->DefaultWindowProc(message, wParam, lParam);
}

///\brief
///Returns true if the Splitter can be resized with the mouse, false otherwise.
bool        Splitter::IsSizable()
{
    return this->isSizable;
}

///\brief
///Sets whether or not the Splitter can be resized with the mouse via
///click and drag
void        Splitter::SetSizable(bool sizable)
{
    this->isSizable = sizable;
}

void        Splitter::SetSizeCursor()
{
    if ( ! this->isSizable)
    {
        return;
    }

    ::SetCursor(this->direction == SplitHorizontal
        ? Splitter::sHSizeCursor
        : Splitter::sVSizeCursor);
}

bool        Splitter::AddChildWindow(Window* window)
{
    if (this->childWindows.size() >= 2)
    {
        throw TooManyChildWindowsException();
    }

    return base::AddChildWindow(window);
}

///\brief
///Set the left (if SplitHorizontal) or top (if SplitVertical) child.
///
///The Splitter will assume ownership of the specified Window and
///will delete it when destroyed.
///
///\param window
///The Window to use as a child.
///
///\returns
///The previous child Window. The caller is responsible for freeing
///the Window.
Window*     Splitter::SetChild1(Window* window)
{
    Window* oldChild = this->child1;

    if (oldChild)
    {
        this->child1Frame->RemoveChild(oldChild);
    }

    this->child1Frame->AddChild(window);
    this->child1 = window;

    this->Layout();

    return oldChild;
}

///\brief
///Returns the first (left, or top) child.
const Window*   Splitter::Child1()
{
    return this->child1;
}


///\brief
///Set the right (SplitHorizontal) or bottom (SplitVertical) child.
///
///The Splitter will assume ownership of the specified Window and
///will delete it when destroyed.
///
///\param window
///The Window to use as a child.
///
///\returns
///The previous child Window. The caller is responsible for freeing
///the Window.
Window*     Splitter::SetChild2(Window* window)
{
    Window* oldChild = this->child2;

    if (oldChild)
    {
        this->child2Frame->RemoveChild(oldChild);
    }

    this->child2Frame->AddChild(window);
    this->child2 = window;

    this->Layout();

    return oldChild;
}

///\brief
///Returns the second (right, or bottom) child.
const Window*   Splitter::Child2()
{
    return this->child2;
}

void    Splitter::Layout(int child1Size)
{
    Rect clientRect = this->ClientRect();
    //
    int clientSize = (this->direction == SplitVertical 
        ? clientRect.size.height
        : clientRect.size.width);

    int anchorSize = (child1Size == -1 ? this->anchorSize : child1Size);
    //
    if (anchorSize <= -1)
    {
        anchorSize = (clientSize / 2);
    }
    else
    {
        // don't allow the split to go beyond the window boundries
        anchorSize = min(anchorSize, clientSize - this->gripperSize);
    }
 
    // size the windows in their split direction
    bool child1IsAnchor = (this->anchor == AnchorLeft || this->anchor == AnchorTop);
    child1IsAnchor |= (child1Size != -1);

    if (this->minAnchorSize >= 0) anchorSize = max(this->minAnchorSize, anchorSize);
    if (this->maxAnchorSize >= 0) anchorSize = min(this->maxAnchorSize, anchorSize);

    int nonAnchorSize = clientSize - (anchorSize + this->gripperSize);
    //
    int control1Size = child1IsAnchor ? anchorSize : nonAnchorSize;
    int control2Size = child1IsAnchor ? nonAnchorSize : anchorSize;

    if (this->direction == SplitHorizontal)
    {
        child1Frame->Resize(control1Size, clientRect.size.height);
        child2Frame->Resize(control2Size, clientRect.size.height);
        child2Frame->MoveTo(control1Size + this->gripperSize, clientRect.location.y);
    }
    else    // vertical
    {
        child1Frame->Resize(clientRect.size.width, control1Size);
        child2Frame->Resize(clientRect.size.width, control2Size);
        child2Frame->MoveTo(clientRect.location.x, control1Size + this->gripperSize);
    }

    if (child1Size != -1)
    {
        this->anchorSize = anchorSize;
    }
}

///\brief Returns the AnchorDirection of the splitter.
///\see Splitter
AnchorDirection     Splitter::Anchor()
{
    return this->anchor;
}

///\brief Sets the AnchorDirection of the splitter.
///\param newAnchor The new AnchorDirection
///\see Splitter
void        Splitter::SetAnchor(AnchorDirection newAnchor)
{
    this->anchor = newAnchor;
}

///\brief Sets the new AnchorDirection of the splitter.
///
///The size will be the width if SplitHorizontal, the height if 
///SplitVertical.
///
///\param anchorSize the new size of the anchor control, in pixels.
//
///\see Splitter
void    Splitter::SetAnchorSize(int anchorSize)
{
    this->anchorSize = anchorSize;
    this->Layout();
}

///\brief Sets the size of the resize gripper, in pixels.
void    Splitter::SetGripperSize(int gripperSize)
{
    this->gripperSize = gripperSize;
    this->Layout();
}

void    Splitter::OnResized(const Size& newSize)
{
    this->Layout();
}

void    Splitter::OnCreated()
{
    this->AddChild(this->child1Frame);
    this->AddChild(this->child2Frame);
}

void    Splitter::OnMouseMoved(MouseEventFlags flags, const Point& location)
{
    this->SetSizeCursor();

    if (this->isDragging)
    {
        Rect clientRect = this->ClientRect();
        int position = this->direction == SplitHorizontal ? location.x : location.y;
        //
        if (this->direction == SplitHorizontal)
        {
            int minX = clientRect.location.x;
            int maxX = clientRect.location.x + clientRect.size.width;
            //
            if (position < minX) position = minX;
            if (position > maxX) position = maxX;
        }
        else
        {
            int minY = clientRect.location.y;
            int maxY = clientRect.location.y + clientRect.size.height;
            //
            if (position < minY) position = minY;
            if (position > maxY) position = maxY;
        }

        // don't allow the main window to be redrawn until resizing has finished. we
        // do this because we may be redrawing complex child controls (including other
        // splitters) unnecessarily.
        RedrawLock redrawLock(Application::Instance().MainWindow());
        this->Layout(position);
    }
    else if ( ! ::PointInRect(this->CursorPosition(), this->SplitterRect()))
    {
        this->EndMouseCapture();
    }
    else
    {
        if (Window::Capture() != this)
        {
            this->BeginMouseCapture();
        }
    }
}

void    Splitter::OnMouseButtonDown(MouseEventFlags flags, const Point& location)
{
    if ( ! isSizable)
    {
        return;
    }

    this->isDragging = true;
}

void    Splitter::OnMouseExit()
{
    this->EndMouseCapture();
}

void    Splitter::OnMouseEnter()
{
    this->BeginMouseCapture();
}

Rect    Splitter::SplitterRect()
{
    Rect splitterRect;

    bool child1IsAnchor = (this->anchor == AnchorLeft || this->anchor == AnchorTop);
    bool isHoriz = (this->direction == SplitHorizontal);

    int clientSize = isHoriz
        ? this->WindowSize().height
        : this->WindowSize().width;

    splitterRect.size = Size(
        isHoriz ? this->gripperSize : clientSize, 
        isHoriz ? clientSize : this->gripperSize);

    int nonAnchorSize = clientSize - (this->anchorSize + this->gripperSize);
    int location = child1IsAnchor ? this->anchorSize : nonAnchorSize;

    if (location == -1)
    {
        location = (isHoriz
        ? this->WindowSize().width
        : this->WindowSize().height) / 2;
    }

    splitterRect.location = Point(
        isHoriz ? location : 0,
        isHoriz ? 0 : location);

    return splitterRect;
}

void    Splitter::OnMouseButtonUp(MouseEventFlags flags, const Point& location)
{
    this->isDragging = false;
    this->EndMouseCapture();
}

void    Splitter::BeginMouseCapture()
{
    if (this->isSizable)
    {
        Window::BeginCapture(this);
        this->SetSizeCursor();
    }
}

void    Splitter::EndMouseCapture()
{
    if (this->isSizable)
    {
        Window::EndCapture(this);
        ::SetCursor(Splitter::sArrowCursor);
    }
}

///\brief Returns the minimum and maximum allowed sizes of the anchored child.
///\param minSize the minimum size of the anchored child
///\param maxSize the maximum size of the anchored child
void    Splitter::AnchorConstraints(int& minSize, int& maxSize)
{
    if (&minSize) minSize = this->minAnchorSize;
    if (&maxSize) maxSize = this->maxAnchorSize;
}

///\brief Sets the minimum and maximum allowed sizes of the anchored child.
///
///Clients may pass win32cpp::DisableConstraint to disable contraints.
///
///\param minSize the minimum size of the anchored child
///\param maxSize the maximum size of the anchored child
void    Splitter::SetAnchorConstraints(int minSize, int maxSize)
{
    this->minAnchorSize = minSize;
    this->maxAnchorSize = maxSize;
    this->Layout();
}
