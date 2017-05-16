//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/sdk/ITrackList.h>
#include <core/sdk/ITrackListEditor.h>

#include <core/library/track/Track.h>
#include <core/library/ILibrary.h>

#include <unordered_map>
#include <list>

namespace musik { namespace core {

    /* <ugh> */
    class TrackList_ITrackList : public musik::core::sdk::ITrackList {
        public: virtual void Release() { /* not used by the SDK */ }
    };

    class TrackList_ITrackListEditor : public musik::core::sdk::ITrackListEditor {
        public: virtual void Release() { /* not used by the SDK */ }
    };
    /* </ugh> */

    class TrackList :
        public TrackList_ITrackList,
        public TrackList_ITrackListEditor
    {
        public:
            TrackList(ILibraryPtr library);
            virtual ~TrackList();

            /* ITrackList */
            virtual size_t Count() const;
            virtual musik::core::sdk::IRetainedTrack* GetRetainedTrack(size_t index) const;
            virtual int64_t GetId(size_t index) const;
            virtual int IndexOf(int64_t id) const;
            virtual musik::core::sdk::ITrack* GetTrack(size_t index) const;

            /* ITrackListEditor */
            virtual void Add(const int64_t id);
            virtual bool Insert(int64_t id, size_t index);
            virtual bool Swap(size_t index1, size_t index2);
            virtual bool Move(size_t from, size_t to);
            virtual bool Delete(size_t index);
            virtual void Clear();
            virtual void Shuffle();

            /* implementation specific */
            TrackPtr Get(size_t index) const;
            void ClearCache();
            void Swap(TrackList& list);
            void CopyFrom(const TrackList& from);

        private:
            typedef std::list<int64_t> CacheList;
            typedef std::pair<TrackPtr, CacheList::iterator> CacheValue;
            typedef std::unordered_map<int64_t, CacheValue> CacheMap;

            TrackPtr GetFromCache(int64_t key) const;
            void AddToCache(int64_t key, TrackPtr value) const;

            /* lru cache structures */
            mutable CacheList cacheList;
            mutable CacheMap cacheMap;

            std::vector<int64_t> ids;
            ILibraryPtr library;
    };
} }
