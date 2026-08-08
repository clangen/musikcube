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

/** @file DialogOverlay.h @brief A modal dialog overlay with a message and shortcut buttons. */
#pragma once

#include <cursespp/OverlayBase.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ShortcutsWindow.h>

#include <vector>
#include <map>

namespace cursespp {
    /** @brief A modal dialog rendered as an overlay with a title, message and buttons.
     *
     *  @details DialogOverlay displays a framed box with a title, a word-wrapped
     *  message and a row of shortcut "buttons" rendered via a ShortcutsWindow.
     *  Buttons are keyed by the key string that activates them (e.g. "F2").
     *  Pressing a button fires its ButtonCallback and optionally auto-dismisses
     *  the dialog; ESC can be configured to dismiss without a callback. The
     *  dialog is layered on top of the main layout through the OverlayStack, so
     *  it intercepts all input while visible.
     */
    class DialogOverlay: public OverlayBase {
        public:
            /** @brief Callback fired when a dialog button is activated. */
            using ButtonCallback = std::function<void(std::string key)>;
            /** @brief Callback fired when the dialog is dismissed. */
            using DismissCallback = std::function<void()>;

            /** @brief Creates an empty dialog. */
            DialogOverlay();
            /** @brief Destroys the dialog. */
            virtual ~DialogOverlay();

            /** @brief Sets the dialog title.
             *  @param title the title text.
             *  @return *this for chaining.
             */
            DialogOverlay& SetTitle(const std::string& title);
            /** @brief Sets the message body (word-wrapped to fit the dialog).
             *  @param message the message text.
             *  @return *this for chaining.
             */
            DialogOverlay& SetMessage(const std::string& message);

            /** @brief Removes all buttons.
             *  @return *this for chaining.
             */
            DialogOverlay& ClearButtons();

            /** @brief Adds a button bound to a key.
             *  @param rawKey the raw (unnormalized) key, e.g. "F2".
             *  @param key the normalized key string.
             *  @param caption the button label.
             *  @param callback the callback fired when the button is activated.
             *  @return *this for chaining.
             */
            DialogOverlay& AddButton(
                const std::string& rawKey,
                const std::string& key,
                const std::string& caption,
                ButtonCallback callback = ButtonCallback());

            /** @brief Registers a callback invoked when the dialog is dismissed.
             *  @param dismissCb the DismissCallback.
             *  @return *this for chaining.
             */
            DialogOverlay& OnDismiss(DismissCallback dismissCb);

            /** @brief Controls whether the dialog auto-dismisses after a button press.
             *  @param dismiss true to dismiss automatically.
             *  @return *this for chaining.
             */
            DialogOverlay& SetAutoDismiss(bool dismiss = true);

            /** @brief Controls whether ESC dismisses the dialog.
             *  @param dismiss true to dismiss on ESC.
             *  @return *this for chaining.
             */
            DialogOverlay& SetDismissOnEscKey(bool dismiss = true);

            /** @brief Arranges the dialog and its contents. */
            virtual void Layout();
            /** @brief Routes key presses to button activations or dismissal.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            virtual bool KeyPress(const std::string& key);
            /** @brief Renders the dialog contents. */
            virtual void OnRedraw();

        protected:
            /** @brief Called after the dialog is dismissed; subclasses may override. */
            virtual void OnDismissed();

        private:
            void RecalculateSize();
            bool ProcessKey(const std::string& key);

            std::string title;                              /**< The dialog title. */
            std::string message;                            /**< The raw message text. */
            std::vector<std::string> messageLines;          /**< Word-wrapped message lines. */
            std::shared_ptr<ShortcutsWindow> shortcuts;     /**< Renders the button row. */
            int width, height;                              /**< The dialog dimensions in cells. */
            bool autoDismiss;                               /**< Whether button activation dismisses the dialog. */
            bool escDismiss;                                /**< Whether ESC dismisses the dialog. */
            DismissCallback dismissCb;                      /**< Callback fired on dismissal. */

            std::map<std::string, ButtonCallback> buttons;  /**< Key -> button callback map. */
    };
}