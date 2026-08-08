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

/** @file TextLabel.h @brief A non-editable, focusable label window. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Window.h>
#include <cursespp/IInput.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/Text.h>
#include <sigslot/sigslot.h>

namespace cursespp {
    /** @brief A Window that renders a single line of read-only text.
     *
     *  @details TextLabel displays a static string with configurable
     *  alignment and bold styling. Because it implements IKeyHandler it can
     *  participate in focus navigation; when focused, pressing Enter/Space
     *  emits the Activated signal. It relies on the toolkit's deferred redraw
     *  cycle -- call SetText() and let the framework repaint.
     */
    class TextLabel:
        public cursespp::Window,
        public cursespp::IKeyHandler
    {
        public:
            /** @brief Fired when the label is activated via Enter/Space while focused. */
            sigslot::signal1<TextLabel*> Activated;

            /** @brief Creates an empty left-aligned label. */
            TextLabel();
            /** @brief Creates a left-aligned label with the given text.
             *  @param value the label text.
             */
            TextLabel(const std::string& value);
            /** @brief Creates a label with the given text and alignment.
             *  @param value the label text.
             *  @param alignment the text alignment.
             */
            TextLabel(const std::string& value, const text::TextAlign alignment);

            /** @brief Destroys the label. */
            virtual ~TextLabel();

            /* IWindow */
            /** @brief Renders the label text. */
            void OnRedraw() override;
            /** @brief Activates the label on Enter/Space.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            bool KeyPress(const std::string& key) override;
            /** @brief Activates the label when clicked.
             *  @param event the translated mouse event.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& event) override;

            /* virtual methods we define */
            /** @brief Sets whether the label text is rendered in bold.
             *  @param bold true to use bold attributes.
             */
            virtual void SetBold(bool bold);
            /** @brief Returns whether the label is rendered in bold.
             *  @return true if bold.
             */
            virtual bool IsBold() noexcept { return this->bold; }
            /** @brief Returns the display width of the label text.
             *  @return the width in terminal cells.
             */
            virtual size_t Length() { return u8cols(this->buffer); }
            /** @brief Replaces the label text (keeping the current alignment).
             *  @param value the new text.
             */
            virtual void SetText(const std::string& value);
            /** @brief Replaces the label text and its alignment.
             *  @param value the new text.
             *  @param alignment the new alignment.
             */
            virtual void SetText(const std::string& value, const text::TextAlign alignment);
            /** @brief Returns the current label text.
             *  @return the text string.
             */
            virtual std::string GetText() { return this->buffer; }
            /** @brief Sets the text alignment.
             *  @param alignment the new TextAlign.
             */
            virtual void SetAlignment(const text::TextAlign alignment);

        private:
            void ApplyDefaultStyle();

            std::string buffer;          /**< The rendered text. */
            text::TextAlign alignment;   /**< The current text alignment. */
            bool bold;                   /**< Whether the text is rendered bold. */
    };
}
