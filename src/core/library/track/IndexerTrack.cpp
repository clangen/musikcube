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

#include <core/library/track/IndexerTrack.h>

#include <core/support/Common.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <core/db/Connection.h>
#include <core/db/Statement.h>
#include <core/library/LocalLibrary.h>
#include <core/io/DataStreamFactory.h>

#include <unordered_map>

using namespace musik::core;
using namespace musik::core::sdk;

#define GENRE_TRACK_COLUMN_NAME "genre"
#define GENRES_TABLE_NAME "genres"
#define GENRE_TRACK_JUNCTION_TABLE_NAME "track_genres"
#define GENRE_TRACK_FOREIGN_KEY "genre_id"

#define ARTIST_TRACK_COLUMN_NAME "artist"
#define ARTISTS_TABLE_NAME "artists"
#define ARTIST_TRACK_JUNCTION_TABLE_NAME "track_artists"
#define ARTIST_TRACK_FOREIGN_KEY "artist_id"

std::mutex IndexerTrack::sharedWriteMutex;
static std::unordered_map<std::string, int64_t> metadataIdCache;
static std::unordered_map<int, int64_t> thumbnailIdCache; /* albumId:thumbnailId */

/* http://stackoverflow.com/a/2351171 */
static size_t hash32(const char* str) {
    unsigned int h;
    unsigned char *p;
    h = 0;
    for (p = (unsigned char*)str; *p != '\0'; p++) {
        h = 37 * h + *p;
    }
    h += (h >> 5);
    return h;
}

void IndexerTrack::OnIndexerStarted(db::Connection &dbConnection) {
    /* unused, for now */
}

void IndexerTrack::OnIndexerFinished(db::Connection &dbConnection) {
    metadataIdCache.clear();

    /* if we got some new album art, make sure all of the tracks for the
    album get the updated ID! */
    std::string query = "UPDATE tracks SET thumbnail_id=? WHERE album_id=?)";
    db::ScopedTransaction transaction(dbConnection);
    for (auto it : thumbnailIdCache) {
        db::Statement stmt(query.c_str(), dbConnection);
        stmt.BindInt64(0, it.second);
        stmt.BindInt64(1, it.first);
        stmt.Step();
    }

    thumbnailIdCache.clear();
}

IndexerTrack::IndexerTrack(int64_t trackId)
: internalMetadata(new IndexerTrack::InternalMetadata())
, trackId(trackId)
{
}

IndexerTrack::~IndexerTrack() {
    delete this->internalMetadata;
    this->internalMetadata  = nullptr;
}

std::string IndexerTrack::GetString(const char* metakey) {
    if (metakey && this->internalMetadata) {
        MetadataMap::iterator metavalue = this->internalMetadata->metadata.find(metakey);
        if (metavalue != this->internalMetadata->metadata.end()) {
            return metavalue->second;
        }
    }

    return "";
}

long long IndexerTrack::GetInt64(const char* key, long long defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stoll(GetString(key));
        }
    } catch (...) {
    }
    return defaultValue;
}

