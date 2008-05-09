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

#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///Container is a specialization of Window that can house one or more child controls.
///
///Use Container::AddChild and Container::RemoveChild to manage
///children. Once a Window is added to a Container, the Container
///takes ownership of it. <b>Do not, under any circumstances, call
///delete on a Window that is a child of a Container!</b> If the caller
///removes a child via Container::RemoveChild he is then responsible for
///deleting it.
///
///A Window may only have one parent, and a WindowAlreadyHasParentException
///will be thrown if this contract is violated.
///
///Some types of Containers can only hold a specific number of children,
///and will throw a TooManyChildWindowsException if that number is 
///exceeded. Frame, for example, only allows one child Window. Splitter
///allows two. Be sure to read the documentation of classes derived
///from Container for more information.
/*abstract*/ class Container: public Window
{
public: // types
    class TooManyChildWindowsException { };
    class WindowAlreadyHasParentException: public Exception { };
    class WindowHasNoParentException: public Exception { };
    class InvalidChildWindowException: public Exception { };

private: //types
    typedef Window base;

public: // constructors, methods
    /*ctor*/            Container();
    /*dtor*/ virtual    ~Container();

    template <typename WindowType>
    WindowType*     AddChild(WindowType* window);

    template <typename WindowType>
    WindowType*     RemoveChild(WindowType* window);

protected: // methods
    virtual HWND    Create(Window* parent) = 0;
    virtual bool    AddChildWindow(Window* window);
    virtual bool    RemoveChildWindow(Window* window);
    virtual void    OnChildAdded(Window* newChild) { /*for derived use*/ }
    virtual void    OnChildRemoved(Window* oldChild) { /*for derived use*/ }

    virtual void    OnRequestFocusNext();
    virtual void    OnRequestFocusPrev();
    virtual void    OnChildWindowRequestFocusNext(Window* child);
    virtual void    OnChildWindowRequestFocusPrev(Window* child);
    virtual void    OnGainedFocus();
    virtual bool    FocusLastChild();
    virtual bool    FocusFirstChild();
    virtual bool    FocusPrevChild();
    virtual bool    FocusNextChild();

private: // methods
    void            DestroyChildren();

    WindowList::iterator FindChild(const Window* child);
    WindowList::reverse_iterator ReverseFindChild(const Window* child);

protected: // instance data
    WindowList childWindows;
    Window* focusedWindow;
};

//////////////////////////////////////////////////////////////////////////////
// Container template methods
//////////////////////////////////////////////////////////////////////////////

///\brief
///Add the specified Window as a child of the Container.
///
///This method is a templated to make adding children as painless as possible.
///The return value is a pointer to the child that was just added. The user
///can use this method as follows:
///
///\code
///Label* myLabel = myContainer->AddChild(new Label(_T("Value: ")));
///\endcode
///
///\param window
///The Window to add.
///
///\returns
///Returns the Window that was added.
///
///\throws WindowIsNullException
///if window is NULL
///
///\throws WindowAlreadyHasParentException
///if the Window already has a parent
///
///\throws WindowAlreadyHasParentException
///if a class derived from Container doesn't like the child
///
///\see
///Container::RemoveChild
template <typename WindowType>
WindowType*     Container::AddChild(WindowType* window)
{
    if ( ! window)
    {
        throw WindowIsNullException();
        return window;
    }

    if (Window::WindowHasParent(window))
    {
        throw WindowAlreadyHasParentException();
        return window;
    }

    if ( ! this->AddChildWindow(static_cast<Window*>(window)))
    {
        throw InvalidChildWindowException();
        return window;
    }

    // keep track of all windows that have parents
    Window::sAllChildWindows.push_back(window);

    // set us as the window's new parent
    Window::SetParent(window, this);

    this->OnChildAdded(window);

    return window;
}

///\brief
///Removes the specified Window from the Container.
///
///Whoever calls this method is responsible for deleting the Window
///returned. This method is templated to make this as painless
///as possible. The return value is a pointer to the window removed.
///
///\code
///delete myContainer->RemoveChild(myLabel);
///\endcode
///
///\param window
///The Window to remove.
///
///\returns
///Returns the Window that was removed.
///
///\throws WindowIsNullException
///if window is NULL
///
///\throws WindowHasNoParentException
///if the Window doesn't have a parent
///
///\throws InvalidChildWindowException
///if a class derived from Container doesn't like the child
///
///\see
///Container::AddChild
template <typename WindowType>
WindowType*     Container::RemoveChild(WindowType* window)
{
    if ( ! window)
    {
        throw WindowIsNullException();
        return window;
    }

    if ( ! Window::WindowHasParent(window))
    {
        throw WindowHasNoParentException();
        return window;
    }

    if ( ! this->RemoveChildWindow(static_cast<Window*>(window)))
    {
        throw InvalidChildWindowException();
        return window;
    }

    // window is no longer a child window, remove it from the mapping.
    WindowList& allChildren = Window::sAllChildWindows;
    //
    allChildren.erase(
        std::find(allChildren.begin(), allChildren.end(), window));

    // set the MainWindow as the new parent, but do not add it as
    // a child. we do this so the child control doesn't get into
    // a bad state, but does not get automatically destroyed by the
    // main window. the once a child has been removed from a Container
    // it is the user's responsibility to clean it up.
    Window::SetParent(window, Application::Instance().MainWindow());

    this->OnChildRemoved(window);

    return window;
}

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp