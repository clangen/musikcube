//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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

#include <core/musikcore_c.h>
#include <core/runtime/MessageQueue.h>
#include <core/library/LibraryFactory.h>
#include <core/audio/PlaybackService.h>
#include <core/library/LocalMetadataProxy.h>
#include <core/library/IIndexer.h>

#include <string>
#include <thread>
#include <set>

using namespace musik;
using namespace musik::core;
using namespace musik::core::db::local;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::core::runtime;

class mcsdk_context_message_queue: public MessageQueue {
    public:
        mcsdk_context_message_queue();
        virtual ~mcsdk_context_message_queue();
        void Quit();
        void Run();
    private:
        using LockT = std::unique_lock<std::mutex>;
        bool quit;
        std::mutex mutex;
};

struct mcsdk_context_internal {
    mcsdk_context_message_queue message_queue;
    std::thread thread;
    ILibraryPtr library;
    LocalMetadataProxy* metadata;
    PlaybackService* playback;
    std::shared_ptr<Preferences> preferences;
};

struct mcsdk_svc_indexer_callback_proxy;

struct mcsdk_svc_indexer_context_internal {
    IIndexer* indexer;
    mcsdk_svc_indexer_callback_proxy* callback_proxy;
    std::set<mcsdk_svc_indexer_callbacks*> callbacks;
};

struct mcsdk_player_context_internal {
    Player::EventListener* event_listener;
    std::shared_ptr<IOutput> output;
    std::mutex event_mutex;
    std::condition_variable finished_condition;
    Player* player;
    bool player_finished;
};