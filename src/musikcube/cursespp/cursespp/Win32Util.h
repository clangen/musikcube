//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/** @file Win32Util.h @brief Windows-specific console/window integration helpers. */
#pragma once

#ifdef WIN32

#include <Windows.h>
#include <string>

namespace cursespp {
    /** @brief Windows-only helpers for integrating the curses console with Win32.
     *
     *  @details These functions bridge the curses terminal and the native
     *  Windows console window: intercepting the window procedure, showing and
     *  hiding the console window, minimizing to the system tray, setting the
     *  window icon/title, enforcing single-instance behavior and configuring
     *  DPI and theme awareness. Font registration lets the app load custom
     *  console fonts. This namespace is only compiled on WIN32 builds.
     */
    namespace win32 {
        /** @brief Hooks the console window's WndProc for message handling. */
        void InterceptWndProc();
        /** @brief Shows the main console window. */
        void ShowMainWindow();
        /** @brief Hides the main console window. */
        void HideMainWindow();
        /** @brief Minimizes the main console window. */
        void Minimize();
        /** @brief Returns the native HWND of the main console window.
         *  @return the window handle.
         */
        HWND GetMainWindow();
        /** @brief Sets the window icon.
         *  @param resourceId the icon resource id.
         */
        void SetIcon(int resourceId);
        /** @brief Sets the console window title.
         *  @param title the title string.
         */
        void SetAppTitle(const std::string& title);
        /** @brief Enables or disables minimize-to-tray behavior.
         *  @param enabled true to allow tray minimization.
         */
        void SetMinimizeToTray(bool enabled);
        /** @brief Registers a unique id so only one instance of the app runs.
         *  @param uniqueId the single-instance identifier.
         */
        void EnableSingleInstance(const std::string& uniqueId);
        /** @brief Returns whether another instance of the app is running.
         *  @return true if another instance is active.
         */
        bool AlreadyRunning();
        /** @brief Brings a previously-started instance to the foreground.
         *  @param title the window title of the running instance.
         */
        void ShowOtherInstance(const std::string& title);
        /** @brief Configures the process for high-DPI awareness. */
        void ConfigureDpiAwareness();
        /** @brief Configures the process for dark/light theme awareness. */
        void ConfigureThemeAwareness();
        /** @brief Registers a custom font with the system.
         *  @param filename path to the font file.
         *  @return zero on success, or a non-zero result code.
         */
        int RegisterFont(const std::string& filename);
        /** @brief Unregisters a previously registered font.
         *  @param filename path to the font file.
         *  @return zero on success, or a non-zero result code.
         */
        int UnregisterFont(const std::string& filename);
    }
}

#endif