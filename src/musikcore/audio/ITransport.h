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

/** @file ITransport.h
 *  @brief Abstract interface for audio playback transports.
 *  @details A transport controls one or more Player instances and exposes
 *      transport-level state (volume, position, playback/stream state) plus
 *      sigslot events for state changes. Implementations include the gapless and
 *      crossfading transports. */

#include <musikcore/config.h>
#include <musikcore/sdk/constants.h>
#include <musikcore/audio/Player.h>
#include <sigslot/sigslot.h>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief Interface for an audio playback transport.
     *  @details Owns the underlying player(s), applies gain/volume, exposes
     *      playback control and emits events when state changes. */
    class ITransport {
        public:
            using Gain = Player::Gain; /**< Volume/gain adjustment applied to the player. */

            /** @brief How a track should be started relative to the active track. */
            enum class StartMode : int {
                Immediate = 0, /**< Start playing right away. */
                Wait = 1       /**< Preload and wait for the current track to finish. */
            };

            /** @brief Emitted when the underlying stream state changes (opened, buffered, eof, ...). */
            sigslot::signal2<musik::core::sdk::StreamState, std::string> StreamEvent;
            /** @brief Emitted when the playback state changes (playing, paused, stopped, ...). */
            sigslot::signal1<musik::core::sdk::PlaybackState> PlaybackEvent;
            /** @brief Emitted when the volume is changed. */
            sigslot::signal0<> VolumeChanged;
            /** @brief Emitted periodically with the current playback position, in seconds. */
            sigslot::signal1<double> TimeChanged;

            virtual ~ITransport() { }

            /** @brief Starts playback of the given URI.
             *  @param uri The URI to play.
             *  @param gain Gain adjustment to apply.
             *  @param mode StartMode::Immediate to play now, StartMode::Wait to preload. */
            virtual void Start(const std::string& uri, Gain gain, StartMode mode) = 0;
            /** @brief Preloads the next track for gapless playback.
             *  @param uri The URI to prepare.
             *  @param gain Gain adjustment to apply. */
            virtual void PrepareNextTrack(const std::string& uri, Gain gain) = 0;

            /** @return The URI of the currently playing track. */
            virtual std::string Uri() = 0;

            /** @brief Stops playback. */
            virtual void Stop() = 0;
            /** @brief Pauses playback.
             *  @return true if the state changed to paused. */
            virtual bool Pause() = 0;
            /** @brief Resumes playback.
             *  @return true if the state changed to playing. */
            virtual bool Resume() = 0;

            /** @return The current playback position, in seconds. */
            virtual double Position() = 0;
            /** @brief Seeks to the given position.
             *  @param seconds The target position, in seconds. */
            virtual void SetPosition(double seconds) = 0;

            /** @return The current volume, in the range [0.0, 1.0]. */
            virtual double Volume() = 0;
            /** @brief Sets the volume.
             *  @param volume The new volume, in the range [0.0, 1.0]. */
            virtual void SetVolume(double volume) = 0;

            /** @return The duration of the current track, in seconds. */
            virtual double GetDuration() = 0;

            /** @return true if output is muted. */
            virtual bool IsMuted() = 0;
            /** @brief Sets or clears the muted state.
             *  @param muted true to mute, false to unmute. */
            virtual void SetMuted(bool muted) = 0;

            /** @brief Re-initializes the output device (used after device changes). */
            virtual void ReloadOutput() = 0;

            /** @return The current playback state. */
            virtual musik::core::sdk::PlaybackState GetPlaybackState() = 0;
            /** @return The current stream state. */
            virtual musik::core::sdk::StreamState GetStreamState() = 0;
    };

} } }
