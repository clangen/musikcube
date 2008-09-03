//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, André Wösten
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
#include <win32cpp/SysTray.hpp>

//////////////////////////////////////////////////////////////////////////////
#define WM_MC2_SYSTRAY (WM_USER + 100)

using namespace win32cpp;

IconList SysTray::iconList;
MenuList SysTray::menuList;
int SysTray::uidCounter = 100;

SysTray::SysTray()
{
    ::InitCommonControls();
}

SysTray::~SysTray()
{
    // iterate through list and delete icons
    for(IconList::iterator i = SysTray::iconList.begin(); i != SysTray::iconList.end(); ++i) {  
        this->DeleteIcon(i->second.uID);
    }
}

bool SysTray::DeleteIcon(UINT uid)
{
    if(SysTray::iconList.find(uid) != SysTray::iconList.end()) {
        return (::Shell_NotifyIcon(NIM_DELETE, &SysTray::iconList[uid]) != 0);
    }

    return false;
}

bool SysTray::ShowBalloon(UINT uid, const uistring& title, const uistring& text, UINT timeout, UINT icon)
{
    if(SysTray::iconList.find(uid) != SysTray::iconList.end()) {
        SysTray::iconList[uid].uFlags |= NIF_INFO;
        SysTray::iconList[uid].uTimeout = timeout * 1000;
        SysTray::iconList[uid].dwInfoFlags = icon;
       
        ::wcsncpy_s(SysTray::iconList[uid].szInfoTitle, 64, title.c_str(), 64);
        ::wcsncpy_s(SysTray::iconList[uid].szInfo, 256, text.c_str(), 256);
        
        return (::Shell_NotifyIcon(NIM_MODIFY, &SysTray::iconList[uid]) != 0);
    }

    return false;
}

bool SysTray::SetIcon(UINT uid, HICON icon)
{
    if(SysTray::iconList.find(uid) != SysTray::iconList.end()) {
        SysTray::iconList[uid].hIcon = icon;
        return (::Shell_NotifyIcon(NIM_MODIFY, &SysTray::iconList[uid]) != 0);
    }

    return false;
}

bool SysTray::SetTooltip(UINT uid, const uistring& tooltip)
{
    if(SysTray::iconList.find(uid) != SysTray::iconList.end()) {
        SysTray::iconList[uid].uFlags |= NIF_TIP;
        ::wcsncpy_s(SysTray::iconList[uid].szTip, 128, tooltip.c_str(), 128);
        return (::Shell_NotifyIcon(NIM_MODIFY, &SysTray::iconList[uid]) != 0);
    }

    return false;
}

bool SysTray::SetPopupMenu(UINT uid, MenuRef menu)
{
    if(menu) {
        SysTray::menuList[uid] = menu;
        return true;
    } 

    return false;
}

LRESULT SysTray::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if(SysTray::menuList.find(message - WM_MC2_SYSTRAY) != SysTray::menuList.end()) {
        switch(LOWORD(lParam)) {
        case WM_RBUTTONDOWN:
            {
                UINT uid = message - WM_MC2_SYSTRAY;
                if(SysTray::menuList.find(uid) != SysTray::menuList.end()) {
                    POINT mousePos = { 0 };
                    ::GetCursorPos(&mousePos);

                    TrackPopupMenu(
                        SysTray::menuList[uid]->Handle(),
                        TPM_LEFTBUTTON,
                        mousePos.x,
                        mousePos.y,
                        NULL,
                        SysTray::iconList[uid].hWnd,
                        NULL);
                }
            }
            return 0;
        }
    }

    return 0;
}

int SysTray::AddIcon(Window* window, HICON icon, const uistring& tooltip)
{
    UINT uid = SysTray::uidCounter++;
    NOTIFYICONDATA nid;

    // setup notifyicondata structure
    ::ZeroMemory(&nid, sizeof(nid));
    nid.cbSize  = sizeof(NOTIFYICONDATA);
    nid.hWnd    = window->Handle();
    nid.uID     = uid;
    nid.uFlags  = NIF_MESSAGE | NIF_ICON;
    if(tooltip != _T("")) {
        nid.uFlags |= NIF_TIP;
        ::wcsncpy_s(nid.szTip, 64, tooltip.c_str(), 64);
    }
    nid.hIcon   = icon;

    nid.uCallbackMessage = WM_MC2_SYSTRAY + uid;

    // create icon
    if(!::Shell_NotifyIcon(NIM_ADD, &nid)) {
        return -1;
    }
    
    nid.uVersion = NOTIFYICON_VERSION;
    if(!::Shell_NotifyIcon(NIM_SETVERSION, &nid)) {
        return -1;
    }
    
    // add to icon list
    SysTray::iconList[uid] = nid;

    return uid;
}