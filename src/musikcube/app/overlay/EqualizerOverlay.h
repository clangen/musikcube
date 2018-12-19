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

#include <core/sdk/IPlugin.h>
#include <core/support/Preferences.h>
#include <cursespp/OverlayBase.h>
#include <cursespp/Checkbox.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>
#include <sigslot/sigslot.h>
#include <memory>

namespace musik {
    namespace cube {
        class EqualizerOverlay: public cursespp::OverlayBase, public sigslot::has_slots<>
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            , public std::enable_shared_from_this<EqualizerOverlay>
#endif
        {
            public:
                using Plugin = std::shared_ptr<musik::core::sdk::IPlugin>;
                using Prefs = std::shared_ptr<musik::core::Preferences>;

                EqualizerOverlay();
                virtual ~EqualizerOverlay();

                static void ShowOverlay();
                static std::shared_ptr<musik::core::sdk::IPlugin> FindPlugin();

                virtual void Layout() override;
                virtual bool KeyPress(const std::string& key) override;
                virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            private:
                class BandsAdapter : public cursespp::ScrollAdapterBase {
                    public:
                        BandsAdapter(Prefs prefs);
                        virtual ~BandsAdapter();
                        virtual size_t GetEntryCount() override;
                        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                    private:
                        Prefs prefs;
                };

                bool CanScroll(int listViewHeight);
                void UpdateSelectedBand(double delta);

                void OnEnabledChanged(cursespp::Checkbox* cb, bool checked);

                Plugin plugin;
                Prefs prefs;
                std::shared_ptr<BandsAdapter> adapter;
                std::shared_ptr<cursespp::TextLabel> titleLabel;
                std::shared_ptr<cursespp::Checkbox> enabledCb;
                std::shared_ptr<cursespp::ListWindow> listView;
        };
    }
}
