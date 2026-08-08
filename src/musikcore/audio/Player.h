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

/** @file Player.h
 *  @brief Asynchronous audio player that decodes, processes and outputs a stream.
 *  @details A Player owns a worker thread that pulls processed buffers from an
 *      IStream and writes them to an IOutput device. It supports seeking, gain,
 *      mix points and asynchronous destroy, and notifies an EventListener of
 *      state changes. */

#include <musikcore/config.h>
#include <musikcore/audio/IStream.h>
#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/IBufferProvider.h>

#include <sigslot/sigslot.h>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

/** @namespace musik::core::audio
 *  @brief Audio pipeline types: buffers, players, transports and streams. */
namespace musik { namespace core { namespace audio {

    struct FftContext; /**< Opaque FFT state used for spectrum analysis. */

    /** @brief Decodes and plays a single URI on a background thread.
     *  @details Creates an IStream for the URI and feeds its processed buffers to
     *      the given IOutput. Callbacks notify the EventListener as the player
     *      buffers, starts, finishes, or fails. Destroy() is asynchronous and may
     *      optionally drain the output before stopping. */
    class Player : public musik::core::sdk::IBufferProvider {
        public:
            /** @brief How the player should behave when destroyed. */
            enum class DestroyMode: int {
                Drain = 0,   /**< Wait for the output to finish playing queued audio. */
                NoDrain = 1  /**< Stop immediately, discarding queued audio. */
            };

            /** @brief Gain profile applied to a track.
             *  @details preamp is a fixed boost/cut, gain the per-track adjustment,
             *      and peak the measured peak level (with peakValid indicating
             *      whether peak data is available). */
            struct Gain {
                Gain() noexcept {
                    this->preamp = this->gain = this->peak = 1.0f;
                    this->peakValid = false;
                }
                float preamp;    /**< Pre-amp gain multiplier. */
                float gain;      /**< Track gain multiplier. */
                float peak;      /**< Measured peak level. */
                bool peakValid;  /**< true if peak contains a valid measurement. */
            };

            /** @brief Callback interface for player lifecycle events. */
            struct EventListener {
                virtual ~EventListener() { }
                /** @brief Called when the first audio buffer has been decoded.
                 *  @param player The player that emitted the event. */
                virtual void OnPlayerBuffered(Player* player) { }
                /** @brief Called when playback actually starts.
                 *  @param player The player that emitted the event. */
                virtual void OnPlayerStarted(Player* player) { }
                /** @brief Called when the stream reaches end-of-file.
                 *  @param player The player that emitted the event. */
                virtual void OnPlayerStreamEof(Player* player) { }
                /** @brief Called when playback has fully finished.
                 *  @param player The player that emitted the event. */
                virtual void OnPlayerFinished(Player* player) { }
                /** @brief Called when the stream could not be opened.
                 *  @param player The player that emitted the event. */
                virtual void OnPlayerOpenFailed(Player* player) { }
                /** @brief Called before the player is destroyed.
                 *  @param player The player about to be destroyed. */
                virtual void OnPlayerDestroying(Player* player) { }
                /** @brief Called when a mix point is reached.
                 *  @param player The player that emitted the event.
                 *  @param id User-defined mix point id.
                 *  @param time Position of the mix point, in seconds. */
                virtual void OnPlayerMixPoint(Player* player, int id, double time) { }
            };

            /** @brief Creates a player and starts decoding in the background.
             *  @param url The URI to play.
             *  @param output The output device to play through.
             *  @param destroyMode How to behave when destroyed.
             *  @param listener Callback for player events.
             *  @param gain Gain profile to apply.
             *  @return A new heap-allocated Player. Call Destroy() to release it. */
            static Player* Create(
                const std::string &url,
                std::shared_ptr<musik::core::sdk::IOutput> output,
                DestroyMode destroyMode,
                EventListener *listener,
                Gain gain = Gain());

            /** @brief Notifies the player a buffer has been consumed by the output.
             *  @param buffer The processed buffer that the player can now recycle. */
            virtual void OnBufferProcessed(musik::core::sdk::IBuffer *buffer);

            /** @brief Removes a listener from the callback list.
             *  @param listener The listener to detach. */
            void Detach(EventListener *listener);
            /** @brief Adds a listener to the callback list.
             *  @param listener The listener to attach. */
            void Attach(EventListener *listener);

