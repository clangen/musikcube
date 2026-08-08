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

///

/// @file LruDiskCache.h
/// @brief LRU disk cache used by the HTTP data stream plugin.
/// @details Persists downloaded audio to a directory on disk. Entries are keyed
/// by a stream id and an instance id, capped at a maximum entry count, and
/// pruned least-recently-used. Cache entries are restored from disk on init.

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <ctime>
#include <cstdint>
#include <filesystem>

/** @brief Least-recently-used on-disk cache for downloaded streams.
 *  @details Each cache entry is a file whose name encodes an id and instance
 *  id. Opening a cached id reuses the stored file (avoids a re-download),
 *  while opening a missing id creates a new scratch file. Purge removes files
 *  that have expired or exceed the configured entry limit. */
class LruDiskCache {
    public:
        /** @brief Constructs an uninitialized cache. */
        LruDiskCache();

        /** @brief Removes expired or excess cache entries.
         *  @details Deletes files older than a time threshold and prunes the
         *  list down to the configured maximum number of entries. */
        void Purge();

        /** @brief Marks a cache entry as complete and records its type.
         *  @param id The stream id of the entry.
         *  @param instanceId The instance id of the entry.
         *  @param type Content type of the cached data.
         *  @return True if the entry was finalized. */
        bool Finalize(size_t id, int64_t instanceId, std::string type);
        /** @brief Opens a cache file for the given id, creating it if needed.
         *  @param id The stream id.
         *  @param instanceId The instance id.
         *  @param mode The fopen mode.
         *  @return A FILE handle, or null on failure. */
        FILE* Open(size_t id, int64_t instanceId, const std::string& mode);
        /** @brief Opens a cache file, returning its metadata.
         *  @param id The stream id.
         *  @param instanceId The instance id.
         *  @param mode The fopen mode.
         *  @param type Receives the cached content type.
         *  @param len Receives the cached file length.
         *  @return A FILE handle, or null on failure. */
        FILE* Open(size_t id, int64_t instanceId, const std::string& mode, std::string& type, size_t& len);
        /** @brief Returns whether a complete entry exists for the id.
         *  @param id The stream id.
         *  @return True if the id is cached and valid. */
        bool Cached(size_t id);
        /** @brief Deletes the cache entry for the given id.
         *  @param id The stream id.
         *  @param instanceId The instance id. */
        void Delete(size_t id, int64_t instanceId);
        /** @brief Marks the entry as recently used.
         *  @param id The stream id. */
        void Touch(size_t id);

        /** @brief Configures the cache root and maximum entry count.
         *  @param root Directory the cache files live in.
         *  @param maxEntries Maximum number of cache entries to keep. */
        void Init(const std::string& root, size_t maxEntries);

    private:
        /** @brief Metadata for a single cached entry. */
        struct Entry {
            /** @brief Stream id of the entry. */
            uint64_t id;
            /** @brief Path of the cache file. */
            std::string path;
            /** @brief Content type of the cached data. */
            std::string type;
            /** @brief Last modification time of the file. */
            std::filesystem::file_time_type time;
        };

        /** @brief Shared pointer to a cache entry. */
        using EntryPtr = std::shared_ptr<Entry>;
        /** @brief List of cache entries. */
        using EntryList = std::vector<EntryPtr>;

        /** @brief Sorts entries by recency and removes excess entries. */
        void SortAndPrune();

        /** @brief Restores an entry from a cache file path.
         *  @param path The cache file path.
         *  @return The parsed entry, or null if invalid. */
        static std::shared_ptr<Entry> Parse(const std::filesystem::path& path);

        /** @brief Guards the cache state. */
        std::recursive_mutex stateMutex;

        /** @brief Whether the cache has been initialized. */
        bool initialized;
        /** @brief Maximum number of entries retained. */
        size_t maxEntries;
        /** @brief The currently known cache entries. */
        EntryList cached;
        /** @brief Root directory of the cache. */
        std::string root;
};
