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

#include "Snapshots.h"
#include <chrono>
#include <algorithm>

using TrackList = Snapshots::TrackList;
using namespace std::chrono;

static const int64_t SIX_HOURS_MILLIS = 1000 * 60 * 60 * 6;

static inline int64_t now() {
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

static inline int64_t expiry() {
    return SIX_HOURS_MILLIS + now();
}

static inline bool expired(int64_t expiry) {
    return now() >= expiry;
}

Snapshots::~Snapshots() {
    Reset();
}

TrackList* Snapshots::Get(const std::string& key) {
    auto it = this->cache.find(key);
    if (it != this->cache.end()) {
        this->cache[key] = CacheKey(it->second.tracks, expiry());
        return it->second.tracks;
    }
    return nullptr;
}

void Snapshots::Put(const std::string& key, TrackList* tracks) {
    this->Prune();
    this->Remove(key);
    this->cache[key] = CacheKey(tracks, expiry());
}

void Snapshots::Remove(const std::string& key) {
    this->Prune();
    auto it = this->cache.find(key);
    if (it != this->cache.end()) {
        it->second.tracks->Release();
        this->cache.erase(it);
    }
}

void Snapshots::Prune() {
    auto it = this->cache.begin();
    while (it != this->cache.end()) {
        if (expired(it->second.expiry)) {
            it->second.tracks->Release();
            it = this->cache.erase(it);
            continue;
        }
        ++it;
    }
}

void Snapshots::Reset() {
    for (auto it : cache) {
        it.second.tracks->Release();
    }
    this->cache.clear();
}