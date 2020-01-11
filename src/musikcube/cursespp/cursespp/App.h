//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <queue>
#include <functional>
#include <cursespp/ILayout.h>
#include <cursespp/IInput.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/OverlayStack.h>
#include <cursespp/Colors.h>

namespace cursespp {
    class App {
        public:
            using KeyHandler = std::function<bool(const std::string&)>;
            using ResizeHandler = std::function<void()>;

            App(const std::string& title);
            ~App(); /* do not subclass */

            App(const App& other) = delete;
            App& operator=(const App& other) = delete;

            void SetKeyHandler(KeyHandler handler);
            void SetKeyHook(KeyHandler hook);
            void SetResizeHandler(ResizeHandler handler);
            void SetColorMode(Colors::Mode mode);
            void SetColorBackgroundType(Colors::BgType bgType);
            void SetColorTheme(const std::string& name);
            void SetMinimumSize(int width, int height);
            void SetMouseEnabled(bool enabled);
            bool IsOverlayVisible() { return this->state.overlay != nullptr; }
            void SetMinimizeToTray(bool minimizeToTray);
            void SetTitle(const std::string& title);
            std::string GetQuitKey();
            void SetQuitKey(const std::string& kn);
            void Minimize();
            void Restore();

#ifdef WIN32
            static bool Running(const std::string& uniqueId, const std::string& title);
            static bool Running(const std::string& title);
            void SetIcon(int resourceId);
            void SetSingleInstanceId(const std::string& uniqueId);
            static bool RegisterFont(const std::string& filename);
            static void SetDefaultFontface(const std::string& fontface);
            static void SetDefaultMenuVisibility(bool visible);
#endif

            void Run(ILayoutPtr layout);
            void ChangeLayout(ILayoutPtr layout);
            ILayoutPtr GetLayout();
            void InjectKeyPress(const std::string& key);
            void Quit();

            static App& Instance();

            static int64_t Now();
            static OverlayStack& Overlays();

        private:
            struct WindowState {
                ILayoutPtr overlay;
                IWindow* overlayWindow{ nullptr };
                ILayoutPtr layout;
                IWindowPtr focused;
                IWindow* rootWindow{ nullptr };
                IInput* input{ nullptr };
                IKeyHandler* keyHandler{ nullptr };

                inline ILayoutPtr ActiveLayout() {
                    /* if there's a visible overlay, it's always the current
                    layout and will consume all key events */
                    return overlay ? overlay : layout;
                }
            };

            void InitCurses();
            void UpdateFocusedWindow(IWindowPtr window);
            void EnsureFocusIsValid();
            void CheckShowOverlay();
            void FocusNextInLayout();
            void FocusPrevInLayout();
            void OnResized();

            std::queue<std::string> injectedKeys;
            WindowState state;
            KeyHandler keyHandler, keyHook;
            ResizeHandler resizeHandler;
            Colors::Mode colorMode { Colors::Palette };
            Colors::BgType bgType { Colors::Theme };
            std::string colorTheme;
            std::string quitKey{"^D"};
            int minWidth, minHeight;
            bool mouseEnabled{true};
            bool quit{false}, initialized{false};
            std::string appTitle;

#ifdef WIN32
            int iconId;
            std::string uniqueId;
#endif
    };
}
