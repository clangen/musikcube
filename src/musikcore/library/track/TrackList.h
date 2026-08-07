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

/** @file TrackList.h
 *  @brief An ordered list of track ids backed by a library, with an LRU cache.
 *  @details TrackList stores track ids and lazily loads Track objects from a
 *      library. A query window plus an LRU cache keep recently accessed tracks in
 *      memory. Also provides the SDK ITrackList interface and a TrackListEditor. */

#include <musikcore/sdk/ITrackList.h>
#include <musikcore/sdk/ITrackListEditor.h>

#include <musikcore/library/track/Track.h>
#include <musikcore/library/ILibrary.h>

#include <sigslot/sigslot.h>

#include <unordered_map>
#include <list>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief An ordered, lazily-loaded collection of tracks.
     *  @details Tracks are referenced by id; Track objects are fetched from the
     *      library on demand and cached. Supports insert/swap/move/delete,
     *      shuffle, and caching of a contiguous window around the playing index. */
    class TrackList :
        public musik::core::sdk::ITrackList,
        public std::enable_shared_from_this<TrackList>,
        public sigslot::has_slots<>
    {
        public:
            /** @brief Emitted when a window of tracks is cached.
             *  @details Arguments: the list, the first index and the last index. */
            mutable sigslot::signal3<const TrackList*, size_t, size_t> WindowCached;

            /** @brief Creates an empty track list bound to a library.
             *  @param library The library used to load tracks. */
            TrackList(ILibraryPtr library);
            /** @brief Copies another track list (by pointer).
             *  @param other The list to copy. */
            TrackList(TrackList* other);
            /** @brief Copies another track list (by shared pointer).
             *  @param other The list to copy. */
            TrackList(std::shared_ptr<TrackList> other);
            /** @brief Creates a track list from explicit ids.
             *  @param library The library used to load tracks.
             *  @param trackIds The initial track ids.
             *  @param trackIdCount Number of ids. */
            TrackList(ILibraryPtr library, const int64_t* trackIds, size_t trackIdCount);

            /* ITrackList */
            /** @return The number of tracks in the list.
             *  @note O(n); an internal count is not maintained. */
            size_t Count() const noexcept override;
            /** @return The id of the track at the given index.
             *  @param index Zero-based index. */
            int64_t GetId(size_t index) const override;
            /** @return The index of the track with the given id, or -1.
             *  @param id The track id. */
            int IndexOf(int64_t id) const override;
            /** @return The track at the given index, or nullptr.
             *  @param index Zero-based index. */
            musik::core::sdk::ITrack* GetTrack(size_t index) const override;
            /** @brief No-op (track lists are not reference counted). */
            void Release() noexcept override { /* not used now */ }

            /* TrackListEditor passes through to us */
            /** @brief Appends a track id to the list.
             *  @param id The track id. */
            void Add(const int64_t id);
            /** @brief Inserts a track id at an index.
             *  @param id The track id.
             *  @param index Destination index.
             *  @return true on success. */
            bool Insert(int64_t id, size_t index);
            /** @brief Swaps two entries.
             *  @param index1 First index.
             *  @param index2 Second index.
             *  @return true on success. */
            bool Swap(size_t index1, size_t index2);
            /** @brief Moves an entry from one index to another.
             *  @param from Source index.
             *  @param to Destination index.
             *  @return true on success. */
            bool Move(size_t from, size_t to);
            /** @brief Deletes the entry at the given index.
             *  @param index Index to delete.
             *  @return true on success. */
            bool Delete(size_t index);
            /** @brief Removes all entries. */
            void Clear() noexcept;
            /** @brief Shuffles the list. */
            void Shuffle();

            /* implementation specific */
            /** @return The track at the given index.
             *  @param index Zero-based index.
             *  @param async If true, load from cache only (no blocking query). */
            TrackPtr Get(size_t index, bool async = false) const;
            /** @return The track at the given index, blocking up to a timeout.
             *  @param index Zero-based index.
             *  @param timeoutMs Maximum wait time. */
            TrackPtr GetWithTimeout(size_t index, size_t timeoutMs) const;
            /** @brief Clears the internal track cache. */
            void ClearCache() noexcept;
            /** @brief Swaps contents with another list.
             *  @param list The list to swap with. */
            void Swap(TrackList& list) noexcept;
            /** @brief Copies the contents of another list into this one.
             *  @param from The source list. */
            void CopyFrom(const TrackList& from);
            /** @brief Copies this list's contents into another.
             *  @param to The destination list. */
            void CopyTo(TrackList& to);
            /** @brief Asynchronously caches a window of tracks.
             *  @param from First index.
             *  @param to Last index.
             *  @param async true to load asynchronously. */
            void CacheWindow(size_t from, size_t to, bool async) const;
            /** @brief Sets the maximum size of the LRU cache.
             *  @param size The new cache size. */
            void SetCacheWindowSize(size_t size);
            /** @return A copy of the internal track id list. */
            const std::vector<int64_t> GetIds() const { return ids; };

            /** @return This list as a raw SDK ITrackList pointer (borrowed). */
            musik::core::sdk::ITrackList* GetSdkValue();

        private:
            /** @brief A contiguous index range used for window caching. */
            struct QueryWindow {
                size_t from{ 0 }; /**< First index. */
                size_t to{ 0 };   /**< Last index. */
                /** @return true if index i falls within the window.
                 *  @param i The index to test. */
                bool Contains(size_t i) noexcept { return to > 0 && i >= from && i <= to; }
                /** @brief Resets the window to empty. */
                void Reset() noexcept { from = to = 0; }
                /** @return true if the window is valid (non-empty). */
                bool Valid() noexcept { return to > 0 && to > from; }
                /** @brief Sets the window bounds.
                 *  @param from First index.
                 *  @param to Last index. */
                void Set(size_t from, size_t to) noexcept { this->from = from; this->to = to; }
            };

            typedef std::list<int64_t> CacheList; /**< LRU ordering of cache keys. */
            typedef std::pair<TrackPtr, CacheList::iterator> CacheValue; /**< Track plus LRU position. */
            typedef std::unordered_map<int64_t, CacheValue> CacheMap; /**< Cache by track id. */

            /** @return The cached track for a key, or nullptr.
             *  @param key The track id. */
            TrackPtr GetFromCache(int64_t key) const;
            /** @brief Adds a track to the cache.
             *  @param key The track id.
             *  @param value The track to cache. */
            void AddToCache(int64_t key, TrackPtr value) const;
            /** @brief Evicts least-recently-used entries from the cache. */
            void PruneCache() const;

            /* lru cache structures */
            mutable CacheList cacheList; /**< LRU ordering. */
            mutable CacheMap cacheMap;   /**< Cache entries by track id. */
            mutable size_t cacheSize;    /**< Maximum cache size. */
            mutable QueryWindow currentWindow; /**< Currently cached window. */
            mutable QueryWindow nextWindow;    /**< Next window to cache. */

            std::vector<int64_t> ids; /**< Track ids in list order. */
            ILibraryPtr library;      /**< Library used to load tracks. */
    };

    /** @brief Adapts a TrackList to the SDK ITrackListEditor interface.
     *  @details Forwards edit operations to the wrapped TrackList. */
    class TrackListEditor : public musik::core::sdk::ITrackListEditor {
        public:
            /** @brief Wraps a shared track list.
             *  @param trackList The list to edit. */
            TrackListEditor(std::shared_ptr<TrackList> trackList) noexcept;
            /** @brief Wraps a track list by reference.
             *  @param trackList The list to edit. */
            TrackListEditor(TrackList& trackList);

            virtual ~TrackListEditor() { }

            /** @brief Appends a track id.
             *  @param id The track id. */
            void Add(const int64_t id) override;
            /** @brief Inserts a track id at an index.
             *  @param id The track id.
             *  @param index Destination index.
             *  @return true on success. */
            bool Insert(int64_t id, size_t index) override;
            /** @brief Swaps two entries.
             *  @param index1 First index.
             *  @param index2 Second index.
             *  @return true on success. */
            bool Swap(size_t index1, size_t index2) override;
            /** @brief Moves an entry from one index to another.
             *  @param from Source index.
             *  @param to Destination index.
             *  @return true on success. */
            bool Move(size_t from, size_t to) override;
            /** @brief Deletes the entry at the given index.
             *  @param index Index to delete.
             *  @return true on success. */
            bool Delete(size_t index) override;
            /** @brief Empties the list. */
            void Clear() override;
            /** @brief Shuffles the list. */
            void Shuffle() override;
            /** @brief No-op (nothing to release). */
            void Release() noexcept override { /* nothing yet */ }

        private:
            std::shared_ptr<TrackList> trackList; /**< Wrapped track list. */
    };
} }
