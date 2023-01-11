//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include <musikcore/sdk/ITrackList.h>
#include <musikcore/sdk/ITrackListEditor.h>

#include <musikcore/library/track/Track.h>
#include <musikcore/library/ILibrary.h>

#include <sigslot/sigslot.h>

#include <unordered_map>
#include <list>

namespace musik { namespace core {

    class TrackList :
        public musik::core::sdk::ITrackList,
        public std::enable_shared_from_this<TrackList>,
        public sigslot::has_slots<>
    {
        public:
            mutable sigslot::signal3<const TrackList*, size_t, size_t> WindowCached;

            EXPORT TrackList(ILibraryPtr library);
            EXPORT TrackList(TrackList* other);
            EXPORT TrackList(std::shared_ptr<TrackList> other);
            EXPORT TrackList(ILibraryPtr library, const int64_t* trackIds, size_t trackIdCount);

            /* ITrackList */
            EXPORT size_t Count() const noexcept override;
            EXPORT int64_t GetId(size_t index) const override;
            EXPORT int IndexOf(int64_t id) const override;
            EXPORT musik::core::sdk::ITrack* GetTrack(size_t index) const override;
            EXPORT void Release() noexcept override { /* not used now */ }

            /* TrackListEditor passes through to us */
            EXPORT void Add(const int64_t id);
            EXPORT bool Insert(int64_t id, size_t index);
            EXPORT bool Swap(size_t index1, size_t index2);
            EXPORT bool Move(size_t from, size_t to);
            EXPORT bool Delete(size_t index);
            EXPORT void Clear() noexcept;
            EXPORT void Shuffle();

            /* implementation specific */
            EXPORT TrackPtr Get(size_t index, bool async = false) const;
            EXPORT TrackPtr GetWithTimeout(size_t index, size_t timeoutMs) const;
            EXPORT void ClearCache() noexcept;
            EXPORT void Swap(TrackList& list) noexcept;
            EXPORT void CopyFrom(const TrackList& from);
            EXPORT void CopyTo(TrackList& to);
            EXPORT void CacheWindow(size_t from, size_t to, bool async) const;
            EXPORT void SetCacheWindowSize(size_t size);
            EXPORT const std::vector<int64_t> GetIds() const { return ids; };

            EXPORT musik::core::sdk::ITrackList* GetSdkValue();

        private:
            struct QueryWindow {
                size_t from{ 0 };
                size_t to{ 0 };
                bool Contains(size_t i) noexcept { return to > 0 && i >= from && i <= to; }
                void Reset() noexcept { from = to = 0; }
                bool Valid() noexcept { return to > 0 && to > from; }
                void Set(size_t from, size_t to) noexcept { this->from = from; this->to = to; }
            };

            typedef std::list<int64_t> CacheList;
            typedef std::pair<TrackPtr, CacheList::iterator> CacheValue;
            typedef std::unordered_map<int64_t, CacheValue> CacheMap;

            TrackPtr GetFromCache(int64_t key) const;
            void AddToCache(int64_t key, TrackPtr value) const;
            void PruneCache() const;

            /* lru cache structures */
            mutable CacheList cacheList;
            mutable CacheMap cacheMap;
            mutable size_t cacheSize;
            mutable QueryWindow currentWindow;
            mutable QueryWindow nextWindow;

            std::vector<int64_t> ids;
            ILibraryPtr library;
    };

    class TrackListEditor : public musik::core::sdk::ITrackListEditor {
        public:
            TrackListEditor(std::shared_ptr<TrackList> trackList) noexcept;
            TrackListEditor(TrackList& trackList);

            virtual ~TrackListEditor() { }

            void Add(const int64_t id) override;
            bool Insert(int64_t id, size_t index) override;
            bool Swap(size_t index1, size_t index2) override;
            bool Move(size_t from, size_t to) override;
            bool Delete(size_t index) override;
            void Clear() override;
            void Shuffle() override;
            void Release() noexcept override { /* nothing yet */ }

        private:
            std::shared_ptr<TrackList> trackList;
    };
} }
