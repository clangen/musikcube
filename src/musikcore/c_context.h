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

/** @file c_context.h
 *  @brief Internal implementation details for the musikcore_c C SDK contexts.
 *  @details This is an implementation header (not part of the public C API). It
 *      defines the concrete C++ types behind the opaque handles: the message
 *      queue, the application context, the indexer context and the player
 *      context. */

#include <musikcore/musikcore_c.h>
#include <musikcore/runtime/MessageQueue.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/LocalMetadataProxy.h>
#include <musikcore/library/IIndexer.h>

#include <string>
#include <thread>
#include <set>

using namespace musik;
using namespace musik::core;
using namespace musik::core::library::query;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::core::runtime;

/** @brief The message queue driving an mcsdk context.
 *  @details A MessageQueue that can be run and stopped from a dedicated thread,
 *      used to dispatch events for the context's services. */
class mcsdk_context_message_queue: public MessageQueue {
    public:
        /** @brief Creates a context message queue. */
        mcsdk_context_message_queue();
        virtual ~mcsdk_context_message_queue();
        /** @brief Signals the queue to stop dispatching and the run loop to exit. */
        void Quit();
        /** @brief Runs the dispatch loop until Quit() is called. */
        void Run();
    private:
        using LockT = std::unique_lock<std::mutex>;
        bool quit; /**< Signals the run loop to exit. */
        std::mutex mutex; /**< Guards the quit flag. */
};

/** @brief State shared by the services in one mcsdk context. */
struct mcsdk_context_internal {
    ILibraryPtr library; /**< The primary library. */
    LocalMetadataProxy* metadata; /**< Metadata proxy service. */
    PlaybackService* playback; /**< Playback service. */
    std::shared_ptr<Preferences> preferences; /**< Preferences store. */
};

/** @brief Forward declaration of the indexer callback proxy. */
struct mcsdk_svc_indexer_callback_proxy;

/** @brief State behind the indexer service handle. */
struct mcsdk_svc_indexer_context_internal {
    IIndexer* indexer; /**< Underlying indexer. */
    mcsdk_svc_indexer_callback_proxy* callback_proxy; /**< Translates C callbacks. */
    std::set<mcsdk_svc_indexer_callbacks*> callbacks; /**< Registered C callbacks. */
};

/** @brief State behind the audio player service handle. */
struct mcsdk_player_context_internal {
    Player::EventListener* event_listener; /**< Translates player events to C callbacks. */
    std::shared_ptr<IOutput> output; /**< Output device the player plays through. */
    std::mutex event_mutex; /**< Guards event delivery. */
    std::condition_variable finished_condition; /**< Signals playback completion. */
    Player* player; /**< Underlying player. */
    bool player_finished; /**< Whether the player has finished. */
};