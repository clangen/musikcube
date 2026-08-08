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

/** @file TextInput.h @brief A single-line editable text input widget. */
#pragma once

#include <cursespp/curses_config.h>
#include <cursespp/Window.h>
#include <cursespp/IInput.h>
#include <cursespp/IKeyHandler.h>
#include <sigslot/sigslot.h>
#include <vector>

namespace cursespp {
    /** @brief A single-line, editable text field rendered as a curses Window.
     *
     *  @details TextInput implements IInput (a text buffer with a cursor) and
     *  IKeyHandler (so it can consume keystrokes). It supports an optional
     *  InputMode that controls raw/normalized/password handling, an optional
     *  placeholder hint text, and either a boxed or underline Style. Enter
     *  presses emit the EnterPressed signal; every buffer mutation emits
     *  TextChanged. Redraw is deferred: callers should not draw into its
     *  ncurses WINDOW directly, but rely on the toolkit's Invalidate/Redraw
     *  cycle.
     */
    class TextInput:
        public cursespp::Window,
        public cursespp::IKeyHandler,
        public cursespp::IInput
    {
        public:
            /** @brief Fired when the user presses Enter with an enabled enter key. */
            sigslot::signal1<TextInput*> EnterPressed;
            /** @brief Fired with the input and its new text whenever the buffer changes. */
            sigslot::signal2<TextInput*, std::string> TextChanged;

            /** @brief The visual style of the input widget. */
            enum Style { StyleBox, StyleLine };  /**< StyleBox draws a boxed border; StyleLine draws a bottom line. */

            /** @brief Creates a default input.
             *  @param inputMode the initial input mode.
             */
            TextInput(InputMode inputMode = IInput::InputNormal);
            /** @brief Creates an input with an explicit visual style.
             *  @param style the Style to render.
             *  @param inputMode the initial input mode.
             */
            TextInput(Style style, InputMode inputMode = IInput::InputNormal);

            /** @brief Destroys the input. */
            virtual ~TextInput();

            /* IInput */
            /** @brief Applies a keystroke to the buffer.
             *  @param key the normalized key string.
             *  @return true if the key was accepted and applied.
             */
            bool Write(const std::string& key) override;
            /** @brief Returns the number of characters in the buffer.
             *  @return the buffer length.
             */
            size_t Length() noexcept override;
            /** @brief Returns the current cursor position.
             *  @return the zero-based cursor offset.
             */
            size_t Position() override;

            /** @brief Returns the active input mode.
             *  @return the current InputMode.
             */
            InputMode GetInputMode() noexcept override {
                return this->inputMode;
            }

            /* IWindow */
            /** @brief Handles editing keys (backspace, delete, arrows, enter, etc.).
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            bool KeyPress(const std::string& key) override;
            /** @brief Renders the buffer, cursor and optional hint text. */
            void OnRedraw() override;

            /* IMouseHandler */
            /** @brief Positions the cursor on click.
             *  @param event the translated mouse event.
             *  @return true if the event was consumed.
             */
            bool ProcessMouseEvent(const IMouseHandler::Event& event) override;

            /* regular methods we define */
            /** @brief Sets keys that are never passed to the input (e.g. global shortcuts).
             *  @param blacklist the set of normalized keys to ignore.
             */
            void SetRawKeyBlacklist(const std::vector<std::string>&& blacklist);
            /** @brief Controls whether the buffer is truncated when it exceeds the display width.
             *  @param truncate true to truncate the visible text.
             */
            void SetTruncate(bool truncate) noexcept;
            /** @brief Sets the placeholder hint shown when the buffer is empty.
             *  @param hint the hint text.
             */
            void SetHint(const std::string& hint);
            /** @brief Enables or disables the Enter key.
             *  @param enabled true to allow Enter to submit.
             */
            void SetEnterEnabled(bool enabled) noexcept;

            /** @brief Returns the visual style of the input.
             *  @return the current Style.
             */
            Style GetStyle() noexcept {
                return style;
            }

            /* virtual methods we define */
            /** @brief Replaces the buffer contents.
             *  @param value the new text.
             */
            virtual void SetText(const std::string& value);

            /** @brief Returns the current buffer contents.
             *  @return the text string.
             */
            virtual std::string GetText() {
                return this->buffer;
            }

            /** @brief Changes the input mode.
             *  @param inputMode the new InputMode.
             */
            virtual void SetInputMode(InputMode inputMode) noexcept {
                this->inputMode = inputMode;
            };

        private:
            bool OffsetPosition(int delta);

            std::vector<std::string> rawBlacklist;  /**< Keys that are never applied to the buffer. */
            std::string buffer, hintText;           /**< The editable buffer and its placeholder hint. */
            int position;                           /**< The cursor position. */
            bool enterEnabled;                      /**< Whether Enter submits the input. */
            bool truncate;                          /**< Whether long text is truncated on render. */
            size_t bufferLength;                    /**< Cached buffer length. */
            Style style;                            /**< The visual style. */
            InputMode inputMode;                    /**< The active input mode. */
    };
}
