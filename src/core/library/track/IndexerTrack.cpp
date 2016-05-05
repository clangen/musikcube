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

#include <core/library/track/IndexerTrack.h>

#include <core/support/Common.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/library/LibraryBase.h>
#include <core/io/DataStreamFactory.h>

#include <boost/lexical_cast.hpp>

using namespace musik::core;

#define GENRE_TRACK_COLUMN_NAME "genre"
#define GENRES_TABLE_NAME "genres"
#define GENRE_TRACK_JUNCTION_TABLE_NAME "track_genres"
#define GENRE_TRACK_FOREIGN_KEY "genre_id"

#define ARTIST_TRACK_COLUMN_NAME "artist"
#define ARTISTS_TABLE_NAME "artists"
#define ARTIST_TRACK_JUNCTION_TABLE_NAME "track_artists"
#define ARTIST_TRACK_FOREIGN_KEY "artist_id"

IndexerTrack::IndexerTrack(DBID id)
: internalMetadata(new IndexerTrack::MetadataWithThumbnail())
, id(id)
, tempSortOrder(0)
{
}

IndexerTrack::~IndexerTrack() {
    delete this->internalMetadata;
    this->internalMetadata  = NULL;
}

std::string IndexerTrack::GetValue(const char* metakey) {
    if (metakey && this->internalMetadata) {
        MetadataMap::iterator metavalue = this->internalMetadata->metadata.find(metakey);
        if (metavalue != this->internalMetadata->metadata.end()) {
            return metavalue->second;
        }
    }

    return "";
}

void IndexerTrack::SetValue(const char* metakey, const char* value) {
    if (metakey && value) {
        this->internalMetadata->metadata.insert(
            std::pair<std::string, std::string>(metakey,value));
    }
}

void IndexerTrack::ClearValue(const char* metakey) {
    if (this->internalMetadata) {
        this->internalMetadata->metadata.erase(metakey);
    }
}

void IndexerTrack::SetThumbnail(const char *data,long size) {
    if (this->internalMetadata->thumbnailData) {
        delete this->internalMetadata->thumbnailData;
    }

    this->internalMetadata->thumbnailData = new char[size];
    this->internalMetadata->thumbnailSize = size;

    memcpy(this->internalMetadata->thumbnailData, data, size);
}

std::string IndexerTrack::URI() {
    return "";
}

std::string IndexerTrack::URL() {
    return this->GetValue("path");
}

Track::MetadataIteratorRange IndexerTrack::GetValues(const char* metakey) {
    if (this->internalMetadata) {
        return this->internalMetadata->metadata.equal_range(metakey);
    }

    return Track::MetadataIteratorRange();
}

Track::MetadataIteratorRange IndexerTrack::GetAllValues() {
    if (this->internalMetadata) {
        return Track::MetadataIteratorRange(
            this->internalMetadata->metadata.begin(),
            this->internalMetadata->metadata.end());
    }

    return Track::MetadataIteratorRange();
}

DBID IndexerTrack::Id() {
    return this->id;
}

bool IndexerTrack::NeedsToBeIndexed(
    const boost::filesystem::path &file,
    db::Connection &dbConnection, 
    DBID currentFolderId)
{
    try {
        this->SetValue("path", file.string().c_str());
        this->SetValue("filename", file.leaf().string().c_str());

        size_t lastDot = file.leaf().string().find_last_of(".");
        if (lastDot != std::string::npos){
            this->SetValue("extension", file.leaf().string().substr(lastDot + 1).c_str());
        }

        DBID fileSize = (DBID) boost::filesystem::file_size(file);
        DBTIME fileTime = (DBTIME) boost::filesystem::last_write_time(file);

        this->SetValue("filesize", boost::lexical_cast<std::string>(fileSize).c_str());
        this->SetValue("filetime", boost::lexical_cast<std::string>(fileTime).c_str());
        
        db::CachedStatement stmt(
            "SELECT id, filename, filesize, filetime " \
            "FROM tracks t " \
            "WHERE folder_id=? AND filename=?", dbConnection);

        stmt.BindInt(0, currentFolderId);
        stmt.BindText(1, this->GetValue("filename"));

        bool fileDifferent = true;

        if (stmt.Step() == db::Row) {
            this->id = stmt.ColumnInt(0);
            int dbFileSize = stmt.ColumnInt(2);
            int dbFileTime = stmt.ColumnInt(3);

            if (fileSize == dbFileSize && fileTime == dbFileTime) {
                return false;
            }
        }
    }
    catch(...) {
    }

    return true;
}

static DBID writeToTracksTable(
    db::Connection &dbConnection,
    IndexerTrack& track, 
    DBID folderId, 
    DBID tempSortOrder)
{
    db::CachedStatement stmt("INSERT OR REPLACE INTO tracks " \
        "(id, track, bpm, duration, filesize, year, folder_id, title, filename, filetime, sort_order1) " \
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", dbConnection);

    /* pull these out into local vars so they stay in scope long enough
    for sqlite to process them without making a copy. */

    stmt.BindText(1, track.GetValue("track"));
    stmt.BindText(2, track.GetValue("bpm"));
    stmt.BindText(3, track.GetValue("duration"));
    stmt.BindText(4, track.GetValue("filesize"));
    stmt.BindText(5, track.GetValue("year"));
    stmt.BindInt(6, folderId);
    stmt.BindText(7, track.GetValue("title"));
    stmt.BindText(8, track.GetValue("filename"));
    stmt.BindText(9, track.GetValue("filetime"));
    stmt.BindInt(10, tempSortOrder);

    if (track.Id() != 0) {
        stmt.BindInt(0, track.Id());
    }

    if (stmt.Step() == db::Done) {
        if (track.Id() == 0) {
            return dbConnection.LastInsertedId();
        }
    }

    return track.Id();
}

static void removeRelation(
    db::Connection& connection, 
    const std::string& field, 
    DBID trackId) 
{
    std::string query = boost::str(boost::format("DELETE FROM %1% WHERE track_id=?") % field);
    db::CachedStatement stmt(query.c_str(), connection);
    stmt.BindInt(0, trackId);
    stmt.Step();
}

static void removeKnownFields(Track::MetadataMap& metadata) {
    metadata.erase("track");
    metadata.erase("bpm");
    metadata.erase("duration");
    metadata.erase("year");
    metadata.erase("title");
    metadata.erase("filename");
    metadata.erase("filetime");
    metadata.erase("filesize");
    metadata.erase("title");
    metadata.erase("path");
    metadata.erase("extension");
    metadata.erase("genre");
    metadata.erase("artist");
    metadata.erase("album");
}

DBID IndexerTrack::ExtractThumbnail(db::Connection& connection, const std::string& libraryDirectory) {
    DBID thumbnailId = 0;
    
    if (this->internalMetadata->thumbnailData) {
        UINT64 sum = Checksum(this->internalMetadata->thumbnailData, this->internalMetadata->thumbnailSize);
    
        db::CachedStatement thumbs("SELECT id FROM thumbnails WHERE filesize=? AND checksum=?", connection);
        thumbs.BindInt(0, this->internalMetadata->thumbnailSize);
        thumbs.BindInt(1, sum);
    
        if (thumbs.Step()==db::Row) {
            thumbnailId = thumbs.ColumnInt(0); /* thumbnail already exists */
        }
    
        if (thumbnailId == 0) { /* doesn't exist yet, let's insert the record and write the file */
            db::Statement insertThumb("INSERT INTO thumbnails (filesize,checksum) VALUES (?,?)", connection);
            insertThumb.BindInt(0, this->internalMetadata->thumbnailSize);
            insertThumb.BindInt(1, sum);
    
            if (insertThumb.Step() == db::Done) {
                thumbnailId = connection.LastInsertedId();

                std::string filename =
                    libraryDirectory +
                    "thumbs/" +
                    boost::lexical_cast<std::string>(thumbnailId) +
                    ".jpg";

#ifdef WIN32
                std::wstring wfilename = u8to16(filename);
                FILE *thumbFile = _wfopen(wfilename.c_str(), _T("wb"));
#else
                FILE *thumbFile = fopen(filename.c_str(), "wb");
#endif
                fwrite(this->internalMetadata->thumbnailData, sizeof(char), this->internalMetadata->thumbnailSize, thumbFile);
                fclose(thumbFile);
            }
        }
    }

    return thumbnailId;
}

void IndexerTrack::ProcessNonStandardMetadata(db::Connection& connection) {
    MetadataMap unknownFields(this->internalMetadata->metadata);
    removeKnownFields(unknownFields);

    db::CachedStatement selectMetaKey("SELECT id FROM meta_keys WHERE name=?", connection);
    db::CachedStatement selectMetaValue("SELECT id FROM meta_values WHERE meta_key_id=? AND content=?", connection);
    db::CachedStatement insertMetaValue("INSERT INTO meta_values (meta_key_id,content) VALUES (?,?)", connection);
    db::CachedStatement insertTrackMeta("INSERT INTO track_meta (track_id,meta_value_id) VALUES (?,?)", connection);
    db::CachedStatement insertMetaKey("INSERT INTO meta_keys (name) VALUES (?)", connection);

    MetadataMap::const_iterator it = unknownFields.begin();
    for ( ; it != unknownFields.end(); ++it){
        DBID keyId = 0;
        std::string key;
    
        /* lookup the ID for the key; insert if it doesn't exist.. */

        selectMetaKey.BindText(0, it->first);
    
        if (selectMetaKey.Step() == db::Row) {
            keyId = selectMetaKey.ColumnInt(0);
        }
        else {
            insertMetaKey.BindText(0, it->first);
            
            if (insertMetaKey.Step() == db::Done) {
                keyId = connection.LastInsertedId();
            }
        }

        selectMetaKey.Reset();
    
        if (keyId == 0) {
            continue; /* welp... */
        }

        /* see if we already have the value as a normalized row in our table.
        if we don't, insert it. */

        DBID valueId = 0;

        selectMetaValue.BindInt(0, keyId);
        selectMetaValue.BindText(1, it->second);
    
        if (selectMetaValue.Step() == db::Row) {
            valueId = selectMetaValue.ColumnInt(0);
        }
        else {
            insertMetaValue.BindInt(0, keyId);
            insertMetaValue.BindText(1, it->second);

            if (insertMetaValue.Step() == db::Done) {
                valueId = connection.LastInsertedId();
            }

            insertMetaValue.Reset();
        }

        selectMetaValue.Reset();
    
        /* now that we have a keyId and a valueId, create the relationship */

        if (valueId != 0) {
            insertTrackMeta.BindInt(0, this->id);
            insertTrackMeta.BindInt(1, valueId);
            insertTrackMeta.Step();
            insertTrackMeta.Reset();
        }
    }
}

DBID IndexerTrack::ExtractAlbum(db::Connection& dbConnection) {
    DBID albumId = 0;
    db::CachedStatement stmt("SELECT id FROM albums WHERE name=?", dbConnection);
    std::string album = this->GetValue("album");
    
    stmt.BindText(0, album);
    
    if (stmt.Step() == db::Row) {
        albumId = stmt.ColumnInt(0);
    }
    else {
        db::Statement insertAlbum("INSERT INTO albums (name) VALUES (?)", dbConnection);
        insertAlbum.BindText(0,album);
    
        if (insertAlbum.Step() == db::Done) {
            albumId = dbConnection.LastInsertedId();
        }
    }

    return albumId;
}

DBID IndexerTrack::ExtractAndSaveMultiValueField(
    db::Connection& connection,
    const std::string& tracksTableColumnName,
    const std::string& fieldTableName,
    const std::string& junctionTableName,
    const std::string& junctionTableForeignKeyColumnName)
{
    std::string aggregatedValue;
    DBID fieldId = 0;
    int count = 0;

    std::set<std::string> processed; /* for deduping */

    MetadataIteratorRange genres = this->GetValues(tracksTableColumnName.c_str());
    while (genres.first != genres.second) {
        if (processed.find(genres.first->second) == processed.end()) {
            processed.insert(genres.first->second);

            std::string value = genres.first->second;

            fieldId = SaveNormalizedFieldValue(
                connection,
                fieldTableName,
                value,
                false,
                junctionTableName,
                junctionTableForeignKeyColumnName);

            if (count != 0) {
                aggregatedValue += ", ";
            }

            aggregatedValue += value;

            ++count;
        }

        ++genres.first;
    }

    if (count > 1 || fieldId == 0) {
        fieldId = SaveNormalizedFieldValue(
            connection,
            fieldTableName,
            aggregatedValue,
            true);
    }

    return fieldId;
}

DBID IndexerTrack::ExtractGenre(db::Connection& dbConnection) {
    return this->ExtractAndSaveMultiValueField(
        dbConnection,
        GENRE_TRACK_COLUMN_NAME,
        GENRES_TABLE_NAME,
        GENRE_TRACK_JUNCTION_TABLE_NAME,
        GENRE_TRACK_FOREIGN_KEY);
}

DBID IndexerTrack::ExtractArtist(db::Connection& dbConnection) {
    return this->ExtractAndSaveMultiValueField(
        dbConnection,
        ARTIST_TRACK_COLUMN_NAME,
        ARTISTS_TABLE_NAME,
        ARTIST_TRACK_JUNCTION_TABLE_NAME,
        ARTIST_TRACK_FOREIGN_KEY);
}

bool IndexerTrack::Save(db::Connection &dbConnection, std::string libraryDirectory, DBID folderId) {
    db::ScopedTransaction transaction(dbConnection);

    /* remove existing relations -- we're going to update them with fresh data */

    if (this->id != 0) {
        removeRelation(dbConnection, "track_genres", this->id);
        removeRelation(dbConnection, "track_artists", this->id);
        removeRelation(dbConnection, "track_meta", this->id);
    }

    /* write generic info to the tracks table */

    this->id = writeToTracksTable(dbConnection, *this, folderId, tempSortOrder);

    DBID albumId = this->ExtractAlbum(dbConnection);
    DBID genreId = this->ExtractGenre(dbConnection);
    DBID artistId = this->ExtractArtist(dbConnection);
    DBID thumbnailId = this->ExtractThumbnail(dbConnection, libraryDirectory);

    /* update all of the track foreign keys */

    {
        db::CachedStatement stmt(
            "UPDATE tracks " \
            "SET album_id=?, visual_genre_id=?, visual_artist_id=?, thumbnail_id=? " \
            "WHERE id=?", dbConnection);

        stmt.BindInt(0, albumId);
        stmt.BindInt(1, genreId);
        stmt.BindInt(2, artistId);
        stmt.BindInt(3, thumbnailId);
        stmt.BindInt(4, this->id);
        stmt.Step();
    }

    ProcessNonStandardMetadata(dbConnection);

    return true;
}

DBID IndexerTrack::SaveNormalizedFieldValue(
    db::Connection &dbConnection,
    const std::string& tableName,
    const std::string& fieldValue,
    bool isAggregatedValue,
    const std::string& relationJunctionTableName,
    const std::string& relationJunctionTableColumn)
{
    DBID fieldId = 0;

    /* find by value */

    {
        std::string query = boost::str(boost::format("SELECT id FROM %1% WHERE name=?") % tableName);
        db::CachedStatement stmt(query.c_str(), dbConnection);
        stmt.BindText(0, fieldValue);

        if (stmt.Step() == db::Row) {
            fieldId = stmt.ColumnInt(0);
        }
    }

    /* not found? insert. */

    if (fieldId == 0) {
        std::string query = boost::str(boost::format(
            "INSERT INTO %1% (name, aggregated) VALUES (?, ?)") % tableName);

        db::CachedStatement stmt(query.c_str(), dbConnection);
        stmt.BindText(0, fieldValue);
        stmt.BindInt(1, isAggregatedValue ? 1 : 0);

        if (stmt.Step() == db::Done) {
            fieldId = dbConnection.LastInsertedId();
        }
    }

    /* if this is a normalized value it may need to be inserted into a
    junction table. see if we were asked to do this... */

    if (relationJunctionTableName.size() && relationJunctionTableColumn.size()) {
        std::string query = boost::str(boost::format(
            "INSERT INTO %1% (track_id, %2%) VALUES (?, ?)")
            % relationJunctionTableName % relationJunctionTableColumn);

        db::Statement stmt(query.c_str(), dbConnection);
        stmt.BindInt(0, this->id);
        stmt.BindInt(1, fieldId);
        stmt.Step();
    }

    return fieldId;
}

TrackPtr IndexerTrack::Copy() {
    return TrackPtr(new IndexerTrack(this->id));
}

bool IndexerTrack::Reload(db::Connection &db) {
    db::Statement genres(
        "SELECT g.name " \
        "FROM genres g, track_genres tg " \
        "WHERE tg.genre_id=g.id AND tg.track_id=? " \
        "ORDER BY tg.id", db);
    
    db::Statement artists(
        "SELECT ar.name " \
        "FROM artists ar, track_artists ta " \
        "WHERE ta.artist_id=ar.id AND ta.track_id=? "\
        "ORDER BY ta.id", db);

    db::Statement allMetadata(
        "SELECT mv.content, mk.name " \
        "FROM meta_values mv, meta_keys mk, track_meta tm " \
        "WHERE tm.track_id=? AND tm.meta_value_id=mv.id AND mv.meta_key_id=mk.id " \
        "ORDER BY tm.id", db);

    db::Statement track(
        "SELECT t.track, t.bpm, t.duration, t.filesize, t.year, t.title, t.filename, t.thumbnail_id, p.path|| f.relative_path || '/' || t.filename, al.name, t.filetime, t.sort_order1 " \
        "FROM tracks t, folders f, paths p, albums al " \
        "WHERE t.id=? AND t.folder_id=f.id AND f.path_id=p.id AND t.album_id=al.id", db);

    track.BindInt(0, this->id);
    if(track.Step() == db::Row) {
        this->SetValue("track", track.ColumnText(0));
        this->SetValue("bpm", track.ColumnText(1));
        this->SetValue("duration", track.ColumnText(2));
        this->SetValue("filesize", track.ColumnText(3));
        this->SetValue("year", track.ColumnText(4));
        this->SetValue("title", track.ColumnText(5));
        this->SetValue("filename", track.ColumnText(6));
        this->SetValue("thumbnail_id", track.ColumnText(7));
        this->SetValue("path", track.ColumnText(8));
        this->SetValue("album", track.ColumnText(9));
        this->SetValue("filetime", track.ColumnText(10));
        this->tempSortOrder = track.ColumnInt(11);

        genres.BindInt(0, this->id);
        while (genres.Step() == db::Row) {
            this->SetValue("genre", genres.ColumnText(0));
        }

        artists.BindInt(0, this->id);
        while (artists.Step()==db::Row) {
            this->SetValue("artist", artists.ColumnText(0));
        }

        allMetadata.BindInt(0, this->id);
        while (allMetadata.Step() == db::Row) {
            this->SetValue(allMetadata.ColumnText(1), allMetadata.ColumnText(0));
        }

        return true;
    }

    return false;
}

IndexerTrack::MetadataWithThumbnail::MetadataWithThumbnail()
: thumbnailData(NULL)
, thumbnailSize(0)
{
}

IndexerTrack::MetadataWithThumbnail::~MetadataWithThumbnail() {
    delete this->thumbnailData;
}