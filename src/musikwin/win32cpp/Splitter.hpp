//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2007, Casey Langen
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

/**
 * @file Splitter.hpp
 * @brief Resizable two-pane container.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Splitter is a
 * Container with exactly two children that can be resized with the mouse.
 * It supports anchoring one child so that it stays fixed while the other
 * fills the remaining space.
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////
// Forward declare
namespace win32cpp {
    class Frame;
}
//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Panel.hpp>
#include <win32cpp/ILayout.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief The orientation of the split. */
enum SplitDirection
{
    /*! */ SplitRow,    /*!< split left/right (children side by side) */
    /*! */ SplitColumn  /*!< split top/bottom (children stacked) */
};

/** @brief Which child is anchored when the splitter resizes. */
enum AnchorDirection
{
    /*! */ AnchorTop,    /*!< anchor the top child */
    /*! */ AnchorBottom, /*!< anchor the bottom child */
    /*! */ AnchorLeft,   /*!< anchor the left child */
    /*! */ AnchorRight   /*!< anchor the right child */
};

/** @brief Used to disable anchor constraints in Splitter.
 *  @see Splitter::SetAnchorConstraints */
const int DisableConstraint = -1;

/** @brief A Container with two children that can be resized with the mouse.
 *  @details Splitter works with any two Window derived instances. Adding
 *           more than two children will result in a
 *           TooManyChildWindowsException.
 *
 *           Typically when a Splitter is resized it's preferable to keep
 *           one of the children stationary or "anchored," while allowing
 *           the other to be resized to fill the new space. The anchor can
 *           be set via Splitter::SetAnchor by specifying a
 *           win32cpp::AnchorDirection. */
class Splitter: public Panel, public ILayout
{
private: // types
    typedef Panel base;

public: // constructors
    /** @brief Constructs a splitter between two children.
     *  @param direction the split orientation
     *  @param child1 the first child (may be NULL)
     *  @param child2 the second child (may be NULL)
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/            Splitter(
                            SplitDirection direction,
                            Window* child1 = NULL,
                            Window* child2 = NULL,
                            LayoutFlags layoutFlags = LayoutWrapWrap);

    /** @brief Constructs an empty splitter.
     *  @param direction the split orientation
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/            Splitter(
                            SplitDirection direction,
                            LayoutFlags layoutFlags = LayoutWrapWrap);

public: // methods
    /** @brief Returns whether the splitter can be resized.
     *  @return true if the divider is draggable */
    bool                IsSizable();
    /** @brief Enables or disables mouse resizing.
     *  @param sizable true to allow dragging the divider */
    void                SetSizable(bool sizable);
    /** @brief Sets the first child.
     *  @param window the child to set
     *  @return the child that was set */
    Window*             SetChild1(Window* window);
    /** @brief Returns the first child.
     *  @return the first child, or NULL */
    const Window*       Child1();
    /** @brief Sets the second child.
     *  @param window the child to set
     *  @return the child that was set */
    Window*             SetChild2(Window* window);
    /** @brief Returns the second child.
     *  @return the second child, or NULL */
    const Window*       Child2();
    /** @brief Returns the current anchor direction.
     *  @return the anchor */
    AnchorDirection     Anchor();
    /** @brief Sets which child stays fixed on resize.
     *  @param newAnchor the anchor direction */
    void                SetAnchor(AnchorDirection newAnchor);
    /** @brief Returns the anchor size constraints.
     *  @param minSize receives the minimum anchor size
     *  @param maxSize receives the maximum anchor size */
    void                AnchorConstraints(int& minSize, int& maxSize);
    /** @brief Sets the anchor size constraints.
     *  @param minSize minimum anchor size, or DisableConstraint
     *  @param maxSize maximum anchor size, or DisableConstraint */
    void                SetAnchorConstraints(int minSize = DisableConstraint, int maxSize = DisableConstraint);
    /** @brief Sets the size of the anchored child.
     *  @param anchorSize the size in pixels */
    void                SetAnchorSize(int anchorSize);
    /** @brief Sets the width of the divider (gripper).
     *  @param pixels the gripper size in pixels */
    void                SetGripperSize(int pixels);

protected: // methods
    void    SetSizeCursor();
    int     AnchorSizeFromMouse(int splitPosition, const Size& newSize);
    void    BeginMouseCapture();
    void    EndMouseCapture();

    // overrides
    virtual bool        AddChildWindow(Window* window);
    virtual LRESULT     WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void        Layout();
    virtual void        LayoutFromMouse();
    virtual void        OnResized(const Size& newSize);
    virtual void        OnMouseMoved(MouseEventFlags flags, const Point& location);
    virtual void        OnMouseButtonDown(MouseEventFlags flags, const Point& location);
    virtual void        OnMouseButtonUp(MouseEventFlags flags, const Point& location);
    virtual void        OnMouseExit();
    virtual void        OnMouseEnter();
    virtual void        OnCreated();

private:
    void                InitializeInstance(SplitDirection direction, Window* child1, Window* child2);

private: // instance data
    Window *child1, *child2;        /**< the two children */
    Frame *child1Frame, *child2Frame; /**< frames wrapping each child */
    int gripperSize, anchorSize;    /**< divider width and anchored size */
    int minAnchorSize, maxAnchorSize; /**< anchor constraints */
    int sizeFromMouse;              /**< size captured when dragging began */
    bool isDragging, isSizable;     /**< drag and resizability flags */
    SplitDirection direction;       /**< split orientation */
    AnchorDirection anchor;         /**< anchored child */
    Rect splitRect;                 /**< current divider rectangle */

private: // class data
    static HCURSOR sHSizeCursor, sVSizeCursor, sArrowCursor; /**< shared cursors */
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
