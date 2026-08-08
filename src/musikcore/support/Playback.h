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

/** @file Playback.h
 *  @brief Playback convenience helpers.
 *  @details Provides common playback operations (volume, seek, pause) against a
 *      transport or playback service, plus helpers to persist and restore the
 *      playback queue. */

#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/PlaybackService.h>

/** @namespace musik::core::playback
 *  @brief High-level playback convenience functions. */
namespace musik {
    namespace core {
        namespace playback {
            /** @brief Increases the transport volume by one step.
             *  @param transport The transport to control. */
            void VolumeUp(musik::core::audio::ITransport& transport);
            /** @brief Decreases the transport volume by one step.
             *  @param transport The transport to control. */
            void VolumeDown(musik::core::audio::ITransport& transport);
            /** @brief Seeks forward by a fixed amount.
             *  @param playback The playback service to control. */
            void SeekForward(musik::core::sdk::IPlaybackService& playback);
            /** @brief Seeks backward by a fixed amount.
             *  @param playback The playback service to control. */
            void SeekBack(musik::core::sdk::IPlaybackService& playback);
            /** @brief Seeks forward proportionally to the track length.
             *  @param playback The playback service to control. */
            void SeekForwardProportional(musik::core::sdk::IPlaybackService& playback);
            /** @brief Seeks backward proportionally to the track length.
             *  @param playback The playback service to control. */
            void SeekBackProportional(musik::core::sdk::IPlaybackService& playback);
            /** @brief Toggles between playing and paused.
             *  @param transport The transport to control. */
            void PauseOrResume(musik::core::audio::ITransport& transport);

            /** @brief Restores the saved playback queue.
             *  @param library The library to read from.
             *  @param playback The playback service to populate. */
            void LoadPlaybackContext(
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback);

            /** @brief Persists the current playback queue.
             *  @param library The library to write to.
             *  @param playback The playback service to save. */
            void SavePlaybackContext(
                musik::core::ILibraryPtr library,
                musik::core::audio::PlaybackService& playback);
        }
    }
}
