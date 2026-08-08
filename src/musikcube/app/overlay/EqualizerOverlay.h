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

/**
 * @file EqualizerOverlay.h
 * @brief Overlay for controlling the equalizer effect plugin.
 * @details Lets the user enable or disable the equalizer plugin, select a
 *          band from the list, and adjust its gain using the left and right
 *          arrow keys. Changes are persisted to preferences.
 */

#include <musikcore/sdk/IPlugin.h>
#include <musikcore/support/Preferences.h>
#include <cursespp/OverlayBase.h>
#include <cursespp/Checkbox.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/ShortcutsWindow.h>
#include <sigslot/sigslot.h>
#include <memory>

namespace musik {
    namespace cube {
        /**
         * @brief Equalizer control overlay.
         * @details Displays an enable checkbox, a list of frequency bands and
         *          a shortcuts bar. The selected band is boosted or cut with
         *          the left and right arrow keys, and the gain of each band is
         *          rendered inside the list. Implements OverlayBase so it can
         *          be shown on top of the current layout.
         */
        class EqualizerOverlay:
            public cursespp::OverlayBase,
            public sigslot::has_slots<>
        {
            public:
                using Plugin = std::shared_ptr<musik::core::sdk::IPlugin>;
                using Prefs = std::shared_ptr<musik::core::Preferences>;

                /**
                 * @brief Creates the overlay and loads the equalizer state.
                 */
                EqualizerOverlay();
                /**
                 * @brief Destroys the overlay and its child views.
                 */
                virtual ~EqualizerOverlay();

                /**
                 * @brief Shows the equalizer overlay.
                 * @details If no equalizer plugin is installed the overlay
                 *          shows an informational message instead.
                 */
                static void ShowOverlay();
                /**
                 * @brief Finds the installed equalizer plugin.
                 * @return the equalizer plugin, or nullptr if not installed
                 */
                static std::shared_ptr<musik::core::sdk::IPlugin> FindPlugin();

                /**
                 * @brief Positions and lays out the child windows.
                 */
                virtual void Layout() override;
                /**
                 * @brief Handles keyboard input.
                 * @param key the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                virtual bool KeyPress(const std::string& key) override;
                /**
                 * @brief Processes runtime messages.
                 * @param message the message to process
                 */
                virtual void ProcessMessage(musik::core::runtime::IMessage &message) override;

            private:
                /**
                 * @brief Scroll adapter that renders the equalizer bands.
                 * @details Each row shows the band label and the current gain
                 *          value, read from the persisted preferences.
                 */
                class BandsAdapter : public cursespp::ScrollAdapterBase {
                    public:
                        /**
                         * @brief Creates the adapter with the given
                         *        preferences.
                         * @param prefs preferences used to read band gains
                         */
                        BandsAdapter(Prefs prefs);
                        /**
                         * @brief Destroys the adapter.
                         */
                        virtual ~BandsAdapter();
                        /**
                         * @brief Returns the number of bands.
                         * @return the number of equalizer bands
                         */
                        virtual size_t GetEntryCount() override;
                        /**
                         * @brief Returns the entry for the given index.
                         * @param window the owning scrollable window
                         * @param index the band index
                         * @return the entry to render
                         */
                        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                    private:
                        Prefs prefs; /**< preferences used to read band gains */
                };

                /**
                 * @brief Adjusts the selected band gain by the given delta.
                 * @param delta the gain change in decibels
                 */
                void UpdateSelectedBand(double delta);
                /**
                 * @brief Pushes the current gains to the plugin and redraws.
                 */
                void NotifyAndRedraw();

                /**
                 * @brief Handles the enable checkbox toggle.
                 * @param cb the checkbox that changed
                 * @param checked true if the equalizer was enabled
                 */
                void OnEnabledChanged(cursespp::Checkbox* cb, bool checked);

                Plugin plugin;                          /**< the equalizer plugin instance */
                Prefs prefs;                            /**< preferences holding the gains */
                std::shared_ptr<BandsAdapter> adapter;  /**< adapter feeding the band list */
                std::shared_ptr<cursespp::Checkbox> enabledCb; /**< the enable checkbox */
                std::shared_ptr<cursespp::ListWindow> listView; /**< the band list window */
                std::shared_ptr<cursespp::ShortcutsWindow> shortcuts; /**< the shortcuts window */
        };
    }
}
