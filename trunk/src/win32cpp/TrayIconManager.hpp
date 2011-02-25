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
#include <win32cpp/Window.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// TrayIconManager
//////////////////////////////////////////////////////////////////////////////

typedef std::map<UINT, NOTIFYICONDATA> IconList;
typedef std::map<UINT, MenuRef> MenuList;
typedef std::map<UINT, UINT> OptionsList;

///\brief
///The TrayIconManager class is used for managing icons in the Taskbar. It must be
///seen as pure manager for the icons in the TrayIconManager bar. Thus there are no
///special classes for TrayIcons.
///
///Since TrayIconManager communicates using the handle of its associated window, it is
///necessary, that icons are associated to a window handle.
///
///That's why the TrayIconManager component should NOT be used directly, but from the
///Application Singleton. Use Application::Instance().SysTrayManager() to
///get the TrayIconManager object!
///
///Internally each icon has three lists associated:
///
/// - IconList iconList - with NOTIFYICONDATA structures
/// - MenuList menuList - with references to win32cpp's MenuRef 
/// - OptionsList optionsList - option bits are set here
///
///Each notify icon is represted by its unique ID, which is assigned by 
///AddIcon(). Using this ID you can access all other methods.
///
///Code example:
///\Example
///\code
///
/// // Create Menu
/// MenuRef myMenu = Menu::CreatePopup();
/// myMenu->Items().Append(MenuItem::Create(_T("Test 1")));
/// myMenu->Items().Append(MenuItem::Create(_T("Test 2")));
/// MenuItemRef trayExit = myMenu->Items().Append(MenuItem::Create(_T("E&xit")));
///
/// // Bind Exit to a handler
/// trayExit->Activated.connect(this, &MainWindowController::OnFileExit);
///
/// // Init tray icon
/// UINT uidTrayIcon = Application::Instance().SysTrayManager()->AddIcon(Application::Instance().MainWindow(), icon16);
/// Application::Instance().SysTrayManager()->SetTooltip(uidTrayIcon, _T("Test");
/// Application::Instance().SysTrayManager()->SetPopupMenu(uidTrayIcon, myMenu);
/// Application::Instance().SysTrayManager()->ShowBalloon(uidTrayIcon, _T("Test"), _T("Welcome to this application!"), 2);
/// Application::Instance().SysTrayManager()->EnableMinimizeToTray(uidTrayIcon);
///\endcode
///\see
///Application
///TopLevelWindow
///MenuRef
class TrayIconManager {
private: // types
    ///\brief List with per-icon options - need to be power of 2! 
    enum Options {
        MINIMIZE_TO_TRAY = 1
    };

    static IconList iconList;
    static MenuList menuList;
    static OptionsList optionsList;
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

    /* ctor */  TrayIconManager();
    /* dtor */  ~TrayIconManager();
};

//////////////////////////////////////////////////////////////////////////////

}