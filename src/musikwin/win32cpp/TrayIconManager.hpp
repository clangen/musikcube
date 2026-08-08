//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright � 2008, Andr� W�sten
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
 * @file TrayIconManager.hpp
 * @brief Manager for notification-area (system tray) icons.
 *
 * Part of the win32cpp native Win32 GUI wrapper library. TrayIconManager
 * wraps the Shell_NotifyIcon API to add, remove and update taskbar
 * notification icons, each identified by a unique UID. It also manages the
 * popup menu, tooltip and balloon notifications associated with each icon.
 * Access it through Application::Instance().SysTrayManager().
 */

#pragma once

#include <map>
#include <win32cpp/Menu.hpp>
#include <win32cpp/Window.hpp>

//////////////////////////////////////////////////////////////////////////////

namespace win32cpp {

//////////////////////////////////////////////////////////////////////////////
// TrayIconManager
//////////////////////////////////////////////////////////////////////////////

/** @brief Per-icon NOTIFYICONDATA structures keyed by icon ID. */
typedef std::map<UINT, NOTIFYICONDATA> IconList;
/** @brief Per-icon popup menus keyed by icon ID. */
typedef std::map<UINT, MenuRef> MenuList;
/** @brief Per-icon option bits keyed by icon ID. */
typedef std::map<UINT, UINT> OptionsList;

/** @brief Manages notification-area icons for an application.
 *  @details This class manages the icons in the Taskbar and must be seen
 *           as a pure manager; there are no special classes for tray
 *           icons. Since TrayIconManager communicates using the handle of
 *           its associated window, icons must be associated with a window
 *           handle. It should NOT be used directly, but from the
 *           Application singleton: use
 *           Application::Instance().SysTrayManager().
 *
 *           Internally each icon has three lists associated:
 *           - IconList iconList - NOTIFYICONDATA structures
 *           - MenuList menuList - win32cpp MenuRef references
 *           - OptionsList optionsList - option bits
 *
 *           Each notify icon is represented by its unique ID, which is
 *           assigned by AddIcon(). Using this ID you can access all other
 *           methods.
 *  @see Application, TopLevelWindow, MenuRef */
class TrayIconManager {
private: // types
    /** @brief Per-icon options; values must be powers of two. */
    enum Options {
        MINIMIZE_TO_TRAY = 1    /*!< minimize to tray on window close */
    };

    static IconList iconList;       /**< NOTIFYICONDATA per icon ID */
    static MenuList menuList;       /**< popup menu per icon ID */
    static OptionsList optionsList; /**< options per icon ID */
    static int uidCounter;          /**< counter used to allocate IDs */

public:
    /** @brief Handles tray messages for the owning window.
     *  @param window the window handle receiving the message
     *  @param message the window message (e.g. WM_W32CPP_SYSTRAY)
     *  @param wParam the WPARAM payload
     *  @param lParam the LPARAM payload
     *  @return the result of handling the message */
    LRESULT     WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    /** @brief Removes the icon with the given ID.
     *  @param uid the icon identifier
     *  @return true if the icon was deleted */
    bool        DeleteIcon(UINT uid);
    /** @brief Adds an icon to the notification area.
     *  @param window the window associated with the icon
     *  @param icon the icon handle to display
     *  @param tooltip optional tooltip text
     *  @return the unique ID of the new icon */
    int         AddIcon(Window* window, HICON icon, const uistring& tooltip = _T(""));
    /** @brief Replaces the icon image for the given ID.
     *  @param uid the icon identifier
     *  @param icon the new icon handle
     *  @return true if the icon was updated */
    bool        SetIcon(UINT uid, HICON icon);
    /** @brief Sets the tooltip for the given icon.
     *  @param uid the icon identifier
     *  @param tooltip the tooltip text
     *  @return true if the tooltip was updated */
    bool        SetTooltip(UINT uid, const uistring& tooltip);
    /** @brief Assigns a popup menu to the given icon.
     *  @param uid the icon identifier
     *  @param menu the popup menu to show on right-click
     *  @return true if the menu was assigned */
    bool        SetPopupMenu(UINT uid, MenuRef menu);
    /** @brief Shows a balloon notification for the given icon.
     *  @param uid the icon identifier
     *  @param title the balloon title
     *  @param text the balloon body text
     *  @param timeout how long to show the balloon, in milliseconds
     *  @param icon the balloon icon (e.g. NIIF_INFO)
     *  @return true if the balloon was shown */
    bool        ShowBalloon(UINT uid, const uistring& title, const uistring& text, UINT timeout, UINT icon = NIIF_INFO);
    /** @brief Enables minimize-to-tray behaviour for the icon.
     *  @param uid the icon identifier */
    void        EnableMinimizeToTray(UINT uid);

    /** @brief Constructs the tray icon manager. */
    /* ctor */  TrayIconManager();
    /** @brief Removes all managed icons. */
    /* dtor */  ~TrayIconManager();
};

//////////////////////////////////////////////////////////////////////////////

}