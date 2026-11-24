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

#include <musikcore/plugin.h>
#include <musikcore/plugin/library.h>
#include <musikcore/plugin/playback-remote.h>
#include "Settings.h"

#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <string_view>
#include <utility>

class ReadWriteLock {
    typedef std::shared_mutex Mutex;
    typedef std::unique_lock<Mutex> WriteLock;
    typedef std::shared_lock<Mutex> ReadLock;

    public:
        WriteLock Write() {
            WriteLock wl(stateMutex);
            return std::move(wl);
        }

        ReadLock Read() {
            ReadLock rl(stateMutex);
            return std::move(rl);
        }

    private:
        Mutex stateMutex;
};

struct Context {
    void Log(mc_log_level level, std::string_view component, std::string_view message) const {
        const mc_string_view tag {
            .data = component.data(),
            .length = component.size(),
        };
        const mc_string_view text {
            .data = message.data(),
            .length = message.size(),
        };
        this->host->log(this->host->host_context, level, tag, text);
    }

    std::string dataDirectory;
    const mc_host_api* host { nullptr };
    const mc_library_host_api* library { nullptr };
    const mc_media_host_api* media { nullptr };
    const mc_audio_scratch_buffer_api* scratch { nullptr };
    const mc_playback_remote_host_api* playbackRemote { nullptr };
    mc_preferences_host_api preferences = MC_PREFERENCES_HOST_API_DEFAULT;
    ServerSettings settings { preferences };
    ReadWriteLock lock;
    std::atomic<bool> stopping { false };
};
