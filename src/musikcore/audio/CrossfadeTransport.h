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

/** @file CrossfadeTransport.h
 *  @brief ITransport implementation that crossfades between tracks.
 *  @details Uses two PlayerContext slots (active and next) plus a Crossfader to
 *      fade out the current track while fading in the next one, allowing overlap
 *      when both players are ready. */

#include <musikcore/config.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Crossfader.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/constants.h>

#include <thread>
#include <mutex>

#include <sigslot/sigslot.h>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief ITransport that crossfades from one track into the next.
     *  @details Maintains an active and a next player. When the next track is ready
     *      it is faded in while the active track fades out, producing a smooth
     *      transition. Falls back to gapless-style handoff when fading is disabled. */
    class CrossfadeTransport :
        public ITransport,
        private Player::EventListener,
        public sigslot::has_slots<>
    {
        public:
            /** @brief Creates an empty crossfade transport. */
            CrossfadeTransport();
            virtual ~CrossfadeTransport();

            /** @brief Stops playback immediately without draining buffers. */
            void StopImmediately();

            /** @brief Starts playback of the given URI.
             *  @param uri The URI to play.
             *  @param gain Gain adjustment to apply.
             *  @param mode Immediate to play now, Wait to preload. */
            void Start(const std::string& uri, Gain gain, StartMode mode) override;
            /** @brief Preloads the next track for the upcoming crossfade.
             *  @param uri The URI to prepare.
             *  @param gain Gain adjustment to apply. */
            void PrepareNextTrack(const std::string& uri, Gain gain) override;

            /** @return The URI of the currently playing track. */
            std::string Uri() override;

            /** @brief Stops playback (gracefully draining the output). */
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
            using Lock = std::unique_lock<std::recursive_mutex>; /**< State lock alias. */
            using Output = std::shared_ptr<musik::core::sdk::IOutput>; /**< Output device alias. */
            using Thread = std::shared_ptr<std::thread>; /**< Thread alias. */
            using IMessage = musik::core::runtime::IMessage; /**< Message alias. */
            using IMessageTarget = musik::core::runtime::IMessageTarget; /**< Target alias. */
            using MessageQueue = musik::core::runtime::MessageQueue; /**< Queue alias. */

            /** @brief State and controls for one of the two player slots. */
            struct PlayerContext {
                PlayerContext(
                    CrossfadeTransport& transport,
                    Crossfader& crossfader) noexcept;

                /** @brief Resets the context to its initial (empty) state. */
                void Reset();

                /** @brief Resets and configures the context for a new URI.
                 *  @param url The URI to play.
                 *  @param listener Player event listener to attach.
                 *  @param gain Gain adjustment to apply.
                 *  @param startImmediate Whether playback should start immediately. */
                void Reset(
                    const std::string& url,
                    Player::EventListener* listener,
                    Gain gain,
                    bool startImmediate);

                /** @brief Hands the active player/output to another context.
                 *  @param context The destination context. */
                void TransferTo(PlayerContext& context) noexcept;

                /** @brief Starts playback at the given transport volume.
                 *  @param transportVolume The current transport volume. */
                void Start(double transportVolume);
                /** @brief Stops playback. */
                void Stop();
                /** @brief Stops playback if the given player is in use.
                 *  @param player Player to compare against. */
                void StopIf(Player const* player);
                /** @brief Pauses playback. */
                void Pause();
                /** @brief Resumes playback.
                 *  @param transportVolume The current transport volume. */
                void Resume(double transportVolume);
                /** @brief Sets the player volume.
                 *  @param volume The new volume. */
                void SetVolume(double volume);
                /** @return true if the slot holds no player. */
                bool IsEmpty() noexcept;

                bool startImmediate; /**< Whether playback started immediately on load. */
                bool started;        /**< Whether playback has started. */
                bool canFade;        /**< Whether this track participates in crossfading. */
                Output output;       /**< Output device for the player. */
                Player *player;      /**< The Player instance, or nullptr. */
                CrossfadeTransport& transport; /**< Owning transport. */
                Crossfader& crossfader;        /**< Crossfader used for transitions. */
            };

            /** @brief Raises a stream state event.
             *  @param type The stream state to emit.
             *  @param player The player associated with the event. */
            void RaiseStreamEvent(musik::core::sdk::StreamState type, Player const* player);
            /** @brief Sets and broadcasts the playback state.
             *  @param state The new playback state. */
            void SetPlaybackState(musik::core::sdk::PlaybackState state);

            /** @brief Called when the crossfader has finished all fades. */
            void OnCrossfaderEmptied();

            /** @brief Player event handlers. @param player The player that emitted the event. */
            void OnPlayerBuffered(Player* player) override;
            void OnPlayerStarted(Player* player) override;
            void OnPlayerFinished(Player* player) override;
            void OnPlayerOpenFailed(Player* player) override;
            /** @brief Player mix-point reached.
             *  @param player The player.
             *  @param id User-defined mix point id.
             *  @param time Position of the mix point, in seconds. */
            void OnPlayerMixPoint(Player* player, int id, double time) override;
            void OnPlayerDestroying(Player* player) override;

            musik::core::sdk::PlaybackState playbackState; /**< Current playback state. */
            musik::core::sdk::StreamState activePlayerState; /**< Stream state of the active player. */
            std::recursive_mutex stateMutex; /**< Guards playback/stream state. */
            Crossfader crossfader;           /**< Performs volume fades. */
            PlayerContext active;            /**< Currently playing track slot. */
            PlayerContext next;              /**< Preloaded upcoming track slot. */
            double volume;                   /**< Current transport volume. */
            bool muted;                      /**< Muted flag. */
    };

} } }
