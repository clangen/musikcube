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

/** @file ShortcutsWindow.h @brief A window that renders a row of key-binding shortcuts. */
#pragma once

#include <cursespp/IKeyHandler.h>
#include <cursespp/Window.h>
#include <cursespp/Text.h>
#include <functional>

namespace cursespp {
    /** @brief A Window that displays a row of contextual key shortcuts ("command bar").
     *
     *  @details ShortcutsWindow renders a set of (key, description) pairs along
     *  a single row, typically at the bottom of the screen as a contextual
     *  legend. One entry may be marked active and highlighted. The window is
     *  also an IKeyHandler: when focused, pressing one of the listed keys
     *  triggers the ChangedCallback. It owns a Text helper to render the row
     *  with the configured alignment (left, center or right).
     */
    class ShortcutsWindow:
        public cursespp::Window,
        public cursespp::IKeyHandler
    {
        public:
            /** @brief Callback fired when one of the listed shortcut keys is pressed.
             *  @param key the normalized key string that was activated.
             */
            using ChangedCallback = std::function<void (std::string /* key */)>;

            /** @brief Creates an empty shortcuts window. */
            ShortcutsWindow();
            /** @brief Destroys the window. */
            virtual ~ShortcutsWindow();

            /** @brief Sets the horizontal alignment of the rendered shortcuts.
             *  @param alignment the text::TextAlign to use.
             */
            void SetAlignment(text::TextAlign alignment);

            /** @brief Adds a shortcut entry.
             *  @param key the normalized key string (e.g. "F2").
             *  @param description the human-readable label.
             *  @param attrs optional curses attributes, or -1 for defaults.
             */
            void AddShortcut(
                const std::string& key,
                const std::string& description,
                int64_t attrs = -1);

            /** @brief Registers the callback invoked when a shortcut is activated.
             *  @param callback the ChangedCallback to invoke.
             */
            void SetChangedCallback(ChangedCallback callback);

            /** @brief Removes all shortcuts. */
            void RemoveAll();
            /** @brief Marks the given shortcut as the active/highlighted entry.
             *  @param key the key string to highlight.
             */
            void SetActive(const std::string& key);

            /** @brief Handles a key press and activates a matching shortcut.
             *  @param key the normalized key string.
             *  @return true if the key matched a shortcut.
             */
            bool KeyPress(const std::string& key) override;
            /** @brief Handles a click on a shortcut entry.
             *  @param mouseEvent the translated mouse event.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& mouseEvent) override;

        protected:
            /** @brief Renders the shortcut row. */
            void OnRedraw() override;
            /** @brief Marks the row dirty when focus changes. */
            void OnFocusChanged(bool focused) override;

        private:
            size_t CalculateLeftPadding();
            int getActiveIndex();

            struct Position {
                int offset{ 0 }, width{ 0 };   /**< Screen offset and width of an entry. */
            };

            struct Entry {
                Entry(const std::string& key, const std::string& desc, int64_t attrs = -1) {
                    this->key = key;
                    this->description = desc;
                    this->attrs = attrs;
                }

                Position position;      /**< Cached geometry of the entry. */
                std::string key;        /**< The normalized key string. */
                std::string description; /**< The human-readable label. */
                int64_t attrs;          /**< Curses attributes for the entry. */
            };

            using EntryList = std::vector<std::shared_ptr<Entry>>;

            ChangedCallback changedCallback;  /**< Callback fired when a shortcut is activated. */
            EntryList entries;                /**< The ordered list of shortcuts. */
            std::string activeKey, originalKey; /**< Currently highlighted key and its original binding. */
            text::TextAlign alignment;        /**< Horizontal alignment of the row. */
    };
}
