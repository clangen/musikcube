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

#include <musikcore/config.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/LocalLibrary.h>

#include <filesystem>

namespace musik { namespace core {

    class IndexerTrack: public Track {
        public:
            EXPORT IndexerTrack(int64_t trackId);
            EXPORT virtual ~IndexerTrack();

            /* ITagStore */
            EXPORT void SetValue(const char* metakey, const char* value) override;
            EXPORT void ClearValue(const char* metakey) override;
            EXPORT bool Contains(const char* metakey) override;
            EXPORT void SetThumbnail(const char *data, long size) override;
            EXPORT bool ContainsThumbnail() override;
            EXPORT void SetReplayGain(const musik::core::sdk::ReplayGain& replayGain) override;

            /* ITrack */
            EXPORT std::string GetString(const char* metakey) override;
            EXPORT int GetString(const char* key, char* dst, int size) override;
            EXPORT long long GetInt64(const char* key, long long defaultValue = 0LL) override;
            EXPORT int GetInt32(const char* key, unsigned int defaultValue = 0) override;
            EXPORT double GetDouble(const char* key, double defaultValue = 0.0f) override;
            EXPORT std::string Uri() override;
            EXPORT int Uri(char* dst, int size) override;
            EXPORT musik::core::sdk::ReplayGain GetReplayGain() override;
            EXPORT musik::core::sdk::MetadataState GetMetadataState() override;

            EXPORT MetadataIteratorRange GetValues(const char* metakey) override;
            EXPORT MetadataIteratorRange GetAllValues() override;
            EXPORT TrackPtr Copy() override;
            EXPORT int64_t GetId() override;
            EXPORT void SetId(int64_t trackId) noexcept override { this->trackId = trackId; }
            EXPORT void SetMetadataState(musik::core::sdk::MetadataState state)  override;

            EXPORT bool NeedsToBeIndexed(
                const std::filesystem::path &file,
                db::Connection &dbConnection);

            EXPORT bool Save(
                db::Connection &dbConnection,
                std::string libraryDirectory);

            static EXPORT void OnIndexerStarted(db::Connection &dbConnection);
            static EXPORT void OnIndexerFinished(db::Connection &dbConnection);

        protected:
            friend class Indexer;
            static std::mutex sharedWriteMutex;

        private:
            int64_t trackId;

        private:
            class InternalMetadata {
                public:
                    EXPORT InternalMetadata();
                    EXPORT ~InternalMetadata();

                    Track::MetadataMap metadata;
                    std::shared_ptr<musik::core::sdk::ReplayGain> replayGain;
                    char *thumbnailData;
                    int thumbnailSize;
            };

            InternalMetadata *internalMetadata;

            int64_t SaveThumbnail(
                db::Connection& connection,
                const std::string& libraryDirectory);

            int64_t GetThumbnailId();

            int64_t SaveGenre(db::Connection& connection);

            int64_t SaveArtist(db::Connection& connection);

            int64_t SaveAlbum(db::Connection& connection, int64_t thumbnailId);

            int64_t SaveSingleValueField(
                db::Connection& connection,
                const std::string& trackMetadataKeyName,
                const std::string& fieldTableName);

            int64_t SaveMultiValueField(
                db::Connection& connection,
                const std::string& tracksTableColumnName,
                const std::string& fieldTableName,
                const std::string& junctionTableName,
                const std::string& junctionTableForeignKeyColumnName);

            int64_t SaveNormalizedFieldValue(
                db::Connection& dbConnection,
                const std::string& tableName,
                const std::string& fieldValue,
                bool isAggregatedValue,
                const std::string& relationJunctionTableName = "",
                const std::string& relationJunctionTableColumn = "");

            void SaveDirectory(
                db::Connection& dbConnection,
                const std::string& filename);

            void SaveReplayGain(db::Connection& dbConnection);

            void ProcessNonStandardMetadata(db::Connection& connection);
    };

} }
