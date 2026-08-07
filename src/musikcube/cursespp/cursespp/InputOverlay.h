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

/** @file InputOverlay.h @brief A modal overlay prompting the user for a single line of text. */
#pragma once

#include <cursespp/OverlayBase.h>
#include <cursespp/TextInput.h>

namespace cursespp {
    /** @brief A modal overlay that captures a single line of user input.
     *
     *  @details InputOverlay renders a titled box containing a TextInput. When
     *  the user presses Enter the entered value is validated (if a validator is
     *  installed) and, on success, delivered via the InputAcceptedCallback
     *  before the overlay dismisses itself. Validators are simple interfaces
     *  (see IValidator and NumberValidator) that decide whether the text is
     *  acceptable and provide the error message to show otherwise. As an
     *  overlay it consumes all input while visible.
     */
    class InputOverlay:
        public OverlayBase,
        public sigslot::has_slots<>
    {
        public:
            /** @brief Validates user input before it is accepted.
             *
             *  @details Implementations decide whether the entered text is
             *  acceptable (IsValid) and provide a localized message when it is
             *  not (ErrorMessage). This is the interface used by NumberValidator
             *  and similar validators.
             */
            struct IValidator {
                /** @brief Checks whether the input is acceptable.
                 *  @param input the raw entered text.
                 *  @return true if the input is valid.
                 */
                virtual bool IsValid(const std::string& input) const = 0;
                /** @brief Returns the error message to show for invalid input.
                 *  @return the localized error string.
                 */
                virtual const std::string ErrorMessage() const = 0;
            };

            /** @brief Callback fired with the accepted input string. */
            using InputAcceptedCallback = std::function<void(const std::string&)>;

            /** @brief Creates an empty input overlay. */
            InputOverlay();
            /** @brief Destroys the overlay. */
            virtual ~InputOverlay();

            /** @brief Sets the overlay title.
             *  @param title the title text.
             *  @return *this for chaining.
             */
            InputOverlay& SetTitle(const std::string& title);
            /** @brief Sets the initial text shown in the input field.
             *  @param text the initial value.
             *  @return *this for chaining.
             */
            InputOverlay& SetText(const std::string& text);
            /** @brief Registers the callback that receives the accepted value.
             *  @param cb the InputAcceptedCallback.
             *  @return *this for chaining.
             */
            InputOverlay& SetInputAcceptedCallback(InputAcceptedCallback cb);
            /** @brief Installs a validator for the entered value.
             *  @param validator the IValidator to use.
             *  @return *this for chaining.
             */
            InputOverlay& SetValidator(std::shared_ptr<IValidator> validator);
            /** @brief Sets the overlay width in cells.
             *  @param width the desired width.
             *  @return *this for chaining.
             */
            InputOverlay& SetWidth(int width);
            /** @brief Sets the input mode of the embedded TextInput.
             *  @param mode the IInput::InputMode to use.
             *  @return *this for chaining.
             */
            InputOverlay& SetInputMode(IInput::InputMode mode);
            /** @brief Controls whether an empty value is accepted.
             *  @param allowEmptyValue true to accept empty input.
             *  @return *this for chaining.
             */
            InputOverlay& SetAllowEmptyValue(bool allowEmptyValue);

            /** @brief Arranges the overlay and its contents. */
            virtual void Layout();
            /** @brief Handles typing and Enter/ESC keys.
             *  @param key the normalized key string.
             *  @return true if the key was consumed.
             */
            virtual bool KeyPress(const std::string& key);
            /** @brief Renders the overlay contents. */
            virtual void OnRedraw();

        protected:
            /** @brief Called when the overlay becomes visible or hidden.
             *  @param visible the new visible state.
             */
            virtual void OnVisibilityChanged(bool visible);
            /** @brief Called when Enter is pressed in the input field.
             *  @param input the TextInput that emitted the event.
             */
            virtual void OnInputEnterPressed(TextInput* input);
            /** @brief Called for each key typed into the input field.
             *  @param input the TextInput that emitted the event.
             *  @param key the normalized key string.
             */
            virtual void OnInputKeyPress(TextInput* input, std::string key);

        private:
            void RecalculateSize();

            std::string title, text;                 /**< The overlay title and initial input text. */
            int x, y;                                /**< The overlay position. */
            int width, height;                       /**< The overlay dimensions. */
            int setWidth;                            /**< The caller-requested width, or -1. */
            bool allowEmptyValue;                    /**< Whether empty input is accepted. */
            std::shared_ptr<TextInput> textInput;    /**< The embedded text input widget. */
            std::shared_ptr<IValidator> validator;   /**< The optional input validator. */
            InputAcceptedCallback inputAcceptedCallback; /**< Callback for the accepted value. */
    };
}