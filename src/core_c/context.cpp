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

#include "musikcore_c.h"

#include <thread>

#include <core/debug.h>
#include <core/runtime/MessageQueue.h>
#include <core/runtime/Message.h>
#include <core/library/LibraryFactory.h>
#include <core/audio/PlaybackService.h>
#include <core/plugin/Plugins.h>
#include <core/library/LocalMetadataProxy.h>
#include <core/support/PreferenceKeys.h>

#include <boost/locale.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
#include <boost/filesystem.hpp>

using namespace musik;
using namespace musik::core;
using namespace musik::core::db::local;
using namespace musik::core::audio;
using namespace musik::core::sdk;
using namespace musik::core::runtime;

/*
 *
 * internal_message_queue
 *
 */

class internal_message_queue: public MessageQueue {
    public:
        internal_message_queue(): MessageQueue() {
            this->quit = false;
        }

        void Quit() {
            {
                LockT lock(this->mutex);
                this->quit = true;
            }
            this->Post(Message::Create(0, 0, 0, 0));
        }

        void Run() {
            while (true) {
                this->WaitAndDispatch();
                {
                    LockT lock(this->mutex);
                    if (this->quit) {
                        return;
                    }
                }
            }
        }

    private:
        using LockT = std::unique_lock<std::mutex>;
        bool quit;
        std::mutex mutex;
};

/*
 *
 * instance context
 *
 */

struct mcsdk_context_internal {
    internal_message_queue message_queue;
    std::thread thread;
    ILibraryPtr library;
    LocalMetadataProxy* metadata;
    PlaybackService* playback;
    std::shared_ptr<Preferences> preferences;
};

static std::mutex global_mutex;
static bool environment_initialized = false;
static mcsdk_context* plugin_context = nullptr;

mcsdk_export void mcsdk_context_init(mcsdk_context** context) {
    std::unique_lock<std::mutex> lock(global_mutex);

    if (!environment_initialized) {
        std::locale locale = std::locale();
        std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
        boost::filesystem::path::imbue(utf8Locale);
        debug::Start();
        environment_initialized = true;
    }
    auto c = new mcsdk_context();
    memset(c, 0, sizeof(mcsdk_context));
    auto internal = new mcsdk_context_internal();
    internal->library = LibraryFactory::Default();
    internal->library->SetMessageQueue(internal->message_queue);
    internal->playback = new PlaybackService(internal->message_queue, internal->library);
    internal->metadata = new LocalMetadataProxy(internal->library);
    internal->preferences = Preferences::ForComponent(prefs::components::Settings);
    c->internal.opaque = internal;
    c->metadata.opaque = internal->metadata;
    c->preferences.opaque = internal->preferences.get();
    c->playback.opaque = internal->playback;
    internal->thread = std::thread([internal] {
        internal->message_queue.Run();
    });
    if (!plugin_context) {
        mcsdk_set_plugin_context(c);
    }
    *context = c;
}

mcsdk_export void mcsdk_context_release(mcsdk_context** context) {
    std::unique_lock<std::mutex> lock(global_mutex);

    auto c = *context;
    auto internal = static_cast<mcsdk_context_internal*>(c->internal.opaque);
    delete internal->playback;
    internal->playback = nullptr;
    internal->library->Indexer()->Stop();
    internal->library.reset();
    internal->preferences.reset();
    delete internal->metadata;
    internal->message_queue.Quit();
    internal->thread.join();
    delete internal;
    delete c;
    if (plugin_context == c) {
        mcsdk_set_plugin_context(nullptr);
    }
    *context = 0;
}

mcsdk_export void mcsdk_set_plugin_context(mcsdk_context* context) {
    if (plugin_context && plugin_context != context) {
        plugin::Deinit();
    }
    plugin_context = context;
    if (plugin_context) {
        auto internal = static_cast<mcsdk_context_internal*>(context->internal.opaque);
        plugin::Init(&internal->message_queue, internal->playback, internal->library);
    }
}

mcsdk_export bool mcsdk_is_plugin_context(mcsdk_context* context) {
    return context && context == plugin_context;
}