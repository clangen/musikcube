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

#include <cursespp/App.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/ITopLevelLayout.h>

#include <sigslot/sigslot.h>

namespace cursespp {
    class AppLayout:
        public cursespp::LayoutBase,
        public sigslot::has_slots<>
    {
        public:
            AppLayout(cursespp::App& app);

            virtual ~AppLayout();

            virtual bool KeyPress(const std::string& key) override;
            virtual void OnLayout() override;
            virtual cursespp::IWindowPtr GetFocus() override;
            virtual cursespp::IWindowPtr FocusNext() override;
            virtual cursespp::IWindowPtr FocusPrev() override;

            void SetLayout(std::shared_ptr<cursespp::LayoutBase> layout);

            void SetAutoHideCommandBar(bool autoHide);
            bool GetAutoHideCommandBar();

        protected:
            virtual void SetPadding(size_t t, size_t l, size_t b, size_t r);

        private:
            void Initialize();

            void EnableDemoModeIfNecessary();

            cursespp::IWindowPtr BlurShortcuts();
            void FocusShortcuts();

            std::shared_ptr<cursespp::ShortcutsWindow> shortcuts;
            std::shared_ptr<cursespp::LayoutBase> layout;
            std::shared_ptr<cursespp::TextLabel> hotkey;
            cursespp::IWindowPtr lastFocus;
            ITopLevelLayout* topLevelLayout;
            size_t paddingT{0}, paddingL{0}, paddingB{0}, paddingR{0};
            bool shortcutsFocused;
            bool autoHideCommandBar{ false };
    };
}
