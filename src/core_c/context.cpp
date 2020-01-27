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

#include <ev++.h>
#include <thread>

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
 * ev_message_queue
 *
 */

static const short EVENT_DISPATCH = 1;
static const short EVENT_QUIT = 2;

class ev_message_queue: public MessageQueue {
    public:
        ev_message_queue(): MessageQueue() {
            if (pipe(pipeFd) != 0) {
                std::cerr << "\n  ERROR! couldn't create pipe\n\n";
                exit(EXIT_FAILURE);
            }
        }

        virtual ~ev_message_queue() {
            if (pipeFd[0]) {
                close(pipeFd[0]);
            }
            if (pipeFd[1]) {
                close(pipeFd[1]);
            }
        }

        void Post(IMessagePtr message, int64_t delayMs) {
            MessageQueue::Post(message, delayMs);

            if (delayMs <= 0) {
                write(pipeFd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));
            }
            else {
                double delayTs = (double) delayMs / 1000.0;
                loop.once<
                    ev_message_queue,
                    &ev_message_queue::DelayedDispatch
                >(-1, ev::TIMER, (ev::tstamp) delayTs, this);
            }
        }

        void DelayedDispatch(int revents) {
            this->Dispatch();
        }

        void SignalQuit() {
            write(pipeFd[1], &EVENT_QUIT, sizeof(EVENT_QUIT));
        }

        void ReadCallback(ev::io& watcher, int revents) {
            short type;
            if (read(pipeFd[0], &type, sizeof(type)) == 0) {
                std::cerr << "read() failed.\n";
                exit(EXIT_FAILURE);
            }
            switch (type) {
                case EVENT_DISPATCH: this->Dispatch(); break;
                case EVENT_QUIT: loop.break_loop(ev::ALL); break;
            }
        }

        void Run() {
            io.set(loop);
            io.set(pipeFd[0], ev::READ);
            io.set<ev_message_queue, &ev_message_queue::ReadCallback>(this);
            io.start();

            sio.set(loop);

            write(pipeFd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));

            loop.run(0);
        }

    private:
        int pipeFd[2];
        ev::dynamic_loop loop;
        ev::io io;
        ev::sig sio;
};

/*
 *
 * instance context
 *
 */

struct mcsdk_context_internal {
    ev_message_queue message_queue;
    std::thread thread;
    ILibraryPtr library;
    LocalMetadataProxy* metadata;
    PlaybackService* playback;
    std::shared_ptr<Preferences> preferences;
};

static mcsdk_context* plugin_context = nullptr;

mcsdk_export void mcsdk_context_init(mcsdk_context** context) {
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);
    auto c = new mcsdk_context();
    memset(c, 0, sizeof(mcsdk_context));
    auto internal = new mcsdk_context_internal();
    internal->library = LibraryFactory::Default();
    internal->library->SetMessageQueue(internal->message_queue);
    internal->playback = new PlaybackService(internal->message_queue, internal->library);
    internal->metadata = new LocalMetadataProxy(internal->library);
    internal->preferences = Preferences::ForComponent(prefs::components::Settings);
    c->internal = internal;
    c->metadata = (mcsdk_svc_metadata) internal->metadata;
    c->preferences = (mcsdk_preferences) internal->preferences.get();
    c->playback = (mcsdk_svc_playback) internal->playback;
    internal->thread = std::thread([internal] {
        internal->message_queue.Run();
    });
    if (!plugin_context) {
        mcsdk_set_plugin_context(c);
    }
    *context = c;
}

mcsdk_export void mcsdk_context_release(mcsdk_context** context) {
    auto c = *context;
    auto internal = static_cast<mcsdk_context_internal*>(c->internal);
    delete internal->playback;
    internal->playback = nullptr;
    internal->library->Indexer()->Stop();
    internal->library.reset();
    internal->preferences.reset();
    delete internal->metadata;
    internal->message_queue.SignalQuit();
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
        auto internal = static_cast<mcsdk_context_internal*>(context->internal);
        plugin::Init(&internal->message_queue, internal->playback, internal->library);
    }
}

mcsdk_export bool mcsdk_is_plugin_context(mcsdk_context* context) {
    return context && context == plugin_context;
}