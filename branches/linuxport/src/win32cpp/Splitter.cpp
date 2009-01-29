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

#include <pch.hpp>
#include <win32cpp/Splitter.hpp>
#include <win32cpp/Frame.hpp>
#include <win32cpp/RedrawLock.hpp>
#include <win32cpp/Application.hpp>


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
///The split direction, SplitColumn or SplitRow
///
///\param child1
///The left (horizontal) or top (vertical) child.
///
///\param child2
///The right (horizontal) or bottom (vertical) child.
///
///\param layoutFlags
///The size flags to be used when positioning this control within a parent
/*ctor*/    Splitter::Splitter(SplitDirection direction, Window* child1, Window* child2, LayoutFlags layoutFlags)
: base(layoutFlags)
{
    this->InitializeInstance(direction, child1, child2);
}

///\brief
///Constructor.
///
///\param direction
///The split direction, SplitColumn or SplitRow
///
///\param layoutFlags
///The size flags to be used when positioning this control within a parent
/*ctor*/    Splitter::Splitter(SplitDirection direction, LayoutFlags layoutFlags)
: base(layoutFlags)
{
    this->InitializeInstance(direction, NULL, NULL);
}

void        Splitter::InitializeInstance(SplitDirection direction, Window* child1, Window* child2)
{
    this->child1 = child1;
    this->child2 = child2;
    this->child1Frame = new Frame(child1, 0);
    this->child2Frame = new Frame(child2, 0);
    this->gripperSize = 4;
    this->anchorSize = -1;
    this->direction = direction;
    this->isDragging = false;
    this->isSizable = true;
    this->anchor = (direction == SplitColumn ? AnchorLeft : AnchorTop);
    this->minAnchorSize = -1;
    this->maxAnchorSize = -1;
    this->sizeFromMouse = -1;
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

    ::SetCursor(this->direction == SplitColumn
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
///Set the left (if SplitColumn) or top (if SplitRow) child.
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
///Set the right (SplitColumn) or bottom (SplitRow) child.
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

void    Splitter::LayoutFromMouse()
{
    Rect clientRect = this->ClientRect();
    int clientSize = (this->direction == SplitRow 
        ? clientRect.size.height
        : clientRect.size.width);

    bool child1IsAnchor = (this->anchor == AnchorLeft || this->anchor == AnchorTop);

    int control1Size = this->sizeFromMouse;
    int control2Size = clientSize - control1Size - this->gripperSize;
    control2Size = max(control2Size, 0);

    this->anchorSize = (child1IsAnchor ? control1Size : control2Size);

    // done! set this back for the next call to Layout()
    this->sizeFromMouse = -1;

    this->Layout();
}

void    Splitter::Layout()
{
    if (this->sizeFromMouse != -1)
    {
        this->LayoutFromMouse();
        return;
    }

    Rect clientRect = this->ClientRect();
    //
    int clientSize = (this->direction == SplitRow 
        ? clientRect.size.height
        : clientRect.size.width);

    int splitterPos = this->anchorSize;
    //
    if (splitterPos <= -1)
    {
        splitterPos = (clientSize / 2);
    }
    else
    {
        // don't allow the split to go beyond the window boundries
        splitterPos = min(splitterPos, clientSize - this->gripperSize);
    }
 
    // size the windows in their split direction
    bool child1IsAnchor = (this->anchor == AnchorLeft || this->anchor == AnchorTop);

    if (this->minAnchorSize >= 0) splitterPos = max(this->minAnchorSize, splitterPos);
    if (this->maxAnchorSize >= 0) splitterPos = min(this->maxAnchorSize, splitterPos);

    int nonAnchorSize = clientSize - (splitterPos + this->gripperSize);
    //
    int control1Size = child1IsAnchor ? splitterPos : nonAnchorSize;
    int control2Size = child1IsAnchor ? nonAnchorSize : splitterPos;

    if (this->direction == SplitColumn)
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

    // remember our splitter rect
    if (this->direction == SplitColumn)
    {
        this->splitRect = Rect(
            Point(control1Size, 0),
            Size(this->gripperSize, clientRect.size.height));
    }
    else
    {
        this->splitRect = Rect(
            Point(0, control1Size),
            Size(clientRect.size.width, this->gripperSize));
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
///The size will be the width if SplitColumn, the height if 
///SplitRow.
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
        int position = this->direction == SplitColumn ? location.x : location.y;
        //
        if (this->direction == SplitColumn)
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
        this->sizeFromMouse = position;
        this->Layout();
    }
    else if ( ! ::PointInRect(this->CursorPosition(), this->splitRect))
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

    if (::PointInRect(this->CursorPosition(), this->splitRect))
    {
        this->isDragging = true;
    }
}

void    Splitter::OnMouseExit()
{
    this->EndMouseCapture();
}

void    Splitter::OnMouseEnter()
{
    this->BeginMouseCapture();
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
