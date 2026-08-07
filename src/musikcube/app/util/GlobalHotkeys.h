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
 * @file GlobalHotkeys.h
 * @brief Dispatcher for hotkeys handled at the application level.
 * @details Owns the global (application-wide) hotkey bindings that control
 *          navigation, playback and the play queue. Key sequences that are
 *          not consumed by the focused window fall through to this class.
 */

#include <stdafx.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/ILibrary.h>

namespace musik {
    namespace cube {
        /**
         * @brief Application-level hotkey handler.
         * @details Translates raw key sequences into actions such as volume
         *          changes, seek, navigation and queue management by comparing
         *          them against the configured hotkey bindings.
         */
        class GlobalHotkeys {
            public:
                DELETE_CLASS_DEFAULTS(GlobalHotkeys)

                /**
                 * @brief Creates the handler bound to the playback service and
                 *        library.
                 * @param playback the active playback service
                 * @param library the active library
                 */
                GlobalHotkeys(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library);

                /**
                 * @brief Destroys the handler.
                 * @note Non-virtual; do not use as a base class.
                 */
                ~GlobalHotkeys(); /* non-virtual; do not use as a base class */

                /**
                 * @brief Attempts to handle the given key sequence.
                 * @param kn the key sequence that was pressed
                 * @return true if the sequence was consumed by a hotkey
                 */
                bool Handle(const std::string& kn);

            private:
                musik::core::audio::PlaybackService& playback; /**< the active playback service */
                musik::core::audio::ITransport& transport;     /**< the transport driving playback */
                musik::core::ILibraryPtr library;              /**< the active library */
        };
    }
}
