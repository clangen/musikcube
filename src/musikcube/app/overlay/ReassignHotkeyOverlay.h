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
 * @file ReassignHotkeyOverlay.h
 * @brief Overlay that lets the user reassign a keyboard hotkey.
 * @details Shows the name of the hotkey being changed and a text input that
 *          captures the new key sequence. When the user confirms, the
 *          binding is saved and the callback is invoked with the new key.
 */

#include <functional>

#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>
#include <cursespp/OverlayBase.h>
#include <cursespp/ShortcutsWindow.h>
#include <app/util/Hotkeys.h>

namespace musik {
    namespace cube {
        /**
         * @brief Hotkey reassignment overlay.
         * @details Displays the hotkey being edited, captures the new key
         *          sequence and stores it through the Hotkeys helpers.
         */
        class ReassignHotkeyOverlay:
            public cursespp::OverlayBase,
            public sigslot::has_slots<>
    {
        public:
            using Callback = std::function<void(std::string)>;

            /**
             * @brief Shows the reassignment overlay for the given hotkey.
             * @param id the hotkey to reassign
             * @param callback invoked with the new key sequence
             */
            static void Show(Hotkeys::Id id, Callback callback);

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

        private:
            /**
             * @brief Creates the overlay for the given hotkey.
             * @param id the hotkey to reassign
             * @param callback invoked with the new key sequence
             */
            ReassignHotkeyOverlay(Hotkeys::Id id, Callback callback);

            void RecalculateSize();
            void InitViews();

            Hotkeys::Id id;                                                 /**< the hotkey being reassigned */
            Callback callback;                                              /**< invoked when the overlay closes */
            int width, height, x, y;                                        /**< cached overlay geometry */
            std::shared_ptr<cursespp::TextLabel> titleLabel, hotkeyLabel;   /**< static labels */
            std::shared_ptr<cursespp::TextInput> hotkeyInput;               /**< the new key sequence input */
            std::shared_ptr<cursespp::ShortcutsWindow> shortcuts;           /**< the shortcuts window */
        };
    }
}
