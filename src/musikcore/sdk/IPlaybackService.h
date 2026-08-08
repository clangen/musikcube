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

/** @file IPlaybackService.h @brief Defines the IPlaybackService interface for controlling and querying playback. */
#pragma once

#include "constants.h"
#include "ITrack.h"
#include "ITrackList.h"
#include "ITrackListEditor.h"

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief The playback engine's public interface, providing control over
     *  the play queue, transport, volume, and shuffle/repeat behavior. */
    class IPlaybackService {
        public:
            /* sdk v1 */
            /** @brief Starts playing the track at the given index.
             *  @param index The zero-based index of the track to play. */
            virtual void Play(size_t index) = 0;

            /** @brief Advances to the next track.
             *  @return True if there was a next track to play. */
            virtual bool Next() = 0;

            /** @brief Moves to the previous track.
             *  @return True if there was a previous track to play. */
            virtual bool Previous() = 0;

            /** @brief Stops playback. */
            virtual void Stop() = 0;

            /** @brief Returns the current repeat mode.
             *  @return The repeat mode. */
            virtual musik::core::sdk::RepeatMode GetRepeatMode() = 0;

            /** @brief Sets the repeat mode.
             *  @param mode The repeat mode to use. */
            virtual void SetRepeatMode(musik::core::sdk::RepeatMode mode) = 0;

            /** @brief Cycles to the next repeat mode. */
            virtual void ToggleRepeatMode() = 0;

            /** @brief Returns the current playback state.
             *  @return The playback state. */
            virtual musik::core::sdk::PlaybackState GetPlaybackState() = 0;

            /** @brief Returns whether the play queue is shuffled.
             *  @return True if shuffled. */
            virtual bool IsShuffled() = 0;

            /** @brief Toggles shuffle mode. */
            virtual void ToggleShuffle() = 0;

            /** @brief Pauses playback if playing, or resumes if paused. */
            virtual void PauseOrResume() = 0;

            /** @brief Returns the current volume.
             *  @return The volume, from 0.0 to 1.0. */
            virtual double GetVolume() = 0;

            /** @brief Sets the volume.
             *  @param volume The volume, from 0.0 to 1.0. */
            virtual void SetVolume(double volume) = 0;

            /** @brief Returns the current playback position.
             *  @return The position, in seconds. */
            virtual double GetPosition() = 0;

            /** @brief Seeks to an absolute position.
             *  @param seconds The target position, in seconds. */
            virtual void SetPosition(double seconds) = 0;

            /** @brief Returns the duration of the currently playing track.
             *  @return The duration, in seconds. */
            virtual double GetDuration() = 0;

            /** @brief Returns whether the output is muted.
             *  @return True if muted. */
            virtual bool IsMuted() = 0;

            /** @brief Toggles the mute state. */
            virtual void ToggleMute() = 0;

            /** @brief Returns the index of the currently playing track.
             *  @return The track index. */
            virtual size_t GetIndex() = 0;

            /** @brief Returns the number of tracks in the play queue.
             *  @return The track count. */
            virtual size_t Count() = 0;

            /** @brief Returns the track at the given index.
             *  @param index The zero-based index.
             *  @return The track, or null if out of range. */
            virtual ITrack* GetTrack(size_t index) = 0;

            /* sdk v2 */
            /** @brief Returns the currently playing track.
             *  @return The playing track. */
            virtual ITrack* GetPlayingTrack() = 0;

            /* sdk v3 */
            /** @brief Replaces the play queue with the contents of the given track list.
             *  @param trackList The track list to copy from. */
            virtual void CopyFrom(const ITrackList* trackList) = 0;

            /** @brief Starts playing a source track list at the given index.
             *  @param source The track list to play from.
             *  @param index The zero-based index of the track to play. */
            virtual void Play(const ITrackList* source, size_t index) = 0;

            /** @brief Returns an editor for modifying the play queue.
             *  @return The track list editor. */
            virtual ITrackListEditor* EditPlaylist() = 0;

            /* sdk v5 */
            /** @brief Returns the current time change mode.
             *  @return The time change mode. */
            virtual musik::core::sdk::TimeChangeMode GetTimeChangeMode() = 0;

            /** @brief Sets the time change mode.
             *  @param mode The time change mode to use. */
            virtual void SetTimeChangeMode(musik::core::sdk::TimeChangeMode) = 0;

            /* sdk v13 */
            /** @brief Reloads the active output plugin. */
            virtual void ReloadOutput() = 0;

            /** @brief Returns a clone of the current play queue.
             *  @return A new track list containing the queue's tracks. */
            virtual ITrackList* Clone() = 0;
    };

} } }

