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

/// @file Snapshots.h
/// @brief Time-bounded cache of play-queue snapshots.
/// @details The WebSocket server snapshots the play queue so a disconnected
/// client can restore its view. This class stores those track lists by key
/// with an expiry time and prunes entries that have been idle too long.

#include <musikcore/sdk/ITrackList.h>
#include <map>
#include <string>

/** @brief Caches snapshots of the play queue for the streaming server.
 *  @details Entries are keyed by a client-generated key and expire after an
 *  idle timeout. Reset clears everything; Prune removes expired entries so the
 *  cache does not grow unboundedly. */
class Snapshots {
    public:
        /** @brief Track list type alias. */
        using TrackList = musik::core::sdk::ITrackList;

        /** @brief Destroys the cache and releases remaining snapshots. */
        ~Snapshots();

        /** @brief Returns a snapshot by key.
         *  @param key The snapshot key.
         *  @return The cached track list, or null. */
        TrackList* Get(const std::string& key);
        /** @brief Stores a snapshot under the given key.
         *  @param key The snapshot key.
         *  @param tracks The track list to cache. */
        void Put(const std::string& key, TrackList* tracks);
        /** @brief Removes a snapshot by key.
         *  @param key The snapshot key. */
        void Remove(const std::string& key);
        /** @brief Removes snapshots whose expiry has passed. */
        void Prune();
        /** @brief Clears the entire cache. */
        void Reset();

    private:
        /** @brief A cached track list and its expiry time. */
        struct CacheKey {
            /** @brief Constructs an empty cache entry. */
            CacheKey(): CacheKey(nullptr, 0) {
            }
            /** @brief Copies a cache entry.
             *  @param ck The entry to copy. */
            CacheKey(const CacheKey& ck) {
                this->tracks = ck.tracks;
                this->expiry = ck.expiry;
            }
            /** @brief Constructs a cache entry.
             *  @param tl The cached track list.
             *  @param ex Expiry timestamp. */
            CacheKey(TrackList* tl, int64_t ex) {
                this->tracks = tl;
                this->expiry = ex;
            }
            /** @brief The cached track list. */
            TrackList* tracks;
            /** @brief Expiry timestamp of the entry. */
            int64_t expiry;
        };

        /** @brief Map of snapshot key to cached entry. */
        std::map<std::string, CacheKey> cache;
};