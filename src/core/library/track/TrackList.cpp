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

#include "pch.hpp"
#include "TrackList.h"

#include <core/library/query/local/LocalQueryBase.h>
#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/library/track/Track.h>
#include <core/library/query/local/TrackMetadataQuery.h>
#include <core/library/query/local/util/SdkWrappers.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>

#include <map>

#define MAX_SIZE 50

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::db::local;
using namespace musik::core::sdk;

TrackList::TrackList(ILibraryPtr library) {
    this->library = library;
}

TrackList::TrackList(TrackList* other)
: ids(other->ids)
, library(other->library) {
}

TrackList::TrackList(ILibraryPtr library, const int64_t* trackIds, size_t trackIdCount)
: library(library) {
    if (trackIdCount > 0) {
        this->ids.insert(this->ids.end(), &trackIds[0], &trackIds[trackIdCount]);
    }
}

TrackList::~TrackList() {

}

size_t TrackList::Count() const {
    return ids.size();
}

void TrackList::Add(const int64_t id) {
    this->ids.push_back(id);
}

bool TrackList::Insert(int64_t id, size_t index) {
    if (index < (int) this->ids.size()) {
        this->ids.insert(this->ids.begin() + index, id);
        return true;
    }
    this->ids.push_back(id);
    return true;
}

bool TrackList::Swap(size_t index1, size_t index2) {
    auto size = this->ids.size();
    if (index1 < size && index2 < size) {
        auto temp = this->ids[index1];
        this->ids[index1] = this->ids[index2];
        this->ids[index2] = temp;
        return true;
    }
    return false;
}

bool TrackList::Move(size_t from, size_t to) {
    auto size = this->ids.size();
    if (from < size && to < size && from != to) {
        auto temp = this->ids[from];
        this->ids.erase(this->ids.begin() + from);
        this->ids.insert(this->ids.begin() + to, temp);
        return true;
    }
    return false;
}

bool TrackList::Delete(size_t index) {
    if (index < (int) this->ids.size()) {
        this->ids.erase(this->ids.begin() + index);
        return true;
    }
    return false;
}

TrackPtr TrackList::Get(size_t index) const {
    try {
        auto id = this->ids.at(index);
        auto cached = this->GetFromCache(id);

        if (cached) {
            return cached;
        }

        auto target = TrackPtr(new LibraryTrack(id, this->library));

        std::shared_ptr<TrackMetadataQuery> query(
            new TrackMetadataQuery(target, this->library));

        this->library->Enqueue(query, ILibrary::QuerySynchronous);

        if (query->GetStatus() == IQuery::Finished) {
            this->AddToCache(id, query->Result());
            return query->Result();
        }
    }
    catch (...) {
    }

    return TrackPtr();
}

ITrack* TrackList::GetTrack(size_t index) const {
    return this->Get(index)->GetSdkValue();
}

int64_t TrackList::GetId(size_t index) const {
    return this->ids.at(index);
}

void TrackList::CopyFrom(const TrackList& from) {
    this->Clear();

    std::copy(
        from.ids.begin(),
        from.ids.end(),
        std::back_inserter(this->ids));
}

void TrackList::CopyTo(TrackList& to) {
    to.CopyFrom(this);
}

int TrackList::IndexOf(int64_t id) const {
    auto it = std::find(this->ids.begin(), this->ids.end(), id);
    return (it == this->ids.end()) ? -1 : it - this->ids.begin();
}

void TrackList::Shuffle() {
    std::random_shuffle(this->ids.begin(), this->ids.end());
}

void TrackList::Clear() {
    this->ClearCache();
    this->ids.clear();
}

void TrackList::ClearCache() {
    this->cacheList.clear();
    this->cacheMap.clear();
}

void TrackList::Swap(TrackList& tl) {
    std::swap(tl.ids, this->ids);
}

TrackPtr TrackList::GetFromCache(int64_t key) const {
    auto it = this->cacheMap.find(key);
    if (it != this->cacheMap.end()) {
        this->cacheList.splice( /* promote to front */
            this->cacheList.begin(),
            this->cacheList,
            it->second.second);

        return it->second.first;
    }

    return TrackPtr();
}

void TrackList::AddToCache(int64_t key, TrackPtr value) const {
    auto it = this->cacheMap.find(key);
    if (it != this->cacheMap.end()) {
        cacheList.erase(it->second.second);
        cacheMap.erase(it);
    }

    cacheList.push_front(key);
    this->cacheMap[key] = std::make_pair(value, cacheList.begin());

    if (this->cacheMap.size() > MAX_SIZE) {
        auto last = cacheList.end();
        --last;
        cacheMap.erase(this->cacheMap.find(*last));
        cacheList.erase(last);
    }
}

ITrackList* TrackList::GetSdkValue() {
    return new SdkTrackList(shared_from_this());
}

/* * * * * TrackListEditor * * * * */

template <typename T>
struct NoDeleter {
    void operator()(T* t) {
    }
};

TrackListEditor::TrackListEditor(std::shared_ptr<TrackList> trackList) {
    this->trackList = trackList;
}

TrackListEditor::TrackListEditor(TrackList& trackList) {
    this->trackList = std::shared_ptr<TrackList>(&trackList, NoDeleter<TrackList>());
}

TrackListEditor::~TrackListEditor() {
}

void TrackListEditor::Add(const int64_t id) {
    this->trackList->Add(id);
}

bool TrackListEditor::Insert(int64_t id, size_t index) {
    return this->trackList->Insert(id, index);
}

bool TrackListEditor::Swap(size_t index1, size_t index2) {
    return this->trackList->Swap(index1, index2);
}

bool TrackListEditor::Move(size_t from, size_t to) {
    return this->trackList->Move(from, to);
}

bool TrackListEditor::Delete(size_t index) {
    return this->trackList->Delete(index);
}

void TrackListEditor::Clear() {
    this->trackList->Clear();
}

void TrackListEditor::Shuffle() {
    this->trackList->Shuffle();
}
