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

/** @file IPlaybackRemote.h @brief Defines the IPlaybackRemote interface for observing playback state. */
#pragma once

#include "IPlaybackService.h"
#include "ITrack.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief A receiver of playback state change notifications, allowing
     *  plugins to observe and react to the current playback session. */
    class IPlaybackRemote {
        public:
            /** @brief Releases the remote; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Associates the remote with a playback service.
             *  @param playback The playback service to observe. */
            virtual void SetPlaybackService(IPlaybackService* playback) = 0;

            /** @brief Called when the currently playing track changes.
             *  @param track The newly playing track. */
            virtual void OnTrackChanged(ITrack* track) = 0;

            /** @brief Called when the playback state changes.
             *  @param state The new playback state. */
            virtual void OnPlaybackStateChanged(PlaybackState state) = 0;

            /** @brief Called when the playback position changes.
             *  @param time The new position, in seconds. */
            virtual void OnPlaybackTimeChanged(double time) = 0;

            /** @brief Called when the output volume changes.
             *  @param volume The new volume, from 0.0 to 1.0. */
            virtual void OnVolumeChanged(double volume) = 0;

            /** @brief Called when the repeat or shuffle mode changes.
             *  @param repeatMode The new repeat mode.
             *  @param shuffled True if the queue is shuffled. */
            virtual void OnModeChanged(RepeatMode repeatMode, bool shuffled) = 0;

            /** @brief Called when the play queue changes. */
            virtual void OnPlayQueueChanged() = 0;
    };

} } }

