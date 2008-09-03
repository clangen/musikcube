//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, André Wösten
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

#include <map>
#include <win32cpp/Menu.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// SysTray
//////////////////////////////////////////////////////////////////////////////

typedef std::map<UINT, NOTIFYICONDATA> IconList;
typedef std::map<UINT, MenuRef> MenuList;
typedef std::map<UINT, UINT> OptionsList;

class SysTray {
private:
    enum Options {
        MINIMIZE_TO_TRAY = 1
    };

    // Contains the list of notify icons
    static IconList iconList;

    // Contains a list of menus for each icon
    static MenuList menuList;

    // Contains a list of options for each icon
    static OptionsList optionsList;

    // Each notify icon has its own UID. This counter increments
    // when an icon is created.
    static int uidCounter;
    
public:
    LRESULT     WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    bool        DeleteIcon(UINT uid);
    int         AddIcon(Window* window, HICON icon, const uistring& tooltip = _T(""));
    bool        SetIcon(UINT uid, HICON icon);
    bool        SetTooltip(UINT uid, const uistring& tooltip);
    bool        SetPopupMenu(UINT uid, MenuRef menu);
    bool        ShowBalloon(UINT uid, const uistring& title, const uistring& text, UINT timeout, UINT icon = NIIF_INFO);
    void        EnableMinimizeToTray(UINT uid);

    /* ctor */  SysTray();
    /* dtor */  ~SysTray();
};

//////////////////////////////////////////////////////////////////////////////

}