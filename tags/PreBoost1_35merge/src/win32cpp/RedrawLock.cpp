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
#include <win32cpp/Color.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

RedrawLock::LockList RedrawLock::sLockList;

///\brief
///Constructor
///
///\param window
///The Window whose redraw will be locked
/*ctor*/    RedrawLock::RedrawLock(Window* window)
: hwnd(window ? window->Handle() : NULL)
{
    Iterator it = RedrawLock::sLockList.find(this->hwnd);
    if (it == RedrawLock::sLockList.end())
    {
        RedrawLock::sLockList[this->hwnd] = 1;
        ::SendMessage(this->hwnd, WM_SETREDRAW, 0, 0);
    }
    else
    {
        RedrawLock::sLockList[this->hwnd]++;
    }
}

/*dtor*/    RedrawLock::~RedrawLock()
{
    Iterator it = RedrawLock::sLockList.find(this->hwnd);
    if (it == RedrawLock::sLockList.end())
    {
        // TODO: log error
    }

    RedrawLock::sLockList[this->hwnd]--;
    //
    if ( ! RedrawLock::sLockList[this->hwnd])
    {
        ::SendMessage(this->hwnd, WM_SETREDRAW, 1, 0);
        ::RedrawWindow(this->hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);

        RedrawLock::sLockList.erase(this->hwnd);
    }
}