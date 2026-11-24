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

#include "Snapshots.h"

#include <utility>

namespace {
    constexpr auto snapshotLifetime = std::chrono::hours(6);
}

const mc_play_queue* Snapshots::Get(const std::string& key) {
    const auto it = this->cache.find(key);
    if (it == this->cache.end()) {
        return nullptr;
    }
    it->second.expiry = std::chrono::steady_clock::now() + snapshotLifetime;
    return it->second.queue.get();
}

void Snapshots::Put(const std::string& key, mc_play_queue** queue) {
    this->Prune();
    std::unique_ptr<mc_play_queue, QueueDeleter> owned(*queue);
    *queue = nullptr;
    this->cache.insert_or_assign(key, Snapshot {
        .queue = std::move(owned),
        .expiry = std::chrono::steady_clock::now() + snapshotLifetime,
    });
}

void Snapshots::Remove(const std::string& key) {
    this->Prune();
    this->cache.erase(key);
}

void Snapshots::Prune() {
    const auto now = std::chrono::steady_clock::now();
    for (auto it = this->cache.begin(); it != this->cache.end();) {
        if (it->second.expiry <= now) {
            it = this->cache.erase(it);
        }
        else {
            ++it;
        }
    }
}

void Snapshots::Reset() {
    this->cache.clear();
}
