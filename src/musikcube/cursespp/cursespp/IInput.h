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

/** @file IInput.h @brief Interface for single- or multi-line text input widgets. */
#pragma once

namespace cursespp {
    /** @brief Contract for widgets that accept typed text.
     *
     *  @details IInput abstracts the behavior of text-entry widgets such as
     *  SingleLineEntry and MultiLineEntry. Implementors own a character buffer
     *  and a cursor position, and translate incoming keystrokes into edits of
     *  that buffer. The input mode determines whether typed characters are
     *  stored verbatim, normalized, or masked (as in a password field).
     */
    class IInput {
        public:
            /** @brief The behavior of a text input widget. */
            enum InputMode {
                InputRaw,       /**< Characters are stored exactly as typed. */
                InputNormal,    /**< Characters are processed through the default normalizer (e.g. removing control characters). */
                InputPassword   /**< Typed characters are masked so the buffer contents are not displayed. */
            };

            virtual ~IInput() { }

            /** @brief Injects a keystroke into the widget, editing the buffer.
             *  @param key the normalized string representation of the key.
             *  @return true if the input accepted and applied the keystroke.
             */
            virtual bool Write(const std::string& key) = 0;

            /** @brief Returns the number of characters currently in the buffer.
             *  @return the buffer length in characters.
             */
            virtual size_t Length() = 0;

            /** @brief Returns the current cursor position within the buffer.
             *  @return the zero-based cursor offset.
             */
            virtual size_t Position() = 0;

            /** @brief Returns the current input mode of the widget.
             *  @return the active InputMode.
             */
            virtual InputMode GetInputMode() = 0;
    };
}
