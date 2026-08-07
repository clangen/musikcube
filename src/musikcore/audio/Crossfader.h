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

/** @file Crossfader.h
 *  @brief Background thread that performs volume fades between players.
 *  @details The Crossfader runs a dedicated thread that smoothly ramps the volume
 *      of one or more Player instances over a requested duration. CrossfadeTransport
 *      uses it to fade out the outgoing track while fading in the incoming one,
 *      and to suspend/resume fades when playback is interrupted. */

#include <musikcore/config.h>
#include <musikcore/audio/ITransport.h>
#include <musikcore/audio/Player.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/constants.h>

#include <thread>
#include <mutex>
#include <atomic>

#include <sigslot/sigslot.h>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    /** @brief Performs volume fade-in/fade-out on players on a background thread.
     *  @details Tracks a list of active fade contexts, each tied to a player and
     *      an output device. The fade thread ramps volume until the duration elapses
     *      or the fade is cancelled. Exposes an Emptied signal when all fades are
     *      complete, used by CrossfadeTransport to advance state. */
    class Crossfader:
        private musik::core::runtime::IMessageTarget,
        private Player::EventListener
    {
        public:
            /** @brief Direction of the fade. */
            enum Direction { FadeIn, FadeOut };

            /** @brief Emitted when the last active fade completes and the list is empty. */
            sigslot::signal0<> Emptied;

            /** @brief Creates a crossfader bound to the given transport.
             *  @param transport Transport used to read the current volume level. */
            Crossfader(ITransport& transport);
            virtual ~Crossfader();

            /** @brief Starts a fade on the given player.
             *  @param player Player to fade.
             *  @param output Output device feeding the player.
             *  @param direction FadeIn or FadeOut.
             *  @param durationMs Fade duration, in milliseconds. */
            void Fade(
                Player* player,
                std::shared_ptr<musik::core::sdk::IOutput> output,
                Direction direction,
                long durationMs);

            /** @brief Cancels an in-progress fade.
             *  @param player Player whose fade should be cancelled.
             *  @param direction Direction of the fade to cancel. */
            void Cancel(Player* player, Direction direction);
            /** @return true if the player currently has an active fade. */
            bool Contains(Player* player);
            /** @brief Pauses all active fades (volume ramping is suspended). */
            void Pause();
            /** @brief Resumes all paused fades. */
            void Resume();
            /** @brief Cancels all fades and returns volume control to the players. */
            void Stop();
            /** @brief Blocks until the fade thread exits. */
            void Drain();

        private:
            /** @brief Fade thread entry point. */
            void ThreadLoop();

            /** @brief Handles internal queue messages.
             *  @param message The incoming runtime message. */
            virtual void ProcessMessage(
                musik::core::runtime::IMessage &message);

            /** @brief Callback invoked when a player is being destroyed.
             *  @param player The player about to be destroyed. */
            virtual void OnPlayerDestroying(musik::core::audio::Player* player);

            /** @brief State for a single in-progress fade. */
            struct FadeContext {
                std::shared_ptr<musik::core::sdk::IOutput> output; /**< Output device being faded. */
                Player* player;   /**< Player being faded. */
                Direction direction; /**< Fade direction. */
                long ticksCounted;   /**< Elapsed ticks of the fade. */
                long ticksTotal;     /**< Total ticks in the fade. */
            };

            using FadeContextPtr = std::shared_ptr<FadeContext>; /**< Shared ownership alias. */

            std::recursive_mutex contextListLock;  /**< Guards the context list. */
            std::unique_ptr<std::thread> thread;   /**< Fade worker thread. */
            musik::core::runtime::MessageQueue messageQueue; /**< Internal message queue. */
            std::list<FadeContextPtr> contextList; /**< Active fade contexts. */
            std::atomic<bool> quit, paused;        /**< Thread lifecycle and pause flags. */
            std::condition_variable_any drainCondition; /**< Signals Drain() when the thread exits. */
            ITransport& transport;                 /**< Reference to the owning transport. */
    };

} } }