            /** @brief Starts playback (no-op if already started). */
            void Play();
            /** @brief Destroys the player using its configured DestroyMode. */
            void Destroy();
            /** @brief Destroys the player with an explicit mode.
             *  @param mode Drain to finish queued audio, NoDrain to stop immediately. */
            void Destroy(DestroyMode mode);

            /** @return The current playback position, in seconds. */
            double GetPosition();
            /** @brief Seeks to the given position.
             *  @param seconds The target position, in seconds. */
            void SetPosition(double seconds);
            /** @return The duration of the stream, in seconds. */
            double GetDuration();

            /** @brief Registers a mix point to be reported when reached.
             *  @param id User-defined id reported with the event.
             *  @param time Position within the track, in seconds. */
            void AddMixPoint(int id, double time);

            /** @return true if the stream supports the given capability.
             *  @param capability The capability to query. */
            bool HasCapability(musik::core::sdk::Capability capability);

            /** @return The URI this player is playing. */
            std::string GetUrl() const { return this->url; }

            /** @return The current stream state. */
            musik::core::sdk::StreamState GetStreamState() noexcept { return this->streamState; }

        private:
            friend void playerThreadLoop(Player* player); /**< Worker thread entry point. */

            /** @brief Reads the current position without synchronization. */
            double GetPositionInternal();

            /** @brief Constructs a player (use Create() instead).
             *  @param url The URI to play.
             *  @param output The output device to play through.
             *  @param finishMode How to behave when destroyed.
             *  @param listener Callback for player events.
             *  @param gain Gain profile to apply. */
            Player(
                const std::string &url,
                std::shared_ptr<musik::core::sdk::IOutput> output,
                DestroyMode finishMode,
                EventListener *listener,
                Gain gain);

            virtual ~Player();

            /** @brief A single scheduled mix point. */
            struct MixPoint {
                MixPoint(int id, double time) noexcept {
                    this->id = id;
                    this->time = time;
                }

                int id;     /**< User-defined identifier. */
                double time; /**< Position in the track, in seconds. */
            };

            using MixPointPtr = std::shared_ptr<MixPoint>; /**< Shared mix point alias. */
            using MixPointList = std::list<MixPointPtr>;   /**< Mix point list alias. */
            using ListenerList = std::list<EventListener*>; /**< Listener list alias. */
            using OutputPtr = std::shared_ptr<musik::core::sdk::IOutput>; /**< Output alias. */

            /** @brief Internal worker-thread state machine. */
            typedef enum {
                Idle = 0,    /**< Not playing. */
                Playing = 1, /**< Actively decoding/outputting. */
                Quit = 2     /**< Exiting the worker loop. */
            } InternalState;

            /** @return true once the worker thread has finished. */
            bool Exited();
            /** @return The current internal state. */
            int State();
            /** @return A snapshot of the current listener list. */
            ListenerList Listeners();

            std::thread* thread; /**< Worker thread. */

            OutputPtr output;    /**< Output device. */
            IStreamPtr stream;   /**< Decoding stream. */
            ListenerList listeners; /**< Registered event listeners. */
            MixPointList pendingMixPoints;   /**< Mix points not yet reached. */
            MixPointList processedMixPoints; /**< Mix points already reported. */
            MixPointList mixPointsHitTemp;   /**< Scratch list to avoid re-allocations. */

            /** @brief Recomputes the next mix point time from pending points. */
            void UpdateNextMixPointTime();

            std::string url; /**< URI being played. */

            /* granular mutexes for better performance */
            std::mutex queueMutex, listenerMutex; /**< Guards queue and listener data. */
            std::condition_variable writeToOutputCondition; /**< Signals the worker thread. */

            double volume;                    /**< Current player volume. */
            double nextMixPoint;              /**< Position of the next mix point, in seconds. */
            std::atomic<double> currentPosition; /**< Current playback position, in seconds. */
            std::atomic<double> seekToPosition;  /**< Pending seek position, or negative if none. */
            std::atomic<musik::core::sdk::StreamState> streamState; /**< Current stream state. */
            std::atomic<int> internalState;   /**< Current InternalState. */
            bool notifiedStarted;             /**< Whether OnPlayerStarted has fired. */
            float* spectrum;                  /**< FFT spectrum data for visualization. */
            DestroyMode destroyMode;          /**< Configured destroy behaviour. */
            Gain gain;                        /**< Gain profile applied to playback. */
            int pendingBufferCount;           /**< Buffers handed to output not yet recycled. */
            bool threadFinished;              /**< Whether the worker thread has exited. */

            FftContext* fftContext; /**< Opaque FFT context for spectrum analysis. */
    };

} } }
