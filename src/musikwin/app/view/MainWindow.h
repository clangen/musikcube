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

/**
 * @file MainWindow.h
 * @brief Top-level window (view) for the musicwin application.
 *
 * Derives from win32cpp::TopLevelWindow and provides a Win32 message
 * queue so asynchronous messages can be marshalled onto the UI thread
 * and dispatched to message targets such as the MainController.
 */

#pragma once

#include <win32cpp/TopLevelWindow.hpp>
#include <musikcore/runtime/IMessageQueue.h>

namespace musik { namespace win {
    /** @brief Main application window.
     *  @details Owns a Win32MessageQueue and exposes it through Queue().
     *           Overrides WindowProc to forward translated messages. */
    class MainWindow : public win32cpp::TopLevelWindow {
        private:
            /** @brief IMessageQueue implementation backed by a Win32 window. */
            class Win32MessageQueue;

        public:
            /** @brief Creates the window.
             *  @param windowTitle the title displayed in the title bar */
            MainWindow(const win32cpp::uichar* windowTitle);
            /** @brief Destroys the window and its message queue. */
            virtual ~MainWindow();

            /** @brief Win32 message procedure for the main window.
             *  @param message the window message identifier
             *  @param wParam the WPARAM payload of the message
             *  @param lParam the LPARAM payload of the message
             *  @return the result of handling the message */
            virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);

            /** @brief Returns the message queue associated with this window.
             *  @return reference to the window's IMessageQueue */
            musik::core::runtime::IMessageQueue& Queue();

        private:
            Win32MessageQueue* queue; /**< the message queue bridging the Win32 message loop */
    };
} }