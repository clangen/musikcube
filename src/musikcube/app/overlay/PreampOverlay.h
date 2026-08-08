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
 * @file PreampOverlay.h
 * @brief Overlay for configuring the preamp gain and replay gain mode.
 * @details Lets the user set the global preamp value and pick the replay
 *          gain mode. The values are persisted to preferences and applied
 *          to the playback service.
 */

#include <functional>

#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/support/Preferences.h>

#include <cursespp/Checkbox.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>
#include <cursespp/OverlayBase.h>
#include <cursespp/ShortcutsWindow.h>

namespace musik {
    namespace cube {
        /**
         * @brief Preamp and replay gain configuration overlay.
         * @details Shows a title, the current preamp value and a dropdown for
         *          the replay gain mode. Pressing enter saves the settings
         *          and invokes the callback. Implements OverlayBase.
         */
        class PreampOverlay:
            public cursespp::OverlayBase,
            public sigslot::has_slots<>
    {
        public:
            using Callback = std::function<void()>;

            /**
             * @brief Shows the preamp overlay for the given playback service.
             * @param playback the playback service to configure
             * @param callback invoked when the overlay is closed
             */
            static void Show(
                musik::core::sdk::IPlaybackService& playback,
                Callback callback);

            /**
             * @brief Positions and lays out the child views.
             */
            virtual void Layout();
            /**
             * @brief Handles keyboard input.
             * @param key the key sequence that was pressed
             * @return true if the event was consumed
             */
            virtual bool KeyPress(const std::string& key);

            /**
             * @brief Opens the replay gain mode chooser.
             * @param label the replay gain dropdown label
             */
            void OnReplayGainPressed(cursespp::TextLabel* label);

        private:
            /**
             * @brief Creates the overlay bound to the given playback service.
             * @param playback the playback service to configure
             * @param callback invoked when the overlay is closed
             */
            PreampOverlay(
                musik::core::sdk::IPlaybackService& playback,
                Callback callback);

            void RecalculateSize();
            void InitViews();
            bool Save();
            void Load();

            Callback callback;                                              /**< invoked when the overlay closes */
            int width, height, x, y;                                        /**< cached overlay geometry */

            musik::core::sdk::IPlaybackService& playback;                   /**< the playback service being configured */
            std::shared_ptr<musik::core::Preferences> prefs;                /**< preferences used to persist the settings */
            std::shared_ptr<cursespp::TextLabel> titleLabel, preampLabel, replayGainDropdown; /**< static labels */
            std::shared_ptr<cursespp::TextInput> preampInput;               /**< the preamp value input */
            std::shared_ptr<cursespp::ShortcutsWindow> shortcuts;           /**< the shortcuts window */
        };
    }
}
