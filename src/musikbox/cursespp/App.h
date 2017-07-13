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

#include "IViewRoot.h"
#include "ILayout.h"
#include "IInput.h"
#include "IKeyHandler.h"
#include "OverlayStack.h"
#include "Colors.h"

namespace cursespp {
    class App {
        public:
            using MainKeyHandler = std::function<bool(const std::string&)>;
            using ResizeHandler = std::function<void()>;

            App(const std::string& title);
            ~App(); /* do not subclass */

            App(const App& other) = delete;
            App& operator=(const App& other) = delete;

            void SetKeyHandler(MainKeyHandler handler);
            void SetResizeHandler(ResizeHandler handler);
            void SetColorMode(Colors::Mode mode);
            void SetColorTheme(const std::string& fn);
            void SetMinimumSize(int width, int height);
            bool IsOverlayVisible() { return this->state.overlay != nullptr; }
            void SetMinimizeToTray(bool minimizeToTray);
            void Minimize();
            void Restore();

#ifdef WIN32
            static bool Running(const std::string& uniqueId, const std::string& title);
            static bool Running(const std::string& title);
            void SetIcon(int resourceId);
            void SetSingleInstanceId(const std::string& uniqueId);
#endif

            void Run(ILayoutPtr layout);
            void ChangeLayout(ILayoutPtr layout);

            static int64_t Now();
            static OverlayStack& Overlays();

        private:
            struct WindowState {
                ILayoutPtr overlay;
                IWindow* overlayWindow;
                ILayoutPtr layout;
                IWindowPtr focused;
                IViewRoot* viewRoot;
                IInput* input;
                IKeyHandler* keyHandler;

                WindowState() {
                    this->overlayWindow = nullptr;
                    this->viewRoot = nullptr;
                    this->input = nullptr;
                    this->keyHandler = nullptr;
                }

                inline ILayoutPtr ActiveLayout() {
                    /* if there's a visible overlay, it's always the current
                    layout and will consume all key events */
                    return overlay ? overlay : layout;
                }
            };

            void UpdateFocusedWindow(IWindowPtr window);
            void EnsureFocusIsValid();
            void CheckShowOverlay();
            void FocusNextInLayout();
            void FocusPrevInLayout();
            void OnResized();

            WindowState state;
            MainKeyHandler keyHandler;
            ResizeHandler resizeHandler;
            Colors::Mode colorMode;
            std::string colorTheme;
            int minWidth, minHeight;

#ifdef WIN32
            int iconId;
            std::string uniqueId, appTitle;
#endif
    };
}
