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

#include "pch.hpp"
#include <win32cpp/Theme.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

//////////////////////////////////////////////////////////////////////////////

ThemeRef        Theme::Create(HWND handle, const std::wstring& className)
{
    ThemeRef theme(new Theme(handle, className));
    return (theme->dll ? theme : ThemeRef());
}

/*ctor*/        Theme::Theme(HWND handle, const std::wstring& className)
: dll(NULL)
, theme(NULL)
{
    if ( ! handle)
    {
        return;
    }

    this->dll = ::LoadLibrary(L"UXTHEME.DLL");
    if (this->dll != NULL)
    {
        this->OpenThemeDataProc = (PFNOPENTHEMEDATA)GetProcAddress(this->dll, "OpenThemeData");
        this->DrawThemeBackgroundProc = (PFNDRAWTHEMEBACKGROUND)GetProcAddress(this->dll, "DrawThemeBackground");
        this->CloseThemeDataProc = (PFNCLOSETHEMEDATA)GetProcAddress(this->dll, "CloseThemeData");
        this->DrawThemeTextProc = (PFNDRAWTHEMETEXT)GetProcAddress(this->dll, "DrawThemeText");

        if (( ! this->OpenThemeDataProc) || ( ! this->DrawThemeBackgroundProc)
        || ( ! this->CloseThemeDataProc) || ( ! this->DrawThemeTextProc))
        {
            ::FreeLibrary(this->dll);
            this->dll = NULL;
        }
        else
        {
            this->theme = this->OpenThemeDataProc(handle, className.c_str());
        }
    }
}

/*dtor*/        Theme::~Theme()
{
    if (this->dll)
    {
        ::FreeLibrary(this->dll);
        this->dll = NULL;
    }

    if ((this->theme) && (this->CloseThemeDataProc))
    {
        this->CloseThemeDataProc(this->theme);
        this->theme = NULL;
    }
}


HRESULT         Theme::DrawThemeBackground(HDC hdc, int iPartId, int iStateId, const RECT* pRect, const RECT* pClipRect)
{
    return this->DrawThemeBackgroundProc(this->theme, hdc, iPartId, iStateId, pRect, pClipRect);
}

