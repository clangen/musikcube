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

#include <core/library/track/LibraryTrack.h>
#include <core/library/LibraryFactory.h>

#include <core/support/Common.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/library/LocalLibrary.h>

using namespace musik::core;

LibraryTrack::LibraryTrack()
: id(0)
, libraryId(0) {
}

LibraryTrack::LibraryTrack(int64_t id, int libraryId)
: id(id)
, libraryId(libraryId) {
}

LibraryTrack::LibraryTrack(int64_t id, musik::core::ILibraryPtr library)
: id(id)
, libraryId(library->Id()) {
}

LibraryTrack::~LibraryTrack() {
}

std::string LibraryTrack::GetString(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    MetadataMap::iterator metavalue = this->metadata.find(metakey);
    if (metavalue != this->metadata.end()) {
        return metavalue->second;
    }
    return "";
}

long long LibraryTrack::GetInt64(const char* key, long long defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stoll(GetString(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

int LibraryTrack::GetInt32(const char* key, unsigned int defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stol(GetString(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

double LibraryTrack::GetDouble(const char* key, double defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stod(GetString(key));
        }
    }
    catch (...) {
    }
    return defaultValue;
}

void LibraryTrack::SetValue(const char* metakey, const char* value) {
    std::unique_lock<std::mutex> lock(this->mutex);
    this->metadata.insert(std::pair<std::string, std::string>(metakey,value));
}

void LibraryTrack::ClearValue(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    this->metadata.erase(metakey);
}

bool LibraryTrack::Contains(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return this->metadata.find(metakey) != this->metadata.end();
}

void LibraryTrack::SetThumbnail(const char *data, long size) {
    /* do nothing. we implement a fat interface; this is just used by
    IndexerTrack. */
}

bool LibraryTrack::ContainsThumbnail() {
    std::unique_lock<std::mutex> lock(this->mutex);
    auto it = this->metadata.find("thumbnail_id");
    if (it != this->metadata.end()) {
        return it->second.size() > 0;
    }
    return false;
}

void LibraryTrack::SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) {
    /* do nothing, we don't use this value and this method should never be
    called. it's used by the IndexerTrack implementation, and also by the
    playback engine, but not here in the library. */
}

std::string LibraryTrack::Uri() {
    return this->GetString("filename");
}

int LibraryTrack::GetString(const char* key, char* dst, int size) {
    return CopyString(this->GetString(key), dst, size);
}

int LibraryTrack::Uri(char* dst, int size) {
    return CopyString(this->Uri(), dst, size);
}

Track::MetadataIteratorRange LibraryTrack::GetValues(const char* metakey) {
    std::unique_lock<std::mutex> lock(this->mutex);
    return this->metadata.equal_range(metakey);
}

Track::MetadataIteratorRange LibraryTrack::GetAllValues() {
    return Track::MetadataIteratorRange(
        this->metadata.begin(),
        this->metadata.end());

    return Track::MetadataIteratorRange();
}

int64_t LibraryTrack::GetId() {
    return this->id;
}

int LibraryTrack::LibraryId() {
    return this->libraryId;
}

TrackPtr LibraryTrack::Copy() {
    return TrackPtr(new LibraryTrack(this->id, this->libraryId));
}

bool LibraryTrack::Load(Track *target, db::Connection &db) {
    /* if no ID is specified, see if we can look one up by filename
    in the current database. */
    if (target->GetId() == 0) {
        std::string path = target->GetString("filename");

        if (!path.size()) {
            return false;
        }

        db::Statement idFromFn(
            "SELECT id " \
            "FROM tracks " \
            "WHERE filename=? " \
            "LIMIT 1", db);

        idFromFn.BindText(0, path.c_str());

        if (idFromFn.Step() != db::Row) {
            return false;
        }

        target->SetId(idFromFn.ColumnInt64(0));
    }

    db::Statement genresQuery(
        "SELECT g.name " \
        "FROM genres g, track_genres tg " \
        "WHERE tg.genre_id=g.id AND tg.track_id=? " \
        "ORDER BY tg.id", db);

    db::Statement artistsQuery(
        "SELECT ar.name " \
        "FROM artists ar, track_artists ta " \
        "WHERE ta.artist_id=ar.id AND ta.track_id=? "\
        "ORDER BY ta.id", db);

    db::Statement allMetadataQuery(
        "SELECT mv.content, mk.name " \
        "FROM meta_values mv, meta_keys mk, track_meta tm " \
        "WHERE tm.track_id=? AND tm.meta_value_id=mv.id AND mv.meta_key_id=mk.id " \
        "ORDER BY tm.id", db);

    db::Statement trackQuery(
        "SELECT t.track, t.disc, t.bpm, t.duration, t.filesize, t.title, t.filename, t.thumbnail_id, al.name, t.filetime, t.visual_genre_id, t.visual_artist_id, t.album_artist_id, t.album_id, t.rating " \
        "FROM tracks t, paths p, albums al " \
        "WHERE t.id=? AND t.album_id=al.id", db);

    trackQuery.BindInt64(0, (int64_t) target->GetId());
    if (trackQuery.Step() == db::Row) {
        target->SetValue("track", trackQuery.ColumnText(0));
        target->SetValue("disc", trackQuery.ColumnText(1));
        target->SetValue("bpm", trackQuery.ColumnText(2));
        target->SetValue("duration", trackQuery.ColumnText(3));
        target->SetValue("filesize", trackQuery.ColumnText(4));
        target->SetValue("title", trackQuery.ColumnText(5));
        target->SetValue("filename", trackQuery.ColumnText(6));
        target->SetValue("thumbnail_id", trackQuery.ColumnText(7));
        target->SetValue("album", trackQuery.ColumnText(8));
        target->SetValue("filetime", trackQuery.ColumnText(9));
        target->SetValue("visual_genre_id", trackQuery.ColumnText(10));
        target->SetValue("visual_artist_id", trackQuery.ColumnText(11));
        target->SetValue("album_artist_id", trackQuery.ColumnText(12));
        target->SetValue("album_id", trackQuery.ColumnText(13));
        target->SetValue("rating", trackQuery.ColumnText(14));

        genresQuery.BindInt64(0, (int64_t) target->GetId());
        while (genresQuery.Step() == db::Row) {
            target->SetValue("genre", genresQuery.ColumnText(0));
        }

        artistsQuery.BindInt64(0, (int64_t) target->GetId());
        while (artistsQuery.Step() == db::Row) {
            target->SetValue("artist", artistsQuery.ColumnText(0));
        }

        allMetadataQuery.BindInt64(0, (int64_t) target->GetId());
        while (allMetadataQuery.Step() == db::Row) {
            target->SetValue(allMetadataQuery.ColumnText(1), allMetadataQuery.ColumnText(0));
        }

        return true;
    }

    return false;
}