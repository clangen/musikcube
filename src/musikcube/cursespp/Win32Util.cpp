//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "stdafx.h"
#include "Win32Util.h"
#include <Windows.h>
#include <Commctrl.h>
#include <shellapi.h>

#ifdef WIN32

#define WM_TRAYICON (WM_USER + 2000)
#define WM_SHOW_OTHER_INSTANCE (WM_USER + 2001)

static std::basic_string<TCHAR> className = L"Curses_App";
static HWND mainWindow = nullptr;
static WNDPROC oldWndProc = nullptr;
static std::unique_ptr<NOTIFYICONDATA> trayIcon;
static bool minimizeToTray = false, minimizedToTray = false;
static std::string appTitle;
static HICON icon16 = nullptr, icon32 = nullptr;

static std::string runingMutexName;
static HANDLE runningMutex;
static DWORD runningMutexLastError = 0;

static HWND findThisProcessMainWindow() {
    static TCHAR buffer[256];

    if (mainWindow == nullptr) {
        DWORD dwProcID = GetCurrentProcessId();
        HWND hWnd = GetTopWindow(GetDesktopWindow());
        while (hWnd) {
            DWORD dwWndProcID = 0;
            GetWindowThreadProcessId(hWnd, &dwWndProcID);
            if (dwWndProcID == dwProcID) {
                GetClassName(hWnd, buffer, sizeof(buffer));
                if (className == std::basic_string<TCHAR>(buffer)) {
                    mainWindow = hWnd;
                    return hWnd;
                }
            }
            hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
        }
    }

    return nullptr;
}

static HWND findOtherProcessMainWindow(const std::string& title) {
    static TCHAR buffer[256];

    DWORD dwProcID = GetCurrentProcessId();
    HWND hWnd = GetTopWindow(GetDesktopWindow());

    while (hWnd) {
        DWORD dwWndProcID = 0;
        GetWindowThreadProcessId(hWnd, &dwWndProcID);
        if (dwWndProcID != dwProcID) { /* not in this process */
            GetClassName(hWnd, buffer, sizeof(buffer));
            if (className == std::basic_string<TCHAR>(buffer)) {
                ::GetWindowText(hWnd, buffer, sizeof(buffer));
                if (title == u16to8(buffer)) { /* title must match*/
                    return hWnd;
                }
            }
        }

        hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
    }

    return nullptr;
}

static HICON loadIcon(int resourceId, int size) {
    return (HICON) ::LoadImageA(
        GetModuleHandle(nullptr),
        MAKEINTRESOURCEA(resourceId),
        IMAGE_ICON,
        size,
        size,
        0);
}

static void initTrayIcon(HWND hwnd) {
    if (!trayIcon) {
        trayIcon.reset(new NOTIFYICONDATA());
        SecureZeroMemory(trayIcon.get(), sizeof(*trayIcon));
        trayIcon->hWnd = hwnd;
        trayIcon->uID = 0;
        trayIcon->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        trayIcon->uCallbackMessage = WM_TRAYICON;
        trayIcon->hIcon = icon16;

        std::wstring title = u8to16(appTitle);
        ::wcscpy_s(trayIcon->szTip, 255, title.c_str());
    }
}

static void restoreFromTray(HWND hwnd) {
    Shell_NotifyIcon(NIM_DELETE, trayIcon.get());
    minimizedToTray = false;
    ShowWindow(hwnd, SW_SHOWNORMAL);

}

static void resetMutex() {
    CloseHandle(runningMutex);
    runningMutex = nullptr;
    runningMutexLastError = 0;
}

