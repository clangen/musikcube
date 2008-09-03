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
#include <win32cpp/WindowGeometry.hpp>

#include <string>
#include <sigslot/sigslot.h>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// uichar, uistring
//////////////////////////////////////////////////////////////////////////////

#include <TCHAR.H>

///\brief A single character, as used in the UI (a TCHAR).
typedef TCHAR uichar;

///\brief A std::basic_string<TCHAR>
typedef std::basic_string<uichar> uistring;

//////////////////////////////////////////////////////////////////////////////
// byte
//////////////////////////////////////////////////////////////////////////////

///\brief An unsigned byte.
typedef unsigned char byte;

//////////////////////////////////////////////////////////////////////////////
// GET_X_LPARAM, GET_Y_LPARAM
//////////////////////////////////////////////////////////////////////////////

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

//////////////////////////////////////////////////////////////////////////////

///\brief
///Classes wishing to handle <b>sigslot</b> signals must inherit from this class.
typedef sigslot::has_slots<> EventHandler;

//////////////////////////////////////////////////////////////////////////////

///\brief
///User-defined application-wide messages
#define WM_W32CPP                                 (WM_USER + 1000)
#define WM_W32CPP_SYSTRAY                         (WM_W32CPP  + 1)
#define WM_W32CPP_APPLICATIONTHREAD_CALL_WAITING  (WM_W32CPP  + 2)

//////////////////////////////////////////////////////////////////////////////


} // namespace win32cpp
