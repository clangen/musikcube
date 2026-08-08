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

/** @file IKeyHandler.h @brief Interface for objects that consume raw keyboard input. */
#pragma once

#include <string>

namespace cursespp {
    /** @brief Contract for objects that can receive and process keyboard input.
     *
     *  @details Objects implementing IKeyHandler receive a normalized string
     *  representation of each keystroke (e.g. "a", "ENTER", "ESC", "F5").
     *  The toolkit translates the platform-specific curses key codes into
     *  these strings before dispatching them, so handlers do not need to deal
     *  with raw int constants. This is the mechanism through which windows and
     *  layouts react to user input.
     */
    class IKeyHandler {
        public:
            virtual ~IKeyHandler() { }

            /** @brief Invoked whenever a key is pressed while this handler is active.
             *  @param key the normalized string representation of the key.
             *  @return true if the key was consumed, false to let the event bubble up.
             */
            virtual bool KeyPress(const std::string& key) = 0;
    };
}
