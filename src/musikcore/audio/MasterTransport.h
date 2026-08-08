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

/** @file MasterTransport.h
 *  @brief ITransport facade that delegates to the active underlying transport.
 *  @details Wraps a concrete ITransport (e.g. GaplessTransport or
 *      CrossfadeTransport) and forwards all calls to it. The active transport can
 *      be switched at runtime; the chosen type is persisted in preferences. */

#include <musikcore/sdk/constants.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/support/Preferences.h>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief Dispatcher transport that forwards to a selected concrete transport.
     *  @details Relays ITransport calls to the currently selected underlying
     *      transport and mirrors its signals. SwitchTo() replaces the underlying
     *      transport and persists the selection. */
    class MasterTransport :
        public sigslot::has_slots<>,
        public musik::core::audio::ITransport
    {
        public:
            using Type = musik::core::sdk::TransportType; /**< Underlying transport type. */

            /** @brief Creates a master transport and loads the persisted type. */
            MasterTransport() noexcept;

            /** @brief Starts playback of the given URI.
             *  @param uri The URI to play.
             *  @param gain Gain adjustment to apply.
             *  @param mode StartMode::Immediate or Wait. */
            void Start(const std::string& uri, Gain gain, StartMode mode) override;
            /** @brief Preloads the next track.
             *  @param uri The URI to prepare.
             *  @param gain Gain adjustment to apply. */
            void PrepareNextTrack(const std::string& uri, Gain gain) override;

            /** @return The URI of the currently playing track. */
            std::string Uri() override;

            /** @brief Stops playback. */
            void Stop() override;
            /** @brief Pauses playback.
             *  @return true if the state changed to paused. */
            bool Pause() override;
            /** @brief Resumes playback.
             *  @return true if the state changed to playing. */
            bool Resume() override;

            /** @return The current playback position, in seconds. */
            double Position() override;
            /** @brief Seeks the active player.
             *  @param seconds The target position, in seconds. */
            void SetPosition(double seconds) override;

            /** @return The current volume, in the range [0.0, 1.0]. */
            double Volume() override;
            /** @brief Sets the volume.
             *  @param volume The new volume, in the range [0.0, 1.0]. */
            void SetVolume(double volume) override;

            /** @return The duration of the current track, in seconds. */
            double GetDuration() override;

            /** @return true if output is muted. */
            bool IsMuted() override;
            /** @brief Sets or clears the muted state.
             *  @param muted true to mute, false to unmute. */
            void SetMuted(bool muted) override;

            /** @brief Re-initializes the output device. */
            void ReloadOutput() override;

            /** @return The current playback state. */
            musik::core::sdk::PlaybackState GetPlaybackState() override;
            /** @return The current stream state. */
            musik::core::sdk::StreamState GetStreamState() override;

            /** @brief Switches to a different underlying transport type.
             *  @param type The transport type to switch to. */
            void SwitchTo(Type type);
            /** @return The type of the currently selected transport. */
            Type GetType() noexcept;

        private:
            /** @brief Forwards stream events from the underlying transport.
             *  @param type The stream state.
             *  @param url The associated URI. */
            void OnStreamEvent(musik::core::sdk::StreamState type, std::string url);
            /** @brief Forwards playback events from the underlying transport.
             *  @param type The playback state. */
            void OnPlaybackEvent(musik::core::sdk::PlaybackState type);
            /** @brief Forwards volume-change events from the underlying transport. */
            void OnVolumeChanged();
            /** @brief Forwards time-change events from the underlying transport.
             *  @param time The new position, in seconds. */
            void OnTimeChanged(double time);

            std::shared_ptr<musik::core::audio::ITransport> transport; /**< Currently selected transport. */
            std::shared_ptr<musik::core::Preferences> prefs; /**< Preferences for transport type. */
            Type type; /**< Currently selected transport type. */
    };

} } }
