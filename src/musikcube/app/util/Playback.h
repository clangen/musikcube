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
 * @file Playback.h
 * @brief Playback convenience helpers for track list views.
 * @details Provides free functions that start playback from the current
 *          selection or from the top of a track list, and that hot swap the
 *          queue so it plays in place starting from the selection.
 */

#include <stdafx.h>

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/library/ILibrary.h>

#include <app/window/TrackListView.h>

namespace musik {
    namespace cube {
        /**
         * @brief Helpers that start playback from a track list.
         */
        namespace playback {
            /**
             * @brief Plays the selected track in the given list.
             * @param trackList the track list view
             * @param playback the active playback service
             */
            void PlaySelected(
                musik::cube::TrackListView& trackList,
                musik::core::audio::PlaybackService& playback);

            /**
             * @brief Plays all tracks in the list starting from the top.
             * @param trackList the track list view
             * @param playback the active playback service
             */
            void PlayFromTop(
                musik::cube::TrackListView& trackList,
                musik::core::audio::PlaybackService& playback);

            /**
             * @brief Hot swaps the queue to play from the selection in place.
             * @param trackList the track list view
             * @param playback the active playback service
             * @return true if the queue was swapped
             */
            bool HotSwap(
                musik::cube::TrackListView& trackList,
                musik::core::audio::PlaybackService& playback);
        }
    }
}
