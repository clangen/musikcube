//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include <Windows.h>

#ifdef WIN32

static std::basic_string<TCHAR> className = "Curses_App";
static HWND mainWindow = nullptr;

static void findMainWindow() {
    static TCHAR buffer[256];

    if (mainWindow == nullptr) {
        DWORD dwProcID = GetCurrentProcessId();
        HWND hWnd = GetTopWindow(GetDesktopWindow());
        while (hWnd) {
            DWORD dwWndProcID = 0;
            GetWindowThreadProcessId(hWnd, &dwWndProcID);
            if (dwWndProcID == dwProcID) {
                GetClassName(hWnd, buffer, sizeof(buffer));
                if (className == std::string(buffer)) {
                    mainWindow = hWnd;
                    return;
                }
            }
            hWnd = GetNextWindow(hWnd, GW_HWNDNEXT);
        }
    }
}

namespace musik {
    namespace box {
        namespace win32 {
            void ShowMainWindow() {
                findMainWindow();
                if (mainWindow) {
                    ShowWindow(mainWindow, SW_SHOWNORMAL);
                }
            }

            void HideMainWindow() {
                findMainWindow();
                if (mainWindow) {
                    ShowWindow(mainWindow, SW_HIDE);
                }
            }
        }
    }
}

#endif