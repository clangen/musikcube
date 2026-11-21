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

#pragma once

#include <musikcore/equalizer.hpp>
#include <cursespp/OverlayBase.h>
#include <cursespp/Checkbox.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/ShortcutsWindow.h>
#include <sigslot/sigslot.h>
#include <memory>

namespace musik {
    namespace cube {
        class EqualizerOverlay:
            public cursespp::OverlayBase,
            public sigslot::has_slots<>
        {
            public:
                explicit EqualizerOverlay(const mc_equalizer_state& state);
                virtual ~EqualizerOverlay();

                static void ShowOverlay();

                virtual void Layout() override;
                virtual bool KeyPress(const std::string& key) override;
                virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            private:
                class BandsAdapter : public cursespp::ScrollAdapterBase {
                    public:
                        explicit BandsAdapter(const mc_equalizer_state& state);
                        virtual ~BandsAdapter();
                        virtual size_t GetEntryCount() override;
                        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                    private:
                        const mc_equalizer_state& state;
                };

                void UpdateSelectedBand(double delta);
                void Refresh();

                void OnEnabledChanged(cursespp::Checkbox* cb, bool checked);

                mc_equalizer_state state;
                std::shared_ptr<BandsAdapter> adapter;
                std::shared_ptr<cursespp::Checkbox> enabledCb;
                std::shared_ptr<cursespp::ListWindow> listView;
                std::shared_ptr<cursespp::ShortcutsWindow> shortcuts;
        };
    }
}
