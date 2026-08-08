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
 * @file Theme.hpp
 * @brief Wrapper for Windows visual styles (UxTheme.dll).
 *
 * Part of the win32cpp native Win32 GUI wrapper library. Theme loads the
 * UxTheme API functions dynamically at runtime and lets controls draw with
 * the current visual style (XP+). It only implements the subset of the
 * UxTheme API used by the library.
 */

#pragma once

#include <boost/shared_ptr.hpp>
#include <uxtheme.h>
#include <tmschema.h>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////

class Theme;
/** @brief Shared pointer to a Theme object. */
typedef boost::shared_ptr<Theme> ThemeRef;

/** @brief Wraps Theme (UXTHEME.DLL) handling for XP+ systems.
 *  @details Dynamically loads the UxTheme functions via GetProcAddress so
 *           the library works on systems without visual styles enabled. */
class Theme : public boost::noncopyable
{
public: // types

private: // types
    typedef HRESULT (__stdcall *PFNCLOSETHEMEDATA)(HTHEME hTheme); /**< CloseThemeData signature */
    typedef HRESULT (__stdcall *PFNDRAWTHEMEBACKGROUND)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, const RECT *pRect,  const RECT *pClipRect); /**< DrawThemeBackground signature */
    typedef HTHEME (__stdcall *PFNOPENTHEMEDATA)(HWND hwnd, LPCWSTR pszClassList); /**< OpenThemeData signature */
    typedef HRESULT (__stdcall *PFNDRAWTHEMETEXT)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int iCharCount, DWORD dwTextFlags, DWORD dwTextFlags2, const RECT *pRect); /**< DrawThemeText signature */

public:
    /** @brief Opens a theme for the given window and class.
     *  @param handle the window handle
     *  @param className the theme class list (e.g. L"BUTTON")
     *  @return a shared Theme, or empty if themes are unavailable */
    static ThemeRef Create(HWND handle, const std::wstring& className);

public: // destructor
    /** @brief Closes the theme and releases the loaded DLL. */
    /*dtor*/    ~Theme();

private: // constructors
    /** @brief Loads UxTheme and opens the theme.
     *  @param handle the window handle
     *  @param className the theme class list */
    /*ctor*/    Theme(HWND handle, const std::wstring& className);

public: // methods
    /** @brief Draws the theme background for a part and state.
     *  @param hdc the target device context
     *  @param iPartId the theme part identifier
     *  @param iStateId the part state identifier
     *  @param pRect the bounding rectangle
     *  @param pClipRect optional clipping rectangle
     *  @return S_OK on success, otherwise an HRESULT error */
    HRESULT DrawThemeBackground(HDC hdc, int iPartId, int iStateId, const RECT* pRect, const RECT* pClipRect);

private: // instance data
    HMODULE dll;            /**< loaded UxTheme.dll module */
    HTHEME theme;           /**< opened theme handle */
    PFNOPENTHEMEDATA OpenThemeDataProc;        /**< imported OpenThemeData */
    PFNCLOSETHEMEDATA CloseThemeDataProc;      /**< imported CloseThemeData */
    PFNDRAWTHEMEBACKGROUND DrawThemeBackgroundProc; /**< imported DrawThemeBackground */
    PFNDRAWTHEMETEXT DrawThemeTextProc;         /**< imported DrawThemeText */
};

//////////////////////////////////////////////////////////////////////////////

} // namespace win32cpp
