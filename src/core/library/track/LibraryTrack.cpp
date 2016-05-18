//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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

#include <boost/lexical_cast.hpp>
#include <boost/thread/mutex.hpp>

using namespace musik::core;

LibraryTrack::LibraryTrack()
: meta(NULL)
, id(0)
, libraryId(0) {
}

LibraryTrack::LibraryTrack(DBID id, int libraryId)
: meta(NULL)
, id(id)
, libraryId(libraryId) {
}

LibraryTrack::LibraryTrack(DBID id, musik::core::LibraryPtr library)
: meta(NULL)
, id(id)
, libraryId(library->Id()) {
}

LibraryTrack::~LibraryTrack(){
    delete this->meta;
    this->meta = NULL;
}

std::string LibraryTrack::GetValue(const char* metakey) {
    if (metakey && this->meta) {
        if (this->meta->library) {
            boost::mutex::scoped_lock lock(this->meta->mutex);
            MetadataMap::iterator metavalue = this->meta->metadata.find(metakey);
            if (metavalue != this->meta->metadata.end()) {
                return metavalue->second;
            }
        }
        else {
            MetadataMap::iterator metavalue = this->meta->metadata.find(metakey);
            if(metavalue != this->meta->metadata.end()) {
                return metavalue->second;
            }
        }
    }

    return "";
}

void LibraryTrack::SetValue(const char* metakey, const char* value) {
    this->InitMeta();

    if (metakey && value) {
        if(this->meta->library) {
            boost::mutex::scoped_lock lock(this->meta->mutex);
            this->meta->metadata.insert(std::pair<std::string, std::string>(metakey,value));
        }
        else {
            this->meta->metadata.insert(std::pair<std::string, std::string>(metakey,value));
        }
    }
}

void LibraryTrack::ClearValue(const char* metakey) {
    if (this->meta) {
        if (this->meta->library) {
            boost::mutex::scoped_lock lock(this->meta->mutex);
            this->meta->metadata.erase(metakey);
        }
        else {
            this->meta->metadata.erase(metakey);
        }
    }
}

void LibraryTrack::SetThumbnail(const char *data, long size) {
    this->InitMeta();

    delete this->meta->thumbnailData;
    this->meta->thumbnailData = new char[size];
    this->meta->thumbnailSize = size;

    memcpy(this->meta->thumbnailData, data, size);
}

std::string LibraryTrack::URI() {
    int libraryId = this->meta ? this->meta->library->Id() : this->libraryId;
    return boost::str(boost::format("mcdb://%1%/%2%") % libraryId % this->id);
}

std::string LibraryTrack::URL() {
    return this->GetValue("path");
}

Track::MetadataIteratorRange LibraryTrack::GetValues(const char* metakey) {
    if (this->meta) {
        if (this->meta->library) {
            boost::mutex::scoped_lock lock(this->meta->mutex);
            return this->meta->metadata.equal_range(metakey);
        }
        else {
            return this->meta->metadata.equal_range(metakey);
        }
    }

    return Track::MetadataIteratorRange();
}

Track::MetadataIteratorRange LibraryTrack::GetAllValues() {
    if (this->meta) {
        return Track::MetadataIteratorRange(
            this->meta->metadata.begin(),
            this->meta->metadata.end());
    }

    return Track::MetadataIteratorRange();
}

DBID LibraryTrack::Id() {
    return this->id;
}

musik::core::LibraryPtr LibraryTrack::Library() {
    if (this->meta) {
        return this->meta->library;
    }

    return LibraryFactory::Instance().GetLibrary(this->libraryId);
}

int LibraryTrack::LibraryId() {
    return this->libraryId;
}

void LibraryTrack::InitMeta() {
    if (!this->meta) {
        this->meta = new MetaData();
        if (this->libraryId) {
            this->meta->library = LibraryFactory::Instance().GetLibrary(this->libraryId);
        }
    }
}

TrackPtr LibraryTrack::Copy() {
    return TrackPtr(new LibraryTrack(this->id, this->libraryId));
}

bool LibraryTrack::Load(Track *target, db::Connection &db) {
    /* if no ID is specified, see if we can look one up by filename
    in the current database. */
    if (target->Id() == 0) {
        std::string path = target->GetValue("filename");

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

        target->SetId(idFromFn.ColumnInt(0));
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
        "SELECT t.track, t.bpm, t.duration, t.filesize, t.year, t.title, t.filename, t.thumbnail_id, al.name, t.filetime " \
        "FROM tracks t, paths p, albums al " \
        "WHERE t.id=? AND t.album_id=al.id", db);

    trackQuery.BindInt(0, target->Id());
    if (trackQuery.Step() == db::Row) {
        target->SetValue("track", trackQuery.ColumnText(0));
        target->SetValue("bpm", trackQuery.ColumnText(1));
        target->SetValue("duration", trackQuery.ColumnText(2));
        target->SetValue("filesize", trackQuery.ColumnText(3));
        target->SetValue("year", trackQuery.ColumnText(4));
        target->SetValue("title", trackQuery.ColumnText(5));
        target->SetValue("filename", trackQuery.ColumnText(6));
        target->SetValue("thumbnail_id", trackQuery.ColumnText(7));
        target->SetValue("album", trackQuery.ColumnText(8));
        target->SetValue("filetime", trackQuery.ColumnText(9));

        genresQuery.BindInt(0, target->Id());
        while (genresQuery.Step() == db::Row) {
            target->SetValue("genre", genresQuery.ColumnText(0));
        }

        artistsQuery.BindInt(0, target->Id());
        while (artistsQuery.Step() == db::Row) {
            target->SetValue("artist", artistsQuery.ColumnText(0));
        }

        allMetadataQuery.BindInt(0, target->Id());
        while (allMetadataQuery.Step() == db::Row) {
            target->SetValue(allMetadataQuery.ColumnText(1), allMetadataQuery.ColumnText(0));
        }

        return true;
    }

    return false;
}


LibraryTrack::MetaData::MetaData()
: thumbnailData(NULL)
, thumbnailSize(0) {
}

LibraryTrack::MetaData::~MetaData() {
    delete this->thumbnailData;
}