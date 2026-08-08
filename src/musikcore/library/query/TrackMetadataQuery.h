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

/** @file TrackMetadataQuery.h
 *  @brief Query that loads metadata for a single track.
 *  @details Populates a Track from the library, either with full metadata (all
 *      columns plus ReplayGain) or with ids only (external id / source id). */

#include <musikcore/library/QueryBase.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/ILibrary.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

/** @brief Loads a single track's metadata into a Track object.
 *  @details The target Track's id/external id selects the row. Type::Full loads
 *      all metadata; Type::IdsOnly loads just the identifying fields (used for
 *      lightweight lookups). */
class TrackMetadataQuery : public QueryBase {
    public:
        static const std::string kQueryName; /**< Query type name. */

        /** @brief How much metadata to load. */
        enum class Type: int { Full = 0, IdsOnly = 1 };

        DELETE_CLASS_DEFAULTS(TrackMetadataQuery)

        /** @brief Creates a track metadata query.
         *  @param target The Track to populate (result is stored back into it).
         *  @param library The library to query.
         *  @param type Full or IdsOnly. */
        TrackMetadataQuery(
            musik::core::TrackPtr target,
            musik::core::ILibraryPtr library,
            Type type = Type::Full) noexcept;

        /** @return The populated track (same object passed to the constructor). */
        TrackPtr Result() {
            return this->result;
        }

        /* IQuery */
        /** @return The query type name. */
        std::string Name() override {
            return kQueryName;
        }

        /* ISerializableQuery */
        /** @return The serialized query parameters. */
        std::string SerializeQuery() override;
        /** @return The serialized result. */
        std::string SerializeResult() override;
        /** @brief Populates the result from serialized data.
         *  @param data The serialized result. */
        void DeserializeResult(const std::string& data) override;
        /** @brief Recreates a query from serialized parameters.
         *  @param library The library the query will run on.
         *  @param data The serialized query.
         *  @return The deserialized query. */
        static std::shared_ptr<TrackMetadataQuery> DeserializeQuery(
            musik::core::ILibraryPtr library, const std::string& data);

    protected:
        /* QueryBase */
        /** @brief Runs the query against the database.
         *  @param db The connection to run on.
         *  @return true on success. */
        bool OnRun(musik::core::db::Connection& db) override;

    private:
        Type type; /**< Full or IdsOnly. */
        musik::core::ILibraryPtr library; /**< Library to query. */
        musik::core::TrackPtr result; /**< Populated track. */
};

} } } }