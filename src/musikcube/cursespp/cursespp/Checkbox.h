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

/** @file Checkbox.h @brief A toggleable text label rendered as a checkbox. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/TextLabel.h>
#include <cursespp/IKeyHandler.h>
#include <sigslot/sigslot.h>

namespace cursespp {
    /** @brief A TextLabel that can be toggled between checked and unchecked.
     *
     *  @details Checkbox renders a labeled toggle. The displayed text is
     *  prefixed with a checkbox glyph that reflects the current state. Pressing
     *  Enter/Space or clicking it flips the state and emits CheckChanged. The
     *  original label text is kept separately so it can be re-rendered when the
     *  checked state changes.
     */
    class Checkbox: public cursespp::TextLabel {
        public:
            /** @brief Fired with the checkbox and its new state when toggled. */
            sigslot::signal2<Checkbox*, bool> CheckChanged;

            /** @brief Creates an unchecked checkbox with empty text. */
            Checkbox();
            /** @brief Creates an unchecked checkbox with the given label.
             *  @param value the label text.
             */
            Checkbox(const std::string& value);
            /** @brief Creates an unchecked checkbox with the given label and alignment.
             *  @param value the label text.
             *  @param alignment the text alignment.
             */
            Checkbox(const std::string& value, const text::TextAlign alignment);

            /** @brief Destroys the checkbox. */
            virtual ~Checkbox();

            /** @brief Sets the checked state.
             *  @param checked true to check the box.
             */
            virtual void SetChecked(bool checked);
            /** @brief Returns the current checked state.
             *  @return true if checked.
             */
            virtual bool IsChecked() { return this->checked; }

            /* TextLabel */
            /** @brief Sets the label text (kept separate from the checkbox glyph).
             *  @param value the label text.
             */
            virtual void SetText(const std::string& value) override;
            /** @brief Returns the label text.
             *  @return the label string.
             */
            virtual std::string GetText() override;

            /* Window */
            /** @brief Toggles the checkbox on Enter/Space.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            bool KeyPress(const std::string& key) override;
            /** @brief Toggles the checkbox when clicked.
             *  @param event the translated mouse event.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& event) override;

        private:
            bool checked;                 /**< Whether the box is currently checked. */
            std::string originalText;     /**< The raw label text before glyph decoration. */
    };
}
