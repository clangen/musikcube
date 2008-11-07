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

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///FramePadding is used by Frame to specify the padding around the child control.
///
///\see
///Frame.
struct FramePadding
{
    ///\brief Constructor.
    /*ctor*/    FramePadding(int padding = 0)
    : left(padding)
    , right(padding)
    , top(padding)
    , bottom(padding)
    {
    }

    ///\brief Constructor.
    /*ctor*/    FramePadding(int left, int right, int top, int bottom)
    : left(left)
    , right(right)
    , top(top)
    , bottom(bottom)
    {
    }

    ///\brief Copy constructor.
    /*ctor*/    FramePadding(const FramePadding& padding)
    : left(padding.left)
    , right(padding.right)
    , top(padding.top)
    , bottom(padding.bottom)
    {
    }

    ///\brief Equality operator
    bool operator==(const FramePadding& padding) const
    {
        return ((padding.left == this->left) && (padding.right == this->right)
                && (padding.top == this->top) && (padding.bottom == this->bottom));
    }

    ///\brief Inequality operator
    bool operator!=(const FramePadding& padding) const
    {
        return ! (padding == *this);
    }

    ///\brief Left padding, in pixels
    int left;
    ///\brief Right padding, in pixels
    int right;
    ///\brief Top padding, in pixels
    int top;
    ///\brief Bottom padding, in pixels
    int bottom;
};

//////////////////////////////////////////////////////////////////////////////

///\brief
///Frame is a Container that adds a padding (border) to a single child Window.
///
///If the child is resized, Frame will automatically resize itself
///to accomidate the child and respect the the specified FramePadding.
///If the Frame is resized, it will automatically resize the child window 
///to fit its new ClientSize.
///
///Attempting to add more than 1 child Window to a Frame will result
///in a TooManyChildWindowsException.
///
///\see
///Container, Panel
class Frame: public Panel
{
private: //types
    typedef Panel base;

public: // constructors
    /*ctor*/    Frame(Window* child = NULL, int padding = 0);
    /*ctor*/    Frame(Window* child, const FramePadding& padding);

public: // methods
    void    SetPadding(const FramePadding& padding);
    void    SetPadding(int padding);

protected: // methods
    void    ResizeFromChild();
    void    OnChildResized(Window* window, Size newSize);

    virtual bool    AddChildWindow(Window* window);
    virtual void    OnChildAdded(Window* newChild);
    virtual void    OnCreated();
    virtual void    OnResized(const Size& newSize);
    virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

protected: // instance data
    FramePadding padding;

private: // instance data
    Window* child;
    bool isResizingHACK;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
