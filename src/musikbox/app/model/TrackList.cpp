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

#include "stdafx.h"
#include "TrackList.h"

#include <core/library/query/QueryBase.h>
#include <core/library/track/LibraryTrack.h>
#include <core/library/LocalLibraryConstants.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>

#include <map>

#define MAX_SIZE 50

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::LibraryPtr;
using musik::core::ILibrary;

using namespace musik::core::db;
using namespace musik::core::query;
using namespace musik::core::library::constants;
using namespace musik::box;

class TrackMetadataQuery : public QueryBase {
    public:
        TrackMetadataQuery(DBID trackId, LibraryPtr library);
        virtual ~TrackMetadataQuery() { }

        TrackPtr Result() { return this->result; }

    protected:
        virtual bool OnRun(Connection& db);
        virtual std::string Name() { return "TrackMetadataQuery"; }

    private:
        DBID trackId;
        LibraryPtr library;
        TrackPtr result;
};

TrackList::TrackList(LibraryPtr library) {
    this->library = library;
}

TrackList::~TrackList() {

}

size_t TrackList::Count() {
    return ids.size();
}

void TrackList::Add(const DBID& id) {
    this->ids.push_back(id);
}

TrackPtr TrackList::Get(size_t index) {
    auto id = this->ids.at(index);
    auto cached = this->GetFromCache(id);

    if (cached) {
        return cached;
    }

    std::shared_ptr<TrackMetadataQuery> query(
        new TrackMetadataQuery(id, this->library));

    this->library->Enqueue(query, ILibrary::QuerySynchronous);
    this->AddToCache(id, query->Result());
    return query->Result();
}

void TrackList::CopyFrom(TrackList& from) {
    this->Clear();

    std::copy(
        from.ids.begin(),
        from.ids.end(),
        std::back_inserter(this->ids));
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

TrackPtr TrackList::GetFromCache(DBID key) {
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

void TrackList::AddToCache(DBID key, TrackPtr value) {
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

TrackMetadataQuery::TrackMetadataQuery(DBID trackId, LibraryPtr library) {
    this->trackId = trackId;
    this->library = library;
}

bool TrackMetadataQuery::OnRun(Connection& db) {
    static const std::string query =
        "SELECT DISTINCT t.id, t.track, t.disc, t.bpm, t.duration, t.filesize, t.year, t.title, t.filename, t.thumbnail_id, al.name AS album, gn.name AS genre, ar.name AS artist, t.filetime "
        "FROM tracks t, paths p, albums al, artists ar, genres gn "
        "WHERE t.id=? AND t.album_id=al.id AND t.visual_genre_id=gn.id AND t.visual_artist_id=ar.id ";

    Statement trackQuery(query.c_str(), db);
    trackQuery.BindInt(0, this->trackId);

    if (trackQuery.Step() == Row) {
        DBID id = trackQuery.ColumnInt64(0);
        TrackPtr track = TrackPtr(new LibraryTrack(id, this->library));
        track->SetValue(Track::TRACK_NUM, trackQuery.ColumnText(1));
        track->SetValue(Track::DISC_NUM, trackQuery.ColumnText(2));
        track->SetValue(Track::BPM, trackQuery.ColumnText(3));
        track->SetValue(Track::DURATION, trackQuery.ColumnText(4));
        track->SetValue(Track::FILESIZE, trackQuery.ColumnText(5));
        track->SetValue(Track::YEAR, trackQuery.ColumnText(6));
        track->SetValue(Track::TITLE, trackQuery.ColumnText(7));
        track->SetValue(Track::FILENAME, trackQuery.ColumnText(8));
        track->SetValue(Track::THUMBNAIL_ID, trackQuery.ColumnText(9));
        track->SetValue(Track::ALBUM, trackQuery.ColumnText(10));
        track->SetValue(Track::GENRE, trackQuery.ColumnText(11));
        track->SetValue(Track::ARTIST, trackQuery.ColumnText(12));
        track->SetValue(Track::FILETIME, trackQuery.ColumnText(13));

        this->result = track;
        return true;
    }

    return false;
}
