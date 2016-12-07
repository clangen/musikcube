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

#include "ILayout.h"
#include "IInput.h"
#include "IKeyHandler.h"
#include "OverlayStack.h"

namespace cursespp {
    class App {
        public:
            using MainKeyHandler = std::function<bool(const std::string&)>;
            using ResizeHandler = std::function<void()>;

            App(const std::string& title);
            ~App(); /* do not subclass */

            void SetKeyHandler(MainKeyHandler handler);
            void SetResizeHandler(ResizeHandler handler);
            void SetCustomColorsDisabled(bool disable);
            void SetMinimumSize(int width, int height);
            bool IsOverlayVisible() { return this->state.overlay != nullptr; }

            void Run(ILayoutPtr layout);
            void ChangeLayout(ILayoutPtr layout);

            static int64 Now();
            static OverlayStack& Overlays();

        private:
            struct WindowState {
                ILayoutPtr overlay;
                ILayoutPtr layout;
                IWindowPtr focused;
                IInput* input;
                IKeyHandler* keyHandler;

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
            bool disableCustomColors;
            int minWidth, minHeight;
    };
}
