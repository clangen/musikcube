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

#pragma once

#include <core/sdk/IMetadataProxy.h>
#include <core/sdk/IPreferences.h>
#include <core/sdk/IPlaybackService.h>
#include <core/sdk/IEnvironment.h>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>

class ReadWriteLock {
    typedef boost::shared_mutex Mutex;
    typedef boost::unique_lock<Mutex> WriteLock;
    typedef boost::shared_lock<Mutex> ReadLock;

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
    Context() {
        this->metadataProxy = nullptr;
        this->prefs = nullptr;
        this->playback = nullptr;
    }

    musik::core::sdk::IMetadataProxy* metadataProxy;
    musik::core::sdk::IPreferences* prefs;
    musik::core::sdk::IPlaybackService* playback;
    musik::core::sdk::IEnvironment* environment;
    ReadWriteLock lock;
};