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
#include <win32cpp/Container.hpp>
#include <win32cpp/Application.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    Container::Container()
: base()
, focusedWindow(NULL)
{
}

/*dtor*/    Container::~Container()
{
    this->DestroyChildren();
}

void        Container::DestroyChildren()
{
    try
    {
        // destroy all of our managed child windows
        WindowList& allChildren = Window::sAllChildWindows;
        WindowList::iterator end = this->childWindows.end();
        WindowList::iterator it = this->childWindows.begin();
        //
        while (it != end)
        {
            allChildren.erase(
                std::find(allChildren.begin(), allChildren.end(), *it));

            try
            {
                (*it)->Destroy();
                delete (*it);
            }
            catch(...)
            {
                // FIXME: log window couldn't be destroyed
            }

            ++it;
        }
        //
        this->childWindows.clear();

        this->Destroy();
    }
    catch(...)
    {
        // FIXME: log dtor threw unexpectedly
    }
}

bool        Container::AddChildWindow(Window* window)
{
    if ( ! this->Handle())
    {
        this->Initialize(Application::Instance().MainWindow());
    }

    if ( ! window->Handle())
    {
        window->Initialize(this);
    }

    this->childWindows.push_back(window);

    window->RequestFocusNext.connect(
        this, &Container::OnChildWindowRequestFocusNext);

    window->RequestFocusPrev.connect(
        this, &Container::OnChildWindowRequestFocusPrev);

    return true;
}

bool        Container::RemoveChildWindow(Window* window)
{
    WindowList::iterator it = this->FindChild(window);
    //
    if (it == this->childWindows.end())
    {
        return false;
    }

    window->RequestFocusNext.disconnect(this);
    window->RequestFocusPrev.disconnect(this);

    this->childWindows.erase(it);

    if (this->focusedWindow == window)
    {
        this->focusedWindow = NULL;
    }

    return true;
}

Window::WindowList::iterator Container::FindChild(const Window* child)
{
    return std::find(
        this->childWindows.begin(), this->childWindows.end(), child);
}

Window::WindowList::reverse_iterator Container::ReverseFindChild(const Window* child)
{
    return std::find(
        this->childWindows.rbegin(), this->childWindows.rend(), child);
}

template <typename List, typename Iterator>
Window*     FocusNextValidChild(List& collection, Iterator it, Iterator end)
{
    if (collection.size())
    {
        while ((it != end) && ( ! (*it)->TabStop()))
        {
            it++;
        }

        if ((it != end) && ((*it)->SetFocus()))
        {
            return (*it);
        }
    }

    return NULL;
}

bool        Container::FocusFirstChild()
{
    this->focusedWindow = FocusNextValidChild(
        this->childWindows,
        this->childWindows.begin(),
        this->childWindows.end());
    
    return (this->focusedWindow != NULL);
}

bool        Container::FocusLastChild()
{
    this->focusedWindow = FocusNextValidChild(
        this->childWindows,
        this->childWindows.rbegin(),
        this->childWindows.rend());

    return (this->focusedWindow != NULL);
}

bool        Container::FocusNextChild()
{
    WindowList::iterator it = this->FindChild(this->focusedWindow);
    it++;

    this->focusedWindow = FocusNextValidChild(
        this->childWindows,
        it,
        this->childWindows.end());

    return (this->focusedWindow != NULL);
}

bool        Container::FocusPrevChild()
{
    WindowList::reverse_iterator it = this->ReverseFindChild(this->focusedWindow);
    it++;

    this->focusedWindow = FocusNextValidChild(
        this->childWindows,
        it,
        this->childWindows.rend());

    return (this->focusedWindow != NULL);
}

void        Container::OnGainedFocus()
{
    bool success;

    Window::sFocusDirection == Window::FocusForward
        ? success = this->FocusFirstChild()
        : success = this->FocusLastChild();

    if ( ! success)
    {
        Window::sFocusDirection == Window::FocusForward
            ? this->OnRequestFocusNext()
            : this->OnRequestFocusPrev();
    }
}

void        Container::OnChildWindowRequestFocusNext(Window* window)
{
    this->focusedWindow = window;
    this->OnRequestFocusNext();
}

void        Container::OnChildWindowRequestFocusPrev(Window* window)
{
    this->focusedWindow = window;
    this->OnRequestFocusPrev();
}

void        Container::OnRequestFocusNext()
{
    bool focusChildSuccess = (this->focusedWindow == NULL)
        ? this->FocusFirstChild()
        : this->FocusNextChild();

    // we weren't able to focus the next child, so notify the parent.
    if (focusChildSuccess == NULL)
    {
        base::OnRequestFocusNext();
    }
}

void        Container::OnRequestFocusPrev()
{
    bool focusChildSuccess = (this->focusedWindow == NULL)
        ? this->FocusLastChild()
        : this->FocusPrevChild();

    // we weren't able to focus the previous child, so notify the parent.
    if ( ! focusChildSuccess)
    {
        base::OnRequestFocusPrev();
    }
}