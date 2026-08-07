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

/** @file GaplessTransport.h
 *  @brief ITransport implementation for gapless (seamless) track transitions.
 *  @details Uses an active and a next Player. The next track is preloaded and
 *      starts exactly when the active track finishes, avoiding a gap in audio. */

#include <musikcore/config.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/Player.h>
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/constants.h>

#include <thread>
#include <mutex>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief ITransport that switches tracks without an audible gap.
     *  @details Preloads the next track into a second player. When the active
     *      player reaches EOF the next player takes over seamlessly. If no next
     *      track was prepared, playback stops normally. */
    class GaplessTransport :
        public ITransport,
        private Player::EventListener
    {
        public:
            /** @brief Creates an empty gapless transport. */
            GaplessTransport();
            virtual ~GaplessTransport();

            /** @brief Starts playback of the given URI.
             *  @param uri The URI to play.
             *  @param gain Gain adjustment to apply.
             *  @param mode Immediate to play now, Wait to preload. */
            void Start(const std::string& uri, Gain gain, StartMode mode) override;
            /** @brief Preloads the next track for gapless playback.
             *  @param uri The URI to prepare.
             *  @param gain Gain adjustment to apply. */
            void PrepareNextTrack(const std::string& uri, Gain gain) override;

            /** @return The URI of the currently playing track. */
            std::string Uri() override;

            /** @brief Stops playback, suppressing the stop event.
             *  @details Used internally and by the transport switch logic. */
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
            double Volume() noexcept override;
            /** @brief Sets the volume.
             *  @param volume The new volume, in the range [0.0, 1.0]. */
            void SetVolume(double volume) override;

            /** @return true if output is muted. */
            bool IsMuted() noexcept override;
            /** @brief Sets or clears the muted state.
             *  @param muted true to mute, false to unmute. */
            void SetMuted(bool muted) override;

            /** @return The duration of the current track, in seconds. */
            double GetDuration() override;

            /** @brief Re-initializes the output device. */
            void ReloadOutput() override;

            /** @return The current playback state. */
            musik::core::sdk::PlaybackState GetPlaybackState() override;
            /** @return The current stream state. */
            musik::core::sdk::StreamState GetStreamState() override;

        private:
            using LockT = std::unique_lock<std::recursive_mutex>; /**< State lock alias. */

            /** @brief Starts playback using a pre-created player.
             *  @param player The player to start.
             *  @param mode StartMode::Immediate or Wait. */
            void StartWithPlayer(Player* player, StartMode mode = StartMode::Immediate);

            /** @brief Internal stop routine.
             *  @param suppressStopEvent Whether to suppress the stop playback event.
             *  @param stopOutput Whether the output device should be stopped too.
             *  @param exclude Optional player to leave running. */
            void StopInternal(
                bool suppressStopEvent,
                bool stopOutput,
                Player const* exclude = nullptr);

            /** @brief Enables or disables the handoff to the prepared next player.
             *  @param nextCanStart Whether the next player may start at EOF. */
            void SetNextCanStart(bool nextCanStart);

            /** @brief Raises a stream state event.
             *  @param type The stream state to emit.
             *  @param player The player associated with the event. */
            void RaiseStreamEvent(musik::core::sdk::StreamState type, Player const* player);
            /** @brief Sets and broadcasts the playback state.
             *  @param state The new playback state. */
            void SetPlaybackState(musik::core::sdk::PlaybackState state);

            /** @brief Player event handlers. @param player The player that emitted the event. */
            void OnPlayerStarted(Player* player) override;
            void OnPlayerBuffered(Player* player) override;
            void OnPlayerStreamEof(Player* player) override;
            void OnPlayerFinished(Player* player) override;
            void OnPlayerOpenFailed(Player* player) override;
            void OnPlayerDestroying(Player* player) override;

            /** @brief Stops and clears the active player slot. */
            void ResetActivePlayer();
            /** @brief Stops and clears the prepared next player slot. */
            void ResetNextPlayer();

            musik::core::sdk::PlaybackState playbackState; /**< Current playback state. */
            musik::core::sdk::StreamState activePlayerState; /**< Stream state of the active player. */
            std::recursive_mutex stateMutex; /**< Guards playback/stream state. */
            std::shared_ptr<musik::core::sdk::IOutput> output; /**< Shared output device. */
            Player* activePlayer; /**< Currently playing player, or nullptr. */
            Player* nextPlayer;   /**< Preloaded next player, or nullptr. */
            double volume;        /**< Current transport volume. */
            bool nextCanStart;    /**< Whether the next player may start at EOF. */
            bool muted;           /**< Muted flag. */
    };

} } }
