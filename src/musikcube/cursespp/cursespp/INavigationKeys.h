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

/** @file INavigationKeys.h @brief Interface for mapping normalized keys to navigation actions. */
#pragma once

#include <string>

namespace cursespp {
    /** @brief Contract for translating key strings into navigation gestures.
     *
     *  @details Curses terminals do not always report navigation keys (arrows,
     *  page up/down, home/end, tab, etc.) in a uniform way. Implementors of
     *  INavigationKeys inspect a normalized key string and decide whether it
     *  maps to one of the logical navigation actions -- Up, Down, Left, Right,
     *  Next, Prev, PageUp, PageDown, Home, End or Mode. The overloaded,
     *  argument-free variants return the canonical key string that the
     *  corresponding action is bound to (used for building help/legend text).
     */
    class INavigationKeys {
        public:
            virtual ~INavigationKeys() { }

            /** @brief Checks whether a key maps to the "cursor up" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "cursor up".
             */
            virtual bool Up(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "cursor down" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "cursor down".
             */
            virtual bool Down(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "cursor left" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "cursor left".
             */
            virtual bool Left(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "cursor right" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "cursor right".
             */
            virtual bool Right(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "next item" action (e.g. Tab).
             *  @param key the normalized key string.
             *  @return true if the key represents "next item".
             */
            virtual bool Next(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "page up" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "page up".
             */
            virtual bool PageUp(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "page down" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "page down".
             */
            virtual bool PageDown(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "go to beginning" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "home".
             */
            virtual bool Home(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "go to end" action.
             *  @param key the normalized key string.
             *  @return true if the key represents "end".
             */
            virtual bool End(const std::string& key) = 0;

            /** @brief Checks whether a key maps to the "previous item" action (e.g. Shift+Tab).
             *  @param key the normalized key string.
             *  @return true if the key represents "previous item".
             */
            virtual bool Prev(const std::string& key) = 0;

            /** @brief Checks whether a key maps to a mode-toggle action (e.g. Tab cycling modes).
             *  @param key the normalized key string.
             *  @return true if the key represents a mode change.
             */
            virtual bool Mode(const std::string& key) = 0;

            /** @brief Returns the canonical key string bound to the "cursor up" action.
             *  @return the normalized key string, e.g. "KEY_UP".
             */
            virtual std::string Up() = 0;

            /** @brief Returns the canonical key string bound to the "cursor down" action.
             *  @return the normalized key string, e.g. "KEY_DOWN".
             */
            virtual std::string Down() = 0;

            /** @brief Returns the canonical key string bound to the "cursor left" action.
             *  @return the normalized key string, e.g. "KEY_LEFT".
             */
            virtual std::string Left() = 0;

            /** @brief Returns the canonical key string bound to the "cursor right" action.
             *  @return the normalized key string, e.g. "KEY_RIGHT".
             */
            virtual std::string Right() = 0;

            /** @brief Returns the canonical key string bound to the "next item" action.
             *  @return the normalized key string, e.g. "TAB".
             */
            virtual std::string Next() = 0;

            /** @brief Returns the canonical key string bound to the "page up" action.
             *  @return the normalized key string, e.g. "PAGEUP".
             */
            virtual std::string PageUp() = 0;

            /** @brief Returns the canonical key string bound to the "page down" action.
             *  @return the normalized key string, e.g. "PAGEDOWN".
             */
            virtual std::string PageDown() = 0;

            /** @brief Returns the canonical key string bound to the "go to beginning" action.
             *  @return the normalized key string, e.g. "HOME".
             */
            virtual std::string Home() = 0;

            /** @brief Returns the canonical key string bound to the "go to end" action.
             *  @return the normalized key string, e.g. "END".
             */
            virtual std::string End() = 0;

            /** @brief Returns the canonical key string bound to the "previous item" action.
             *  @return the normalized key string, e.g. "SHIFT+TAB".
             */
            virtual std::string Prev() = 0;

            /** @brief Returns the canonical key string bound to the mode-toggle action.
             *  @return the normalized key string.
             */
            virtual std::string Mode() = 0;
    };
}
