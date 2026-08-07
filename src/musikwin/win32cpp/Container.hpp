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
 * @file Container.hpp
 * @brief Abstract base class for windows that house child controls.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Container is a
 * Window that owns zero or more child windows. Children are added with
 * AddChild() and removed with RemoveChild(); a container takes ownership of
 * its children. Derived classes impose limits on the number of children
 * (e.g. Frame allows one, Splitter allows two).
 */

#pragma once

//////////////////////////////////////////////////////////////////////////////

#include <win32cpp/Win32Config.hpp>
#include <win32cpp/Window.hpp>

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

/** @brief Container is a specialization of Window that can house child controls.
 *  @details Use Container::AddChild and Container::RemoveChild to manage
 *           children. Once a Window is added to a Container, the Container
 *           takes ownership of it. <b>Do not, under any circumstances, call
 *           delete on a Window that is a child of a Container!</b> If the
 *           caller removes a child via Container::RemoveChild he is then
 *           responsible for deleting it.
 *
 *           A Window may only have one parent, and a
 *           WindowAlreadyHasParentException will be thrown if this contract
 *           is violated.
 *
 *           Some types of Containers can only hold a specific number of
 *           children, and will throw a TooManyChildWindowsException if that
 *           number is exceeded. Frame, for example, only allows one child
 *           Window. Splitter allows two. Be sure to read the documentation
 *           of classes derived from Container for more information. */
/*abstract*/ class Container: public Window
{
public: // types
    class TooManyChildWindowsException { };                             /**< too many children for this container */
    class WindowAlreadyHasParentException: public Exception { };        /**< child already has a parent */
    class WindowHasNoParentException: public Exception { };             /**< child has no parent */
    class InvalidChildWindowException: public Exception { };            /**< container rejected the child */

private: //types
    typedef Window base;

public: // constructors, methods
    /** @brief Constructs an empty container. */
    /*ctor*/ Container();
    /** @brief Constructs a container with layout flags.
     *  @param layoutFlags layout flags used for sizing */
    /*ctor*/ Container(LayoutFlags layoutFlags);
    /** @brief Destroys the container and its children. */
    /*dtor*/ virtual ~Container();

    /** @brief Adds a child window, taking ownership of it.
     *  @tparam WindowType the concrete window type
     *  @param window the window to add
     *  @return the added window */
    template <typename WindowType>
    WindowType*     AddChild(WindowType* window);

    /** @brief Removes a child window; the caller owns it afterwards.
     *  @tparam WindowType the concrete window type
     *  @param window the window to remove
     *  @return the removed window */
    template <typename WindowType>
    WindowType*     RemoveChild(WindowType* window);

protected: // methods
    /** @brief Creates the underlying HWND. Pure virtual. */
    virtual HWND    Create(Window* parent) = 0;
    /** @brief Adds a window to the child list.
     *  @param window the window to add
     *  @return true if accepted */
    virtual bool    AddChildWindow(Window* window);
    /** @brief Removes a window from the child list.
     *  @param window the window to remove
     *  @return true if removed */
    virtual bool    RemoveChildWindow(Window* window);
    /** @brief Hook invoked after a child is added.
     *  @param newChild the added child */
    virtual void    OnChildAdded(Window* newChild) { /*for derived use*/ }
    /** @brief Hook invoked after a child is removed.
     *  @param oldChild the removed child */
    virtual void    OnChildRemoved(Window* oldChild) { /*for derived use*/ }

    virtual void    OnRequestFocusNext();
    virtual void    OnRequestFocusPrev();
    virtual void    OnChildWindowRequestFocusNext(Window* child);
    virtual void    OnChildWindowRequestFocusPrev(Window* child);
    virtual void    OnGainedFocus();
    /** @brief Attempts to focus the last child.
     *  @return true if focus was set */
    virtual bool    FocusLastChild();
    /** @brief Attempts to focus the first child.
     *  @return true if focus was set */
    virtual bool    FocusFirstChild();
    /** @brief Focuses the previous focusable child.
     *  @return true if focus moved */
    virtual bool    FocusPrevChild();
    /** @brief Focuses the next focusable child.
     *  @return true if focus moved */
    virtual bool    FocusNextChild();

private: // methods
    /** @brief Destroys all child windows. */
    void            DestroyChildren();

    WindowList::iterator FindChild(const Window* child);
    WindowList::reverse_iterator ReverseFindChild(const Window* child);

protected: // instance data
    WindowList childWindows;    /**< the list of child windows */
    Window* focusedWindow;      /**< the currently focused child */
};

//////////////////////////////////////////////////////////////////////////////
// Container template methods
//////////////////////////////////////////////////////////////////////////////

/** @brief Add the specified Window as a child of the Container.
 *  @details This method is templated to make adding children as painless as
 *           possible. The return value is a pointer to the child that was
 *           just added. The user can use this method as follows:
 *  @code
 *  Label* myLabel = myContainer->AddChild(new Label(_T("Value: ")));
 *  @endcode
 *  @param window the Window to add
 *  @return the Window that was added
 *  @throws WindowIsNullException if window is NULL
 *  @throws WindowAlreadyHasParentException if the Window already has a parent
 *  @throws InvalidChildWindowException if a derived container rejects the child
 *  @see Container::RemoveChild */
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

/** @brief Removes the specified Window from the Container.
 *  @details Whoever calls this method is responsible for deleting the
 *           Window returned. This method is templated to make this as
 *           painless as possible. The return value is a pointer to the
 *           window removed.
 *  @code
 *  delete myContainer->RemoveChild(myLabel);
 *  @endcode
 *  @param window the Window to remove
 *  @return the Window that was removed
 *  @throws WindowIsNullException if window is NULL
 *  @throws WindowHasNoParentException if the Window doesn't have a parent
 *  @throws InvalidChildWindowException if a derived container rejects the child
 *  @see Container::AddChild */
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

    this->OnChildRemoved(window);

    return window;
}

//////////////////////////////////////////////////////////////////////////////

}   // win32cpp
