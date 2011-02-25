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
#include <win32cpp/WindowPadding.hpp>
#include <win32cpp/Application.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///Frame is a Container that adds a padding (border) to a single child Window.
///
///If the child is resized, Frame will automatically resize itself
///to accomidate the child and respect the the specified WindowPadding.
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
    /*ctor*/    Frame(Window* child, const WindowPadding& padding);

public: // methods
    void SetPadding(const WindowPadding& padding);
    void SetPadding(int padding);
    virtual Size ClientSize() const;

protected: // methods
    void ResizeFromChild();
    void OnChildResized(Window* window, Size newSize);

    virtual bool AddChildWindow(Window* window);
    virtual void OnChildAdded(Window* newChild);
    virtual void OnCreated();
    virtual void OnResized(const Size& newSize);
    virtual HWND Create(Window* parent);

    static bool RegisterWindowClass();

private: // instance data
    WindowPadding padding;
    Window* child;
    bool isResizingHACK;
};

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