int IndexerTrack::GetInt32(const char* key, unsigned int defaultValue) {
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

double IndexerTrack::GetDouble(const char* key, double defaultValue) {
    try {
        std::string value = GetString(key);
        if (value.size()) {
            return std::stod(GetString(key));
        }
    } catch (...) {
    }
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

bool IndexerTrack::Contains(const char* metakey) {
    auto md = this->internalMetadata;
    return md && md->metadata.find(metakey) != md->metadata.end();
}

void IndexerTrack::SetThumbnail(const char *data, long size) {
    if (this->internalMetadata->thumbnailData) {
        delete[] this->internalMetadata->thumbnailData;
    }

    this->internalMetadata->thumbnailData = new char[size];
    this->internalMetadata->thumbnailSize = size;

    memcpy(this->internalMetadata->thumbnailData, data, size);
}

int64_t IndexerTrack::GetThumbnailId() {
    std::string key = this->GetString("album") + "-" + this->GetString("album_artist");
    size_t id = hash32(key.c_str());
    auto it = thumbnailIdCache.find(id);
    if (it != thumbnailIdCache.end()) {
        return it->second;
    }
    return 0;
}

bool IndexerTrack::ContainsThumbnail() {
    if (this->internalMetadata->thumbnailData &&
        this->internalMetadata->thumbnailSize)
    {
        return true;
    }
    std::unique_lock<std::mutex> lock(sharedWriteMutex);
    return this->GetThumbnailId() != 0;
}

void IndexerTrack::SetReplayGain(const ReplayGain& replayGain) {
    this->internalMetadata->replayGain.reset();
    this->internalMetadata->replayGain = std::make_shared<ReplayGain>();
    memcpy(this->internalMetadata->replayGain.get(), &replayGain, sizeof(ReplayGain));
}

std::string IndexerTrack::Uri() {
    return this->GetString("filename");
}

int IndexerTrack::GetString(const char* key, char* dst, int size) {
    return CopyString(this->GetString(key), dst, size);
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

int64_t IndexerTrack::GetId() {
    return this->trackId;
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

        size_t fileSize = (size_t) boost::filesystem::file_size(file);
        size_t fileTime = (size_t) boost::filesystem::last_write_time(file);

        this->SetValue("filesize", std::to_string(fileSize).c_str());
        this->SetValue("filetime", std::to_string(fileTime).c_str());

        db::Statement stmt(
            "SELECT id, filename, filesize, filetime " \
            "FROM tracks t " \
            "WHERE filename=?", dbConnection);

        stmt.BindText(0, this->GetString("filename"));

        bool fileDifferent = true;

        if (stmt.Step() == db::Row) {
            this->trackId = stmt.ColumnInt64(0);
            int dbFileSize = stmt.ColumnInt32(2);
            int dbFileTime = stmt.ColumnInt32(3);

            if (fileSize == dbFileSize && fileTime == dbFileTime) {
                return false;
            }
        }
    }
    catch (...) {
    }

    return true;
}

static int64_t writeToTracksTable(
    db::Connection &dbConnection,
    IndexerTrack& track)
{
    std::string externalId = track.GetString("external_id");
    int64_t id = track.GetId();

    if (externalId.size() == 0) {
        return 0;
    }

    int sourceId = track.GetInt32("source_id", 0);

    /* if there's no ID specified, but we have an external ID, let's
    see if we can find the corresponding ID. this can happen when
    IInputSource plugins are reading/writing track data. */
    if (id == 0) {
        db::Statement stmt("SELECT id FROM tracks WHERE source_id=? AND external_id=?", dbConnection);
        stmt.BindInt32(0, sourceId);
        stmt.BindText(1, externalId);
        if (stmt.Step() == db::Row) {
            id = stmt.ColumnInt64(0);
            track.SetId(id);
        }
    }

    std::string query;

    if (id != 0) {
        query =
            "UPDATE tracks "
            "SET track=?, disc=?, bpm=?, duration=?, filesize=?, "
            "    title=?, filename=?, filetime=?, path_id=?, "
            "    date_updated=julianday('now'), external_id=? "
            "WHERE id=?";
    }
    else {
        query =
            "INSERT INTO tracks "
            "(track, disc, bpm, duration, filesize, title, filename, "
            " filetime, path_id, external_id, date_added, date_updated) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, julianday('now'), julianday('now'))";
    }

    db::Statement stmt(query.c_str(), dbConnection);

    stmt.BindText(0, track.GetString("track"));
    stmt.BindText(1, track.GetString("disc"));
    stmt.BindText(2, track.GetString("bpm"));
    stmt.BindInt32(3, track.GetInt32("duration"));
    stmt.BindInt32(4, track.GetInt32("filesize"));
    stmt.BindText(5, track.GetString("title"));
    stmt.BindText(6, track.GetString("filename"));
    stmt.BindInt32(7, track.GetInt32("filetime"));
    stmt.BindInt64(8, track.GetInt64("path_id"));
    stmt.BindText(9, track.GetString("external_id"));

    if (id != 0) {
        stmt.BindInt64(10, id);
    }

    if (stmt.Step() == db::Done) {
        if (id == 0) {
            return dbConnection.LastInsertedId();
        }
    }

    return id;
}

static void removeRelation(
    db::Connection& connection,
    const std::string& field,
    int64_t trackId)
{
    std::string query = u8fmt("DELETE FROM %s WHERE track_id=?", field.c_str());
    db::Statement stmt(query.c_str(), connection);
    stmt.BindInt64(0, trackId);
    stmt.Step();
}

static void removeKnownFields(Track::MetadataMap& metadata) {
    metadata.erase("track");
    metadata.erase("disc");
    metadata.erase("bpm");
    metadata.erase("duration");
    metadata.erase("title");
    metadata.erase("filename");
    metadata.erase("filetime");
    metadata.erase("filesize");
    metadata.erase("title");
    metadata.erase("path");
    metadata.erase("extension");
    metadata.erase("genre");
    metadata.erase("artist");
    metadata.erase("album_artist");
    metadata.erase("album");
    metadata.erase("source_id");
    metadata.erase("external_id");
    metadata.erase("visible");
}

void IndexerTrack::SaveReplayGain(db::Connection& dbConnection)
{
    auto replayGain = this->internalMetadata->replayGain;
    if (replayGain) {
        {
            db::Statement removeOld("DELETE FROM replay_gain WHERE track_id=?", dbConnection);
            removeOld.BindInt64(0, this->trackId);
            removeOld.Step();
        }

        {
            if (replayGain->albumGain != 1.0 || replayGain->albumPeak != 1.0 ||
                replayGain->albumGain != 1.0 || replayGain->albumPeak != 1.0)
            {
                db::Statement insert(
                    "INSERT INTO replay_gain "
                    "(track_id, album_gain, album_peak, track_gain, track_peak) "
                    "VALUES (?, ?, ?, ?, ?);",
                    dbConnection);

                insert.BindInt64(0, this->trackId);
                insert.BindFloat(1, replayGain->albumGain);
                insert.BindFloat(2, replayGain->albumPeak);
                insert.BindFloat(3, replayGain->trackGain);
                insert.BindFloat(4, replayGain->trackPeak);

                insert.Step();
            }
        }
    }
}

int64_t IndexerTrack::SaveThumbnail(db::Connection& connection, const std::string& libraryDirectory) {
    int64_t thumbnailId = 0;

    if (this->internalMetadata->thumbnailData) {
        int64_t sum = Checksum(this->internalMetadata->thumbnailData, this->internalMetadata->thumbnailSize);

        db::Statement thumbs("SELECT id FROM thumbnails WHERE filesize=? AND checksum=?", connection);
        thumbs.BindInt32(0, this->internalMetadata->thumbnailSize);
        thumbs.BindInt64(1, sum);

        if (thumbs.Step() == db::Row) {
            thumbnailId = thumbs.ColumnInt64(0); /* thumbnail already exists */
        }

        if (thumbnailId == 0) { /* doesn't exist yet, let's insert the record and write the file */
            db::Statement insertThumb("INSERT INTO thumbnails (filesize,checksum) VALUES (?,?)", connection);
            insertThumb.BindInt32(0, this->internalMetadata->thumbnailSize);
            insertThumb.BindInt64(1, sum);

            if (insertThumb.Step() == db::Done) {
                thumbnailId = connection.LastInsertedId();

                std::string filename =
                    libraryDirectory +
                    "thumbs/" +
                    std::to_string(thumbnailId) +
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

    std::map<int64_t, std::set<int64_t>> processed;

    db::Statement selectMetaKey("SELECT id FROM meta_keys WHERE name=?", connection);
    db::Statement selectMetaValue("SELECT id FROM meta_values WHERE meta_key_id=? AND content=?", connection);
    db::Statement insertMetaValue("INSERT INTO meta_values (meta_key_id,content) VALUES (?,?)", connection);
    db::Statement insertTrackMeta("INSERT INTO track_meta (track_id,meta_value_id) VALUES (?,?)", connection);
    db::Statement insertMetaKey("INSERT INTO meta_keys (name) VALUES (?)", connection);

    MetadataMap::const_iterator it = unknownFields.begin();
    for ( ; it != unknownFields.end(); ++it){
        int64_t keyId = 0;
        std::string key;
        bool keyCached = false, valueCached = false;

        /* lookup the ID for the key; insert if it doesn't exist.. */
        if (metadataIdCache.find("metaKey-" + it->first) != metadataIdCache.end()) {
            keyId = metadataIdCache["metaKey-" + it->first];
            keyCached = true;
        }
        else {
            selectMetaKey.Reset();
            selectMetaKey.BindText(0, it->first);

            if (selectMetaKey.Step() == db::Row) {
                keyId = selectMetaKey.ColumnInt64(0);
            }
            else {
                insertMetaKey.Reset();
                insertMetaKey.BindText(0, it->first);

                if (insertMetaKey.Step() == db::Done) {
                    keyId = connection.LastInsertedId();
                }
            }

            if (keyId != 0) {
                metadataIdCache["metaKey-" + it->first] = keyId;
            }
        }

        if (keyId == 0) {
            continue; /* welp... */
        }

        /* see if we already have the value as a normalized row in our table.
        if we don't, insert it. */

        int64_t valueId = 0;

        if (metadataIdCache.find("metaValue-" + it->second) != metadataIdCache.end()) {
            valueId = metadataIdCache["metaValue-" + it->second];
            valueCached = true;
        }
        else {
            selectMetaValue.Reset();
            selectMetaValue.BindInt64(0, keyId);
            selectMetaValue.BindText(1, it->second);

            if (selectMetaValue.Step() == db::Row) {
                valueId = selectMetaValue.ColumnInt64(0);
            }
            else {
                insertMetaValue.Reset();
                insertMetaValue.BindInt64(0, keyId);
                insertMetaValue.BindText(1, it->second);

                if (insertMetaValue.Step() == db::Done) {
                    valueId = connection.LastInsertedId();
                }
            }

            if (valueId != 0) {
                metadataIdCache["metaValue-" + it->second] = valueId;
            }
        }

        /* now that we have a keyId and a valueId, create the relationship */

        if (valueId != 0 && keyId != 0) {

            /* we allow multiple values for the same key (for example, multiple composers
            for a track. but we don't allow duplicates. keep track of what keys and
            values we've already attached to this track, and dont add dupes. */

            bool process = true;
            if (processed.find(valueId) != processed.end()) {
                auto keys = processed[valueId];
                if (keys.find(keyId) != keys.end()) {
                    process = false;
                }
                else {
                    keys.insert(keyId);
                }
            }
            else {
                processed[valueId] = { keyId };
            }

            if (process) {
                insertTrackMeta.Reset();
                insertTrackMeta.BindInt64(0, this->trackId);
                insertTrackMeta.BindInt64(1, valueId);
                insertTrackMeta.Step();
            }
        }
    }
}

static std::string createTrackExternalId(IndexerTrack& track) {
    size_t hash1 = (size_t) hash32(track.GetString("filename").c_str());

    size_t hash2 = (size_t) hash32(
        (track.GetString("title") +
        track.GetString("album") +
        track.GetString("artist") +
        track.GetString("album_artist") +
        track.GetString("filesize") +
        track.GetString("duration")).c_str());

    return std::string("local-") + std::to_string(hash1) + "-" + std::to_string(hash2);
}

int64_t IndexerTrack::SaveAlbum(db::Connection& dbConnection, int64_t thumbnailId) {
    std::string album = this->GetString("album");
    std::string value = album + "-" + this->GetString("album_artist");

    /* ideally we'd use std::hash<>, but on some platforms this returns a 64-bit
    unsigned number, which cannot be easily used with sqlite3. TODO: this seems
    really strange, why don't we just cast to a signed int and be done with it?
    something to do with negative values? i can't remember now. */
    size_t albumId = hash32(value.c_str());

    std::string cacheKey = "album-" + value;
    if (metadataIdCache.find(cacheKey) != metadataIdCache.end()) {
        return metadataIdCache[cacheKey];
    }
    else {
        std::string insertStatement = "INSERT INTO albums (id, name) VALUES (?, ?)";
        db::Statement insertValue(insertStatement.c_str(), dbConnection);
        insertValue.BindInt64(0, albumId);
        insertValue.BindText(1, album);

        if (insertValue.Step() == db::Done) {
            metadataIdCache[cacheKey] = albumId;
        }
    }

    if (thumbnailId != 0) {
        db::Statement updateStatement(
            "UPDATE albums SET thumbnail_id=? WHERE id=?", dbConnection);

        updateStatement.BindInt64(0, thumbnailId);
        updateStatement.BindInt64(1, albumId);
        updateStatement.Step();

        thumbnailIdCache[albumId] = thumbnailId;
    }

    return albumId;
}

int64_t IndexerTrack::SaveSingleValueField(
    db::Connection& dbConnection,
    const std::string& trackMetadataKeyName,
    const std::string& fieldTableName)
{
    int64_t id = 0;

    std::string selectQuery = u8fmt(
        "SELECT id FROM %s WHERE name=?", fieldTableName.c_str());

    db::Statement stmt(selectQuery.c_str(), dbConnection);
    std::string value = this->GetString(trackMetadataKeyName.c_str());

    if (metadataIdCache.find(fieldTableName + "-" + value) != metadataIdCache.end()) {
        id = metadataIdCache[fieldTableName + "-" + value];
    }
    else {
        stmt.BindText(0, value);
        if (stmt.Step() == db::Row) {
            id = stmt.ColumnInt64(0);
        }
        else {
            std::string insertStatement = u8fmt(
                "INSERT INTO %s (name) VALUES (?)", fieldTableName.c_str());

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

int64_t IndexerTrack::SaveMultiValueField(
    db::Connection& connection,
    const std::string& tracksTableColumnName,
    const std::string& fieldTableName,
    const std::string& junctionTableName,
    const std::string& junctionTableForeignKeyColumnName)
{
    std::string aggregatedValue;
    int64_t fieldId = 0;
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
            connection, fieldTableName, aggregatedValue, true);
    }

    return fieldId;
}

int64_t IndexerTrack::SaveGenre(db::Connection& dbConnection) {
    return this->SaveMultiValueField(
        dbConnection,
        GENRE_TRACK_COLUMN_NAME,
        GENRES_TABLE_NAME,
        GENRE_TRACK_JUNCTION_TABLE_NAME,
        GENRE_TRACK_FOREIGN_KEY);
}

int64_t IndexerTrack::SaveArtist(db::Connection& dbConnection) {
    return this->SaveMultiValueField(
        dbConnection,
        ARTIST_TRACK_COLUMN_NAME,
        ARTISTS_TABLE_NAME,
        ARTIST_TRACK_JUNCTION_TABLE_NAME,
        ARTIST_TRACK_FOREIGN_KEY);
}

void IndexerTrack::SaveDirectory(db::Connection& db, const std::string& filename) {
    try {
        std::string dir = NormalizeDir(
            boost::filesystem::path(filename).parent_path().string());

        int64_t dirId = -1;
        if (metadataIdCache.find("directoryId-" + dir) != metadataIdCache.end()) {
            dirId = metadataIdCache["directoryId-" + dir];
        }
        else {
            db::Statement find("SELECT id FROM directories WHERE name=?", db);
            find.BindText(0, dir.c_str());
            if (find.Step() == db::Row) {
                dirId = find.ColumnInt64(0);
            }
            else {
                db::Statement insert("INSERT INTO directories (name) VALUES (?)", db);
                insert.BindText(0, dir);
                if (insert.Step() == db::Done) {
                    dirId = db.LastInsertedId();
                }
            }

            if (dirId != -1) {
                db::Statement update("UPDATE tracks SET directory_id=? WHERE id=?", db);
                update.BindInt64(0, dirId);
                update.BindInt64(1, this->trackId);
                update.Step();
            }
        }

    }
    catch (...) {
        /* not much we can do, but we don't want the app to die if we're
        unable to parse its directory. */
    }
}

bool IndexerTrack::Save(db::Connection &dbConnection, std::string libraryDirectory) {
    static bool disableAlbumArtistFallback =
        Preferences::ForComponent("settings")
            ->GetBool(prefs::keys::DisableAlbumArtistFallback, false);

    std::unique_lock<std::mutex> lock(sharedWriteMutex);

    if (!disableAlbumArtistFallback && this->GetString("album_artist") == "") {
        this->SetValue("album_artist", this->GetString("artist").c_str());
    }

    if (this->GetString("external_id") == "") {
        this->SetValue("external_id", createTrackExternalId(*this).c_str());
    }

    /* remove existing relations -- we're going to update them with fresh data */

    if (this->trackId != 0) {
        removeRelation(dbConnection, "track_genres", this->trackId);
        removeRelation(dbConnection, "track_artists", this->trackId);
        removeRelation(dbConnection, "track_meta", this->trackId);
    }

    /* write generic info to the tracks table */

    this->trackId = writeToTracksTable(dbConnection, *this);

    if (!this->trackId) {
        return false;
    }

    /* see if the metadata reader plugin extracted a thumbnail. if not, we call
    this->GetThumbnailId() to see if one already exists for the album */
    int64_t thumbnailId = this->SaveThumbnail(dbConnection, libraryDirectory);
    if (thumbnailId == 0) {
        thumbnailId = this->GetThumbnailId();
    }

    int64_t albumId = this->SaveAlbum(dbConnection, thumbnailId);
    int64_t genreId = this->SaveGenre(dbConnection);
    int64_t artistId = this->SaveArtist(dbConnection);
    int64_t albumArtistId = this->SaveSingleValueField(dbConnection, "album_artist", "artists");

    /* ensure we have a correct source id */
    int sourceId = 0;

    try {
        std::string source = this->GetString("source_id");
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
            "SET album_id=?, visual_genre_id=?, visual_artist_id=?, album_artist_id=?, thumbnail_id=?, source_id=? " \
            "WHERE id=?", dbConnection);

        stmt.BindInt64(0, albumId);
        stmt.BindInt64(1, genreId);
        stmt.BindInt64(2, artistId);
        stmt.BindInt64(3, albumArtistId);
        stmt.BindInt64(4, thumbnailId);
        stmt.BindInt64(5, sourceId);
        stmt.BindInt64(6, this->trackId);
        stmt.Step();
    }

    ProcessNonStandardMetadata(dbConnection);
    SaveDirectory(dbConnection, this->GetString("filename"));
    SaveReplayGain(dbConnection);

    return true;
}

int64_t IndexerTrack::SaveNormalizedFieldValue(
    db::Connection &dbConnection,
    const std::string& tableName,
    const std::string& fieldValue,
    bool isAggregatedValue,
    const std::string& relationJunctionTableName,
    const std::string& relationJunctionTableColumn)
{
    int64_t fieldId = 0;

    /* find by value */

    {
        if (metadataIdCache.find(tableName + "-" + fieldValue) != metadataIdCache.end()) {
            fieldId = metadataIdCache[tableName + "-" + fieldValue];
        }
        else {
            std::string query = u8fmt("SELECT id FROM %s WHERE name=?", tableName.c_str());
            db::Statement stmt(query.c_str(), dbConnection);
            stmt.BindText(0, fieldValue);

            if (stmt.Step() == db::Row) {
                fieldId = stmt.ColumnInt64(0);
                metadataIdCache[tableName + "-" + fieldValue] = fieldId;
            }
        }
    }

    /* not found? insert. */

    if (fieldId == 0) {
        std::string query = u8fmt(
            "INSERT INTO %s (name, aggregated) VALUES (?, ?)", tableName.c_str());

        db::Statement stmt(query.c_str(), dbConnection);
        stmt.BindText(0, fieldValue);
        stmt.BindInt32(1, isAggregatedValue ? 1 : 0);

        if (stmt.Step() == db::Done) {
            fieldId = dbConnection.LastInsertedId();
        }
    }

    /* if this is a normalized value it may need to be inserted into a
    junction table. see if we were asked to do this... */

    if (relationJunctionTableName.size() && relationJunctionTableColumn.size()) {
        std::string query = u8fmt(
            "INSERT INTO %s (track_id, %s) VALUES (?, ?)",
            relationJunctionTableName.c_str(), relationJunctionTableColumn.c_str());

        db::Statement stmt(query.c_str(), dbConnection);
        stmt.BindInt64(0, this->trackId);
        stmt.BindInt64(1, fieldId);
        stmt.Step();
    }

    return fieldId;
}

TrackPtr IndexerTrack::Copy() {
    return TrackPtr(new IndexerTrack(this->trackId));
}

IndexerTrack::InternalMetadata::InternalMetadata()
: thumbnailData(nullptr)
, thumbnailSize(0) {
}

IndexerTrack::InternalMetadata::~InternalMetadata() {
    delete[] this->thumbnailData;
}
