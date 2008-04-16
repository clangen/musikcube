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
#include <win32cpp/Container.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

/*ctor*/    Container::Container()
: base()
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
        WindowList::iterator end = this->childWindows.end();
        WindowList::iterator it = this->childWindows.begin();
        //
        while (it != end)
        {
            Window::sAllChildWindows.erase(*it);

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

    this->childWindows.insert(window);

    return true;
}

bool        Container::RemoveChildWindow(Window* window)
{
    WindowList::iterator it = this->childWindows.find(window);
    //
    if (it == this->childWindows.end())
    {
        return false;
    }

    this->childWindows.erase(it);

    return true;
}