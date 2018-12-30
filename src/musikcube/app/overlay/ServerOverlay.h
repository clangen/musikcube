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

#include <functional>

#include <core/sdk/IPlugin.h>
#include <core/support/Preferences.h>

#include <cursespp/Checkbox.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>
#include <cursespp/OverlayBase.h>
#include <cursespp/ShortcutsWindow.h>

namespace musik {
    namespace cube {
        class ServerOverlay:
            public cursespp::OverlayBase,
            public sigslot::has_slots<>
    {
        public:
            using Callback = std::function<void()>;
            using Plugin = std::shared_ptr<musik::core::sdk::IPlugin>;
            using Prefs = std::shared_ptr<musik::core::Preferences>;

            static void Show(Callback callback);
            static std::shared_ptr<musik::core::sdk::IPlugin> FindServerPlugin();

            virtual void Layout();
            virtual bool KeyPress(const std::string& key);

        private:
            ServerOverlay(Callback callback, Plugin plugin);

            void RecalculateSize();
            void InitViews();
            bool Save();
            void Load();

            Callback callback;
            Plugin plugin;
            Prefs prefs;
            int width, height, x, y;

            std::shared_ptr<cursespp::TextLabel> titleLabel;
            std::shared_ptr<cursespp::Checkbox> enableWssCb, enableHttpCb, enableSyncTransCb;
            std::shared_ptr<cursespp::Checkbox> ipv6Cb;
            std::shared_ptr<cursespp::TextLabel> wssPortLabel, httpPortLabel, pwLabel, transCacheLabel;
            std::shared_ptr<cursespp::TextInput> wssPortInput, httpPortInput, pwInput, transCacheInput;
            std::shared_ptr<cursespp::ShortcutsWindow> shortcuts;
        };
    }
}
