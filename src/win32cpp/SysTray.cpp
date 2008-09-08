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

using namespace win32cpp;

///\brief Contains the list of notify icons
IconList SysTray::iconList;

///\brief Contains a list of menus for each icon
MenuList SysTray::menuList;

///\brief Contains a list of options for each icon
OptionsList SysTray::optionsList;

///\brief Each notify icon has its own UID. This counter increments
///when an icon is created.
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


///\brief
///Deletes an icon from the tray and all internal structures
///
///\param uid
///The Icon ID
///
///\return bool
bool SysTray::DeleteIcon(UINT uid)
{
    if(SysTray::iconList.find(uid) != SysTray::iconList.end()) {
        if(::Shell_NotifyIcon(NIM_DELETE, &SysTray::iconList[uid]) != 0) {
            // todo: map/set not incrementable: why? 
            //SysTray::iconList.erase(uid);
            //SysTray::menuList.erase(uid);
            //SysTray::optionsList.erase(uid);

            return true;
        }
    }

    return false;
}


///\brief
///Shows a balloon tip over the tray icon
///
///\note
///Windows XP is required for this feature!
///
///\param uid
///The Icon ID
///
///\param title
///Balloon title
///
///\param text
///Balloon inner text
///
///\param timeout
///Time to show the balloon in seconds. There are special restrictions defined
///by the Windows-API. Look here: 
///http://msdn.microsoft.com/en-us/library/bb773352(VS.85).aspx
///
///\param text
///Icon to show. Select from NIIF_NONE, NIIF_INFO, NIIF_WARNING, NIIF_ERROR
///
///\return bool
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


///\brief
///Sets a new icon for the specified tray icon.
///
///\param uid
///The Icon ID
///
///\param icon
///New Icon
///
///\return bool
bool SysTray::SetIcon(UINT uid, HICON icon)
{
    if(SysTray::iconList.find(uid) != SysTray::iconList.end()) {
        SysTray::iconList[uid].hIcon = icon;
        return (::Shell_NotifyIcon(NIM_MODIFY, &SysTray::iconList[uid]) != 0);
    }

    return false;
}


///\brief
///Sets the tooltip for the specified tray icon.
///
///\param uid
///The Icon ID
///
///\param tooltip
///Tooltip to show
///
///\return bool
bool SysTray::SetTooltip(UINT uid, const uistring& tooltip)
{
    if(SysTray::iconList.find(uid) != SysTray::iconList.end()) {
        SysTray::iconList[uid].uFlags |= NIF_TIP;
        ::wcsncpy_s(SysTray::iconList[uid].szTip, 128, tooltip.c_str(), 128);
        return (::Shell_NotifyIcon(NIM_MODIFY, &SysTray::iconList[uid]) != 0);
    }

    return false;
}



///\brief
///Sets the popup menu for the specified tray icon.
///
///\param uid
///The Icon ID
///
///\param menu
///Reference to the menu which should be displayed
///
///\return bool
bool SysTray::SetPopupMenu(UINT uid, MenuRef menu)
{
    if(menu) {
        SysTray::menuList[uid] = menu;
        return true;
    } 

    return false;
}


///\brief
///Window procedure for all SysTray handling. Here WM_RBUTTONDOWN (for popup menu),
///WM_LBUTTONDOWN (for restoring the window after it has been minimized to tray) &
///WM_SIZE for the Minimize to tray feature are handled.
///
///\param window
///Affected window handle
///
///\param message
///Message-ID
///
///\param wParam
///\param lParam
///\return LRESULT
///In this case always 0
LRESULT SysTray::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if(SysTray::menuList.find(message - WM_W32CPP_SYSTRAY) != SysTray::menuList.end()) {
        UINT uid = message - WM_W32CPP_SYSTRAY;
        
        switch(LOWORD(lParam)) {
        case WM_RBUTTONDOWN:
            {
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
        case WM_LBUTTONDOWN:
            {
                 if(SysTray::optionsList[uid] & SysTray::MINIMIZE_TO_TRAY) {
                    // get window object
                    Window* wnd = Window::SubclassedWindowFromHWND(SysTray::iconList[uid].hWnd);

                    // restore window
                    wnd->Show(SW_SHOW);
                    wnd->Show(SW_RESTORE);

                 }
            }
            return 0;
        }
    }

    // handle minimize to tray
    if(message == WM_SIZE && wParam == SIZE_MINIMIZED) {
        // iterate through list with icon options and look, if the assigned icon/window pair wants that
        for(IconList::iterator i = SysTray::iconList.begin(); i != SysTray::iconList.end(); ++i) {  
            // look if there is a corresponding window
            if(i->second.hWnd == window) {
                if(SysTray::optionsList[i->second.uID] & SysTray::MINIMIZE_TO_TRAY) {
                    // get window object
                    Window* wnd = Window::SubclassedWindowFromHWND(window);

                    // and finally minimize it to tray
                    wnd->Show(SW_SHOWMINIMIZED);
                    wnd->Show(SW_HIDE);
                }
            }
        }
    }

    return 0;
}


///\brief
///Activates minimize to tray for the icon specified by uid.
///
///\param uid
///The Icon ID
void SysTray::EnableMinimizeToTray(UINT uid)
{
    SysTray::optionsList[uid] |= SysTray::MINIMIZE_TO_TRAY;
}


///\brief
///Creates and add a notify icon
///
///\param window
///The window to associate the icon with
///
///\param icon
///The Icon to show as HICON
///
///\param tooltip
///The tooltip to show. Leave it empty to show no tooltip
///
///\return int
///Returns the new Icon ID or -1 on failure
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

    nid.uCallbackMessage = WM_W32CPP_SYSTRAY + uid;

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

    // add to options list
    SysTray::optionsList[uid] = 0;

    return uid;
}