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

#include <boost/shared_ptr.hpp>
#include <uxtheme.h>
#include <tmschema.h>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class Theme;
typedef boost::shared_ptr<Theme> ThemeRef;

///\brief
///A class that wraps Theme (UXTHEME.DLL) handling for XP+ systems
class Theme : public boost::noncopyable
{
public: // types

private: // types
    typedef HRESULT (__stdcall *PFNCLOSETHEMEDATA)(HTHEME hTheme);
    typedef HRESULT (__stdcall *PFNDRAWTHEMEBACKGROUND)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT *pRect,  const RECT *pClipRect);
    typedef HTHEME (__stdcall *PFNOPENTHEMEDATA)(HWND hwnd, LPCWSTR pszClassList);
    typedef HRESULT (__stdcall *PFNDRAWTHEMETEXT)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags, DWORD dwTextFlags2, const RECT *pRect);

public:
    static ThemeRef Create(HWND handle, const std::wstring& className);

public: // destructor
    /*dtor*/    ~Theme();

private: // constructors
    /*ctor*/    Theme(HWND handle, const std::wstring& className);

public: // methods
    HRESULT DrawThemeBackground(HDC hdc, int iPartId, int iStateId, const RECT* pRect, const RECT* pClipRect);

private: // instance data
    HMODULE dll;
    HTHEME theme;
    PFNOPENTHEMEDATA OpenThemeDataProc;
    PFNCLOSETHEMEDATA CloseThemeDataProc;
    PFNDRAWTHEMEBACKGROUND DrawThemeBackgroundProc;
    PFNDRAWTHEMETEXT DrawThemeTextProc;
};

//////////////////////////////////////////////////////////////////////////////

} // namespace win32cpp