static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR data) {
    if (minimizeToTray) {
        if ((msg == WM_SIZE && wparam == SIZE_MINIMIZED) ||
            (msg == WM_SYSCOMMAND && wparam == SC_MINIMIZE))
        {
            if (!minimizedToTray) {
                initTrayIcon(hwnd);
                minimizedToTray = (Shell_NotifyIcon(NIM_ADD, trayIcon.get()) != 0);
                if (minimizedToTray) {
                    ShowWindow(hwnd, SW_HIDE);
                    trayIcon->uVersion = NOTIFYICON_VERSION;
                    ::Shell_NotifyIcon(NIM_SETVERSION, trayIcon.get());
                    return 1;
                }
            }
        }
    }

    if (msg == WM_TRAYICON) {
        if (LOWORD(lparam) == WM_LBUTTONUP) {
            restoreFromTray(hwnd);
            return 1;
        }
    }

    if (msg == WM_SHOW_OTHER_INSTANCE) {
        cursespp::win32::ShowMainWindow();
        restoreFromTray(hwnd);
        return 1;
    }

    if (msg == WM_QUIT) {
        resetMutex();
    }

    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

namespace cursespp {
    namespace win32 {
        void ShowMainWindow() {
            findThisProcessMainWindow();
            if (mainWindow) {
                ShowWindow(mainWindow, SW_SHOWNORMAL);
            }
        }

        void HideMainWindow() {
            findThisProcessMainWindow();
            if (mainWindow) {
                ShowWindow(mainWindow, SW_HIDE);
            }
        }

        void Minimize() {
            findThisProcessMainWindow();
            if (mainWindow) {
                ShowWindow(mainWindow, SW_SHOWMINIMIZED);
            }
        }

        HWND GetMainWindow() {
            findThisProcessMainWindow();
            return mainWindow;
        }

        void SetIcon(int resourceId) {
            const HWND hwnd = GetMainWindow();
            icon16 = loadIcon(resourceId, 16);
            icon32 = loadIcon(resourceId, 48);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) icon16);
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM) icon32);
        }

        void InterceptWndProc() {
            HWND hwnd = GetMainWindow();
            if (hwnd) {
                SetWindowSubclass(hwnd, wndProc, 1001, 0);
            }
        }

        void SetMinimizeToTray(bool enabled) {
            minimizeToTray = enabled;
        }

        void SetAppTitle(const std::string& title) {
            appTitle = title;
        }

        bool AlreadyRunning() {
            return !IsDebuggerPresent() && (runningMutexLastError == ERROR_ALREADY_EXISTS);
        }

        void ShowOtherInstance(const std::string& title) {
            HWND otherHwnd = findOtherProcessMainWindow(title);
            if (otherHwnd) {
                SendMessage(otherHwnd, WM_SHOW_OTHER_INSTANCE, 0, 0);
                ::SetForegroundWindow(otherHwnd);
            }
        }

        void EnableSingleInstance(const std::string& uniqueId) {
            if (!uniqueId.size()) {
                resetMutex();
                return;
            }

            std::string mutexName = "cursespp::" + uniqueId;
            if (mutexName != runingMutexName) {
                resetMutex();
                runingMutexName = mutexName;
                runningMutex = CreateMutexA(nullptr, false, runingMutexName.c_str());
                runningMutexLastError = GetLastError();
            }
        }

        void ConfigureDpiAwareness() {
            typedef HRESULT(__stdcall *SetProcessDpiAwarenessProc)(int);
            static const int ADJUST_DPI_PER_MONITOR = 2;

            HMODULE shcoreDll = LoadLibrary(L"shcore.dll");
            if (shcoreDll) {
                SetProcessDpiAwarenessProc setDpiAwareness =
                    (SetProcessDpiAwarenessProc) GetProcAddress(shcoreDll, "SetProcessDpiAwareness");

                if (setDpiAwareness) {
                    setDpiAwareness(ADJUST_DPI_PER_MONITOR);
                }

                FreeLibrary(shcoreDll);
            }
        }

        int RegisterFont(const std::string& filename) {
            return AddFontResourceEx(u8to16(filename).c_str(), FR_PRIVATE, 0);
        }

        int UnregisterFont(const std::string& filename) {
            return RemoveFontResourceEx(u8to16(filename).c_str(), FR_PRIVATE, 0);
        }
    }
}

#endif