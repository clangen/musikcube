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

#include <musikcore/plugin/playback-remote.hpp>
#include <musikcore/playback.h>
#include <atomic>
#include "SPMediaKeyTap.h"

struct IKeyProcessor {
    virtual void ProcessKeyCode(int keyCode) = 0;
};

@interface MediaKeys: NSObject {
    SPMediaKeyTap* keyTap;
    IKeyProcessor* processor;
}
- (BOOL) register;
- (void) requestStop;
- (void) unregister;
- (void) setKeyProcessor: (IKeyProcessor*) processor;
@end

namespace {
    void MC_CALL commandComplete(void*, mc_result) {}

    /* playback handles are delivered on the owner thread and belong to this callback. */
    void MC_CALL playOrPause(void*, mc_result result, mc_playback* playback) {
        if (result == MC_OK && playback) {
            auto snapshot = mc_playback_snapshot MC_PLAYBACK_SNAPSHOT_DEFAULT;
            if (mc_playback_get_snapshot(playback, &snapshot) == MC_OK) {
                if (snapshot.state == MC_PLAYBACK_STOPPED) {
                    mc_play_queue* queue = nullptr;
                    mc_track_result* tracks = nullptr;
                    if (mc_playback_get_queue(playback, &queue) == MC_OK && queue &&
                        mc_play_queue_get_tracks(queue, &tracks) == MC_OK && tracks)
                    {
                        (void) mc_playback_play_result(playback, tracks, 0);
                    }
                    mc_track_result_destroy(&tracks);
                    mc_play_queue_destroy(&queue);
                }
                else {
                    (void) mc_playback_control(playback, MC_PLAYBACK_TOGGLE_PAUSE, 0);
                }
            }
        }
        if (playback) {
            mc_playback_destroy(&playback);
        }
    }

    class PlaybackRemote : public IKeyProcessor {
    public:
        explicit PlaybackRemote(const mc_plugin_services& services)
        : host(*services.playback) {}

        void Changed(const mc_plugin_playback_event&) noexcept {}
        void QueueChanged() noexcept {}

        mc_result Start() {
            this->stopping.store(false);
            this->mediaKeys = [[MediaKeys alloc] init];
            [this->mediaKeys setKeyProcessor:this];
            return [this->mediaKeys register] ? MC_OK : MC_ERROR_IO;
        }

        void RequestStop() noexcept {
            this->stopping.store(true);
            if (this->mediaKeys) {
                [this->mediaKeys requestStop];
            }
        }

        mc_result Join() {
            if (this->mediaKeys) {
                /* callbacks may still be running until the tap thread exits. */
                [this->mediaKeys unregister];
                [this->mediaKeys release];
                this->mediaKeys = nil;
            }
            return MC_OK;
        }

        void ProcessKeyCode(int keyCode) override {
            if (this->stopping.load()) {
                return;
            }
            switch (keyCode) {
                case NX_KEYTYPE_PLAY:
                    (void) this->host.with_playback_async(this->host.context, playOrPause, nullptr);
                    break;
                case NX_KEYTYPE_FAST:
                case NX_KEYTYPE_NEXT:
                    (void) this->host.control_async(this->host.context, MC_PLAYBACK_NEXT, 0, commandComplete, nullptr);
                    break;
                case NX_KEYTYPE_REWIND:
                case NX_KEYTYPE_PREVIOUS:
                    (void) this->host.control_async(this->host.context, MC_PLAYBACK_PREVIOUS, 0, commandComplete, nullptr);
                    break;
            }
        }

    private:
        mc_plugin_playback_service host;
        MediaKeys* mediaKeys { nil };
        std::atomic<bool> stopping { true };
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        return musik::core::plugin::RegisterPlaybackRemote<PlaybackRemote>(*host);
    }

    void MC_CALL shutdown(void*) {}
}

@implementation MediaKeys
- (id) init {
    keyTap = [[SPMediaKeyTap alloc] initWithDelegate:self];
    processor = nil;

    // Register defaults for the whitelist of apps that want to use media keys
    [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
        [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
    nil]];

    return self;
}

- (void) setKeyProcessor: (IKeyProcessor*) keyProcessor {
    processor = keyProcessor;
}

- (void) mediaKeyTap:(SPMediaKeyTap*) keyTap receivedMediaKeyEvent: (NSEvent*) event {
    assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

    int keyCode = (([event data1] & 0xFFFF0000) >> 16);
    int keyFlags = ([event data1] & 0x0000FFFF);
    int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;

    if (keyState == 1 && processor) {
        processor->ProcessKeyCode(keyCode);
    }
}

- (BOOL) register {
    if ([SPMediaKeyTap usesGlobalMediaKeyTap]) {
        return [keyTap startWatchingMediaKeys];
    }
    return NO;
}

- (void) requestStop {
    [keyTap requestStopWatchingMediaKeys];
}

- (void) unregister {
    if (keyTap) {
        [keyTap stopWatchingMediaKeys];
        [keyTap release];
        keyTap = nil;
    }
}
@end

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "e850a2eb-5aaa-4322-b63e-bf1c1593805b", 36 };
    plugin->name = { "macOS Media Keys", 16 };
    plugin->version = { "1", 1 };
    plugin->author = { "clangen", 7 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
