//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/** @file IndexerTrack.h
 *  @brief Track implementation used while indexing files into the library.
 *  @details A writable Track backed by tag data read from a file on disk. Used by
 *      the Indexer to buffer and persist metadata (including thumbnails and
 *      ReplayGain) into the library database. */

#include <musikcore/config.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/LocalLibrary.h>

#include <filesystem>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief A Track holding metadata that has been read from a file.
     *  @details Created during indexing with just a track id; metadata is written
     *      into it through the ITagStore interface and persisted with Save(). */
    class IndexerTrack: public Track {
        public:
            /** @brief Creates an indexer track with the given database id.
             *  @param trackId The track id assigned by the database. */
            IndexerTrack(int64_t trackId);
            virtual ~IndexerTrack();

            /* ITagStore */
            /** @brief Sets a metadata value.
             *  @param metakey The metadata key.
             *  @param value The value to store. */
            void SetValue(const char* metakey, const char* value) override;
            /** @brief Removes a metadata value.
             *  @param metakey The metadata key. */
            void ClearValue(const char* metakey) override;
            /** @return true if the given key has a value.
             *  @param metakey The metadata key. */
            bool Contains(const char* metakey) override;
            /** @brief Sets the embedded thumbnail data.
             *  @param data Raw thumbnail bytes.
             *  @param size Size of the data. */
            void SetThumbnail(const char *data, long size) override;
            /** @return true if a thumbnail is present. */
            bool ContainsThumbnail() override;
            /** @brief Sets the ReplayGain profile.
             *  @param replayGain The gain values. */
            void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) override;

            /* ITrack */
            /** @return The value of the given key as a string.
             *  @param metakey The metadata key. */
            std::string GetString(const char* metakey) override;
            /** @brief Copies a string value into the destination buffer.
             *  @param key The metadata key.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst. */
            int GetString(const char* key, char* dst, int size) override;
            /** @return The 64-bit integer value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            long long GetInt64(const char* key, long long defaultValue = 0LL) override;
            /** @return The 32-bit integer value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            int GetInt32(const char* key, unsigned int defaultValue = 0) override;
            /** @return The double value of the key.
             *  @param key The metadata key.
             *  @param defaultValue Value if the key is missing. */
            double GetDouble(const char* key, double defaultValue = 0.0f) override;
            /** @return The URI of the track. */
            std::string Uri() override;
            /** @brief Copies the URI into the destination buffer.
             *  @param dst Destination buffer.
             *  @param size Capacity of dst. */
            int Uri(char* dst, int size) override;
            /** @return The stored ReplayGain profile. */
            musik::core::sdk::ReplayGain GetReplayGain() override;
            /** @return The current metadata state. */
            musik::core::sdk::MetadataState GetMetadataState() override;

            /** @return An iterator range over the values of the given key.
             *  @param metakey The metadata key. */
            MetadataIteratorRange GetValues(const char* metakey) override;
            /** @return An iterator range over all metadata. */
            MetadataIteratorRange GetAllValues() override;
            /** @return A deep copy of this track. */
            TrackPtr Copy() override;
            /** @return The track id. */
            int64_t GetId() override;
            /** @brief Sets the track id.
             *  @param trackId The new id. */
            void SetId(int64_t trackId) noexcept override { this->trackId = trackId; }
            /** @brief Sets the metadata state.
             *  @param state The new state. */
            void SetMetadataState(musik::core::sdk::MetadataState state)  override;

            /** @brief Determines whether a file needs to be (re)indexed.
             *  @param file The file on disk.
             *  @param dbConnection The database connection.
             *  @return true if the file is new or modified since last index. */
            bool NeedsToBeIndexed(
                const std::filesystem::path &file,
                db::Connection &dbConnection);

            /** @brief Persists this track's metadata into the database.
             *  @param dbConnection The database connection.
             *  @param libraryDirectory The library directory.
             *  @return true on success. */
            bool Save(
                db::Connection &dbConnection,
                std::string libraryDirectory);

            /** @brief Runs pre-indexing database preparation.
             *  @param dbConnection The database connection. */
            static void OnIndexerStarted(db::Connection &dbConnection);
            /** @brief Runs post-indexing database finalization.
             *  @param dbConnection The database connection. */
            static void OnIndexerFinished(db::Connection &dbConnection);

        protected:
            friend class Indexer;
            static std::mutex sharedWriteMutex; /**< Serializes concurrent track writes. */

        private:
            int64_t trackId; /**< Database track id. */

        private:
            /** @brief Buffered tag data for one track. */
            class InternalMetadata {
                public:
                    InternalMetadata();
                    ~InternalMetadata();

                    Track::MetadataMap metadata; /**< Key/value metadata. */
                    std::shared_ptr<musik::core::sdk::ReplayGain> replayGain; /**< ReplayGain profile. */
                    char *thumbnailData; /**< Raw thumbnail bytes. */
                    int thumbnailSize;   /**< Thumbnail size, in bytes. */
            };

            InternalMetadata *internalMetadata; /**< Buffered tag data. */

            /** @brief Saves the thumbnail and returns its id.
             *  @param connection The database connection.
             *  @param libraryDirectory The library directory.
             *  @return The thumbnail id, or -1. */
            int64_t SaveThumbnail(
                db::Connection& connection,
                const std::string& libraryDirectory);

            /** @return The id of the buffered thumbnail, if any. */
            int64_t GetThumbnailId();

            /** @brief Saves the genre value and returns its id.
             *  @param connection The database connection.
             *  @return The genre id. */
            int64_t SaveGenre(db::Connection& connection);

            /** @brief Saves the artist value and returns its id.
             *  @param connection The database connection.
             *  @return The artist id. */
            int64_t SaveArtist(db::Connection& connection);

            /** @brief Saves the album value and returns its id.
             *  @param connection The database connection.
             *  @param thumbnailId The album's thumbnail id.
             *  @return The album id. */
            int64_t SaveAlbum(db::Connection& connection, int64_t thumbnailId);

            /** @brief Saves a single-valued field and returns its id.
             *  @param connection The database connection.
             *  @param trackMetadataKeyName The metadata key.
             *  @param fieldTableName The target table.
             *  @return The field id. */
            int64_t SaveSingleValueField(
                db::Connection& connection,
                const std::string& trackMetadataKeyName,
                const std::string& fieldTableName);

            /** @brief Saves a multi-valued field into the junction table.
             *  @param connection The database connection.
             *  @param tracksTableColumnName The tracks column name.
             *  @param fieldTableName The field table.
             *  @param junctionTableName The junction table.
             *  @param junctionTableForeignKeyColumnName The junction foreign key column.
             *  @return The first field id. */
            int64_t SaveMultiValueField(
                db::Connection& connection,
                const std::string& tracksTableColumnName,
                const std::string& fieldTableName,
                const std::string& junctionTableName,
                const std::string& junctionTableForeignKeyColumnName);

            /** @brief Saves a normalized (dictionary) value and returns its id.
             *  @param dbConnection The database connection.
             *  @param tableName The dictionary table.
             *  @param fieldValue The value to save.
             *  @param isAggregatedValue Whether this is an aggregate row.
             *  @param relationJunctionTableName Optional junction table.
             *  @param relationJunctionTableColumn Optional junction column.
             *  @return The value id. */
            int64_t SaveNormalizedFieldValue(
                db::Connection& dbConnection,
                const std::string& tableName,
                const std::string& fieldValue,
                bool isAggregatedValue,
                const std::string& relationJunctionTableName = "",
                const std::string& relationJunctionTableColumn = "");

            /** @brief Saves the track's directory path.
             *  @param dbConnection The database connection.
             *  @param filename The track's file name. */
            void SaveDirectory(
                db::Connection& dbConnection,
                const std::string& filename);

            /** @brief Saves the ReplayGain values.
             *  @param dbConnection The database connection. */
            void SaveReplayGain(db::Connection& dbConnection);

            /** @brief Persists plugin-defined (non-standard) metadata.
             *  @param connection The database connection. */
            void ProcessNonStandardMetadata(db::Connection& connection);
    };

} }
