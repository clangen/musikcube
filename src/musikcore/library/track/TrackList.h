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

            TrackList(ILibraryPtr library);
            TrackList(TrackList* other);
            TrackList(std::shared_ptr<TrackList> other);
            TrackList(ILibraryPtr library, const int64_t* trackIds, size_t trackIdCount);

            /* ITrackList */
            size_t Count() const noexcept override;
            int64_t GetId(size_t index) const override;
            int IndexOf(int64_t id) const override;
            musik::core::sdk::ITrack* GetTrack(size_t index) const override;
            void Release() noexcept override { /* not used now */ }

            /* TrackListEditor passes through to us */
            void Add(const int64_t id);
            bool Insert(int64_t id, size_t index);
            bool Swap(size_t index1, size_t index2);
            bool Move(size_t from, size_t to);
            bool Delete(size_t index);
            void Clear() noexcept;
            void Shuffle();

            /* implementation specific */
            TrackPtr Get(size_t index, bool async = false) const;
            TrackPtr GetWithTimeout(size_t index, size_t timeoutMs) const;
            void ClearCache() noexcept;
            void Swap(TrackList& list) noexcept;
            void CopyFrom(const TrackList& from);
            void CopyTo(TrackList& to);
            void CacheWindow(size_t from, size_t to, bool async) const;
            void SetCacheWindowSize(size_t size);
            const std::vector<int64_t> GetIds() const { return ids; };

            musik::core::sdk::ITrackList* GetSdkValue();

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
