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

#include <win32cpp/Window.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

///\brief
///Use RedrawLock to stop the specified Window from being redrawn.
///Redrawing is automatically enabled on the Window when the RedrawLock's
///destructor is called.
///
///RedrawLock uses the the WM_SETREDRAW message, see. For more information
///see http://msdn2.microsoft.com/en-us/library/ms534853.aspx
///
///\code
///Splitter* mySplitter = ...;
///{
///    RedrawLock redrawLock(mySplitter)
///
///     //...
///     //perform operations to mySplitter
///     //...
///
///} // mySplitter will be redrawn when redrawLock's destructor is called
///
///\endcode
///
///RedrawLock is safe against locking the same Window multiple times, recursively.
///The Window will not be redrawn until all locks have been destructed.
struct RedrawLock
{
private: // types
    typedef std::map<HWND, unsigned> LockList;
    typedef LockList::iterator Iterator;

public: // constructors, destructors
    /*ctor*/    RedrawLock(Window* window);
    /*dtor*/    ~RedrawLock();

private: // instance data
    HWND hwnd;

private: // class data
    static LockList sLockList;
};

//////////////////////////////////////////////////////////////////////////////

} // win32cpp