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

#include "pch.hpp"

#include <core/library/track/IndexerTrack.h>

#include <core/support/Common.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/library/LocalLibrary.h>
#include <core/io/DataStreamFactory.h>

#include <boost/lexical_cast.hpp>

#include <unordered_map>

using namespace musik::core;

#define GENRE_TRACK_COLUMN_NAME "genre"
#define GENRES_TABLE_NAME "genres"
#define GENRE_TRACK_JUNCTION_TABLE_NAME "track_genres"
#define GENRE_TRACK_FOREIGN_KEY "genre_id"

#define ARTIST_TRACK_COLUMN_NAME "artist"
#define ARTISTS_TABLE_NAME "artists"
#define ARTIST_TRACK_JUNCTION_TABLE_NAME "track_artists"
#define ARTIST_TRACK_FOREIGN_KEY "artist_id"

static std::mutex trackWriteLock;
static std::unordered_map<std::string, int64> metadataIdCache;

void IndexerTrack::ResetIdCache() {
    metadataIdCache.clear();
}

IndexerTrack::IndexerTrack(DBID id)
: internalMetadata(new IndexerTrack::MetadataWithThumbnail())
, id(id)
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

unsigned long long IndexerTrack::GetUint64(const char* key, unsigned long long defaultValue) {
    try { return std::stoull(GetValue(key)); } catch (...) { }
    return defaultValue;
}

long long IndexerTrack::GetInt64(const char* key, long long defaultValue) {
    try { return std::stoll(GetValue(key)); } catch (...) { }
    return defaultValue;
}

unsigned long IndexerTrack::GetUint32(const char* key, unsigned long defaultValue) {
    try { return std::stoul(GetValue(key)); } catch (...) { }
    return defaultValue;
}

long IndexerTrack::GetInt32(const char* key, unsigned int defaultValue) {
    try { return std::stol(GetValue(key)); } catch (...) { }
    return defaultValue;
}

double IndexerTrack::GetDouble(const char* key, double defaultValue) {
    try { return std::stod(GetValue(key)); } catch (...) { }
    return defaultValue;
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

void IndexerTrack::SetThumbnail(const char *data, long size) {
    if (this->internalMetadata->thumbnailData) {
        delete this->internalMetadata->thumbnailData;
    }

    this->internalMetadata->thumbnailData = new char[size];
    this->internalMetadata->thumbnailSize = size;

    memcpy(this->internalMetadata->thumbnailData, data, size);
}

std::string IndexerTrack::Uri() {
    return this->GetValue("filename");
}

int IndexerTrack::GetValue(const char* key, char* dst, int size) {
    return CopyString(this->GetValue(key), dst, size);
}

int IndexerTrack::Uri(char* dst, int size) {
    return CopyString(this->Uri(), dst, size);
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

unsigned long long IndexerTrack::GetId() {
    return this->id;
}

bool IndexerTrack::NeedsToBeIndexed(
    const boost::filesystem::path &file,
    db::Connection &dbConnection)
{
    try {
        this->SetValue("path", file.string().c_str());
        this->SetValue("filename", file.string().c_str());

        size_t lastDot = file.leaf().string().find_last_of(".");
        if (lastDot != std::string::npos) {
            this->SetValue("extension", file.leaf().string().substr(lastDot + 1).c_str());
        }

        DBID fileSize = (DBID)boost::filesystem::file_size(file);
        DBTIME fileTime = (DBTIME)boost::filesystem::last_write_time(file);

        this->SetValue("filesize", boost::lexical_cast<std::string>(fileSize).c_str());
        this->SetValue("filetime", boost::lexical_cast<std::string>(fileTime).c_str());

        db::Statement stmt(
            "SELECT id, filename, filesize, filetime " \
            "FROM tracks t " \
            "WHERE filename=?", dbConnection);

        stmt.BindText(0, this->GetValue("filename"));

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
    catch (...) {
    }

    return true;
}

static DBID writeToTracksTable(
    db::Connection &dbConnection,
    IndexerTrack& track)
{
    /* if there's no ID specified, but we have an external ID, let's
    see if we can find the corresponding ID. this can happen when
    IInputSource plugins are reading/writing track data. */
    if (track.GetId() == 0) {
        std::string externalId = track.GetValue("external_id");
        int sourceId = track.GetInt32("source_id", 0);

        if (externalId.size() && sourceId != 0) {
            db::Statement stmt("SELECT id FROM tracks WHERE source_id=? AND external_id=?", dbConnection);
            stmt.BindInt(0, sourceId);
            stmt.BindText(1, externalId);
            if (stmt.Step() == db::Row) {
                track.SetId(stmt.ColumnInt64(0));
            }
        }
    }

    db::Statement stmt("INSERT OR REPLACE INTO tracks " \
        "(id, track, disc, bpm, duration, filesize, year, title, filename, filetime, path_id) " \
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", dbConnection);

    stmt.BindText(1, track.GetValue("track"));
    stmt.BindText(2, track.GetValue("disc"));
    stmt.BindText(3, track.GetValue("bpm"));
    stmt.BindText(4, track.GetValue("duration"));
    stmt.BindText(5, track.GetValue("filesize"));
    stmt.BindText(6, track.GetValue("year"));
    stmt.BindText(7, track.GetValue("title"));
    stmt.BindText(8, track.GetValue("filename"));
    stmt.BindText(9, track.GetValue("filetime"));
    stmt.BindText(10, track.GetValue("path_id"));

    if (track.GetId() != 0) {
        stmt.BindInt(0, (uint64) track.GetId());
    }

    if (stmt.Step() == db::Done) {
        if (track.GetId() == 0) {
            return dbConnection.LastInsertedId();
        }
    }

    return track.GetId();
}

static void removeRelation(
    db::Connection& connection,
    const std::string& field,
    DBID trackId)
{
    std::string query = boost::str(boost::format("DELETE FROM %1% WHERE track_id=?") % field);
    db::Statement stmt(query.c_str(), connection);
    stmt.BindInt(0, trackId);
    stmt.Step();
}

static void removeKnownFields(Track::MetadataMap& metadata) {
    metadata.erase("track");
    metadata.erase("disc");
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

DBID IndexerTrack::SaveThumbnail(db::Connection& connection, const std::string& libraryDirectory) {
    DBID thumbnailId = 0;

    if (this->internalMetadata->thumbnailData) {
        uint64 sum = Checksum(this->internalMetadata->thumbnailData, this->internalMetadata->thumbnailSize);

        db::Statement thumbs("SELECT id FROM thumbnails WHERE filesize=? AND checksum=?", connection);
        thumbs.BindInt(0, this->internalMetadata->thumbnailSize);
        thumbs.BindInt(1, sum);

        if (thumbs.Step() == db::Row) {
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
                FILE *thumbFile = _wfopen(wfilename.c_str(), L"wb");
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

    db::Statement selectMetaKey("SELECT id FROM meta_keys WHERE name=?", connection);
    db::Statement selectMetaValue("SELECT id FROM meta_values WHERE meta_key_id=? AND content=?", connection);
    db::Statement insertMetaValue("INSERT INTO meta_values (meta_key_id,content) VALUES (?,?)", connection);
    db::Statement insertTrackMeta("INSERT INTO track_meta (track_id,meta_value_id) VALUES (?,?)", connection);
    db::Statement insertMetaKey("INSERT INTO meta_keys (name) VALUES (?)", connection);

    MetadataMap::const_iterator it = unknownFields.begin();
    for ( ; it != unknownFields.end(); ++it){
        DBID keyId = 0;
        std::string key;

        /* lookup the ID for the key; insert if it doesn't exist.. */
        if (metadataIdCache.find("metaKey-" + it->first) != metadataIdCache.end()) {
            keyId = metadataIdCache["metaKey-" + it->first];
        }
        else {
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

            metadataIdCache["metaKey-" + it->first] = keyId;
            selectMetaKey.Reset();
        }

        if (keyId == 0) {
            continue; /* welp... */
        }

        /* see if we already have the value as a normalized row in our table.
        if we don't, insert it. */

        DBID valueId = 0;

        if (metadataIdCache.find("metaValue-" + it->second) != metadataIdCache.end()) {
            valueId = metadataIdCache["metaValue-" + it->second];
        }
        else {
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

            metadataIdCache["metaValue-" + it->second] = valueId;
            selectMetaValue.Reset();
        }

        /* now that we have a keyId and a valueId, create the relationship */

        if (valueId != 0) {
            insertTrackMeta.BindInt(0, this->id);
            insertTrackMeta.BindInt(1, valueId);
            insertTrackMeta.Step();
            insertTrackMeta.Reset();
        }
    }
}

DBID IndexerTrack::SaveSingleValueField(
    db::Connection& dbConnection,
    const std::string& trackMetadataKeyName,
    const std::string& fieldTableName)
{
    DBID id = 0;

    std::string selectQuery = boost::str(boost::format(
        "SELECT id FROM %1% WHERE name=?") % fieldTableName);

    db::Statement stmt(selectQuery.c_str(), dbConnection);
    std::string value = this->GetValue(trackMetadataKeyName.c_str());

    if (metadataIdCache.find(fieldTableName + "-" + value) != metadataIdCache.end()) {
        id = metadataIdCache[fieldTableName + "-" + value];
    }
    else {
        stmt.BindText(0, value);
        if (stmt.Step() == db::Row) {
            id = stmt.ColumnInt(0);
        }
        else {
            std::string insertStatement = boost::str(boost::format(
                "INSERT INTO %1% (name) VALUES (?)") % fieldTableName);

            db::Statement insertValue(insertStatement.c_str(), dbConnection);
            insertValue.BindText(0, value);

            if (insertValue.Step() == db::Done) {
                id = dbConnection.LastInsertedId();
            }
        }

        metadataIdCache[fieldTableName + "-" + value] = id;
    }

    return id;
}

DBID IndexerTrack::SaveMultiValueField(
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

    MetadataIteratorRange values = this->GetValues(tracksTableColumnName.c_str());
    while (values.first != values.second) {
        if (processed.find(values.first->second) == processed.end()) {
            processed.insert(values.first->second);

            std::string value = values.first->second;

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

        ++values.first;
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

DBID IndexerTrack::SaveGenre(db::Connection& dbConnection) {
    return this->SaveMultiValueField(
        dbConnection,
        GENRE_TRACK_COLUMN_NAME,
        GENRES_TABLE_NAME,
        GENRE_TRACK_JUNCTION_TABLE_NAME,
        GENRE_TRACK_FOREIGN_KEY);
}

DBID IndexerTrack::SaveArtist(db::Connection& dbConnection) {
    return this->SaveMultiValueField(
        dbConnection,
        ARTIST_TRACK_COLUMN_NAME,
        ARTISTS_TABLE_NAME,
        ARTIST_TRACK_JUNCTION_TABLE_NAME,
        ARTIST_TRACK_FOREIGN_KEY);
}

bool IndexerTrack::Save(db::Connection &dbConnection, std::string libraryDirectory) {
    std::unique_lock<std::mutex> lock(trackWriteLock);

    if (this->GetValue("album_artist") == "") {
        this->SetValue("album_artist", this->GetValue("artist").c_str());
    }

    /* remove existing relations -- we're going to update them with fresh data */

    if (this->id != 0) {
        removeRelation(dbConnection, "track_genres", this->id);
        removeRelation(dbConnection, "track_artists", this->id);
        removeRelation(dbConnection, "track_meta", this->id);
    }

    /* write generic info to the tracks table */

    this->id = writeToTracksTable(dbConnection, *this);

    DBID albumId = this->SaveSingleValueField(dbConnection, "album", "albums");
    DBID genreId = this->SaveGenre(dbConnection);
    DBID artistId = this->SaveArtist(dbConnection);
    DBID albumArtistId = this->SaveSingleValueField(dbConnection, "album_artist", "artists");
    DBID thumbnailId = this->SaveThumbnail(dbConnection, libraryDirectory);

    /* ensure we have a correct source id */
    std::string externalId = this->GetValue("external_id");
    int sourceId = 0;

    try {
        std::string source = this->GetValue("source_id");
        if (source.size()) {
            sourceId = std::stoi(source.c_str());
        }
    }
    catch (...) {
        /* shouldn't happen... */
    }

    /* update all of the track foreign keys */

    {
        db::Statement stmt(
            "UPDATE tracks " \
            "SET album_id=?, visual_genre_id=?, visual_artist_id=?, album_artist_id=?, thumbnail_id=?, source_id=?, external_id=? " \
            "WHERE id=?", dbConnection);

        stmt.BindInt(0, albumId);
        stmt.BindInt(1, genreId);
        stmt.BindInt(2, artistId);
        stmt.BindInt(3, albumArtistId);
        stmt.BindInt(4, thumbnailId);
        stmt.BindInt(5, sourceId);

        if (externalId.size()) {
            stmt.BindText(6, externalId);
        }
        else {
            stmt.BindNull(6);
        }

        stmt.BindInt(7, this->id);
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
        if (metadataIdCache.find(tableName + "-" + fieldValue) != metadataIdCache.end()) {
            fieldId = metadataIdCache[tableName + "-" + fieldValue];
        }
        else {
            std::string query = boost::str(boost::format("SELECT id FROM %1% WHERE name=?") % tableName);
            db::Statement stmt(query.c_str(), dbConnection);
            stmt.BindText(0, fieldValue);

            if (stmt.Step() == db::Row) {
                fieldId = stmt.ColumnInt(0);
                metadataIdCache[tableName + "-" + fieldValue] = fieldId;
            }
        }
    }

    /* not found? insert. */

    if (fieldId == 0) {
        std::string query = boost::str(boost::format(
            "INSERT INTO %1% (name, aggregated) VALUES (?, ?)") % tableName);

        db::Statement stmt(query.c_str(), dbConnection);
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

IndexerTrack::MetadataWithThumbnail::MetadataWithThumbnail()
: thumbnailData(NULL)
, thumbnailSize(0) {
}

IndexerTrack::MetadataWithThumbnail::~MetadataWithThumbnail() {
    delete this->thumbnailData;
}
