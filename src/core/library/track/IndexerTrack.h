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

#include <core/config.h>
#include <core/library/track/Track.h>
#include <core/library/LocalLibrary.h>

namespace musik { namespace core {

    class IndexerTrack : public Track {
        public:
            IndexerTrack(DBID id);
            virtual ~IndexerTrack(void);

            /* IWritableTrack */
            virtual void SetValue(const char* metakey, const char* value);
            virtual void ClearValue(const char* metakey);
            virtual void SetThumbnail(const char *data, long size);

            /* ITrack */
            virtual std::string GetValue(const char* metakey);
            virtual int GetValue(const char* key, char* dst, int size);
            virtual unsigned long long GetUint64(const char* key, unsigned long long defaultValue = 0ULL);
            virtual long long GetInt64(const char* key, long long defaultValue = 0LL);
            virtual unsigned long GetUint32(const char* key, unsigned long defaultValue = 0);
            virtual long GetInt32(const char* key, unsigned int defaultValue = 0);
            virtual double GetDouble(const char* key, double defaultValue = 0.0f);

            virtual std::string Uri();
            virtual int Uri(char* dst, int size);

            virtual MetadataIteratorRange GetValues(const char* metakey);
            virtual MetadataIteratorRange GetAllValues();
            virtual TrackPtr Copy();

            virtual unsigned long long GetId();
            virtual void SetId(DBID id) { this->id = id; }

            bool NeedsToBeIndexed(
                const boost::filesystem::path &file,
                db::Connection &dbConnection);

            bool Save(
                db::Connection &dbConnection,
                std::string libraryDirectory);

            static void ResetIdCache();

        private:
            DBID id;

        private:
            class MetadataWithThumbnail {
                public:
                    MetadataWithThumbnail();
                    ~MetadataWithThumbnail();

                    Track::MetadataMap metadata;
                    char *thumbnailData;
                    int thumbnailSize;
            };

            MetadataWithThumbnail *internalMetadata;

            DBID SaveThumbnail(
                db::Connection& connection,
                const std::string& libraryDirectory);

            DBID SaveGenre(db::Connection& connection);

            DBID SaveArtist(db::Connection& connection);

            DBID SaveSingleValueField(
                db::Connection& connection,
                const std::string& trackMetadataKeyName,
                const std::string& fieldTableName);

            DBID SaveMultiValueField(
                db::Connection& connection,
                const std::string& tracksTableColumnName,
                const std::string& fieldTableName,
                const std::string& junctionTableName,
                const std::string& junctionTableForeignKeyColumnName);

            DBID SaveNormalizedFieldValue(
                db::Connection& dbConnection,
                const std::string& tableName,
                const std::string& fieldValue,
                bool isAggregatedValue,
                const std::string& relationJunctionTableName = "",
                const std::string& relationJunctionTableColumn = "");

            void ProcessNonStandardMetadata(db::Connection& connection);
    };

} }
