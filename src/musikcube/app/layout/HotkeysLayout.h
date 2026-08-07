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
 * @file HotkeysLayout.h
 * @brief Layout that lists the keyboard hotkey mappings.
 * @details Displays the configured hotkey bindings in a scrollable list and
 *          lets the user activate an entry to reassign it.
 */

#include <cursespp/LayoutBase.h>
#include <cursespp/ListWindow.h>
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/ITopLevelLayout.h>
#include <vector>

namespace musik {
    namespace cube {
        /**
         * @brief Hotkey list layout.
         * @details Shows all hotkey bindings as a list of entries. Activating
         *          an entry opens the flow to reassign that hotkey.
         */
        class HotkeysLayout :
            public cursespp::LayoutBase,
            public cursespp::ITopLevelLayout,
            public sigslot::has_slots<>
        {
            public:
                DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(HotkeysLayout)

                /**
                 * @brief Creates an empty hotkeys layout.
                 */
                HotkeysLayout();

                /* ITopLevelLayout */
                /**
                 * @brief Attaches the shortcuts window shown at the bottom.
                 * @param shortcuts the shortcuts window
                 */
                void SetShortcutsWindow(
                    cursespp::ShortcutsWindow* shortcuts) override;

                /* IWindow */
                /**
                 * @brief Handles keyboard input.
                 * @param kn the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                bool KeyPress(const std::string& kn) override;
                /**
                 * @brief Positions and lays out the child windows.
                 */
                void OnLayout() override;

            private:
                void OnEntryActivated(cursespp::ListWindow* w, size_t index);

                std::shared_ptr<cursespp::ListWindow> listWindow; /**< the hotkey list window */
                cursespp::ShortcutsWindow* shortcuts;             /**< the shortcuts window, not owned */
        };
    }
}