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

/** @file Serialization.h
 *  @brief JSON serialization helpers for query results and inputs.
 *  @details Converts query-related data structures (predicate lists, metadata map
 *      lists, value lists, tracks and track lists) to and from JSON. Used by
 *      serializable queries to marshal data across the remote library connection. */

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

#include "CategoryQueryUtil.h"
#include <musikcore/library/metadata/MetadataMapList.h>
#include <musikcore/library/query/util/SdkWrappers.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/ILibrary.h>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @namespace musik::core::library::query::serialization
     *  @brief JSON (de)serialization of query data structures. */
    namespace serialization {

        /** @brief Serializes a predicate list to JSON.
         *  @param input The predicates to serialize.
         *  @return The JSON representation. */
        nlohmann::json PredicateListToJson(
            const musik::core::library::query::category::PredicateList& input);

        /** @brief Deserializes a predicate list from JSON.
         *  @param input The JSON representation.
         *  @param output The deserialized predicate list. */
        void PredicateListFromJson(
            const nlohmann::json& input,
            musik::core::library::query::category::PredicateList& output);

        /** @brief Serializes a metadata map list to JSON.
         *  @param input The map list to serialize.
         *  @return The JSON representation. */
        nlohmann::json MetadataMapListToJson(
            const musik::core::MetadataMapList& input);

        /** @brief Deserializes a metadata map list from JSON.
         *  @param input The JSON representation.
         *  @param output The deserialized map list. */
        void MetadataMapListFromJson(
            const nlohmann::json& input,
            musik::core::MetadataMapList& output);

        /** @brief Serializes an SDK value list to JSON.
         *  @param input The value list to serialize.
         *  @return The JSON representation. */
        nlohmann::json ValueListToJson(
            const musik::core::library::query::SdkValueList& input);

        /** @brief Deserializes an SDK value list from JSON.
         *  @param input The JSON representation.
         *  @param output The deserialized value list. */
        void ValueListFromJson(
            const nlohmann::json& input,
            musik::core::library::query::SdkValueList& output);

        /** @brief Serializes a track to JSON.
         *  @param input The track to serialize.
         *  @param onlyIds true to serialize only the identifying fields.
         *  @return The JSON representation. */
        nlohmann::json TrackToJson(
            const musik::core::TrackPtr input,
            bool onlyIds = false);

        /** @brief Deserializes a track from JSON.
         *  @param input The JSON representation.
         *  @param output The destination track.
         *  @param onlyIds Whether the JSON contains only identifying fields. */
        void TrackFromJson(
            const nlohmann::json& input,
            musik::core::TrackPtr output,
            bool onlyIds);

        /** @brief Serializes a track list to JSON.
         *  @param input The track list to serialize.
         *  @param onlyIds true to serialize only the identifying fields.
         *  @return The JSON representation. */
        nlohmann::json TrackListToJson(
            const musik::core::TrackList& input,
            bool onlyIds);

        /** @brief Deserializes a track list from JSON.
         *  @param input The JSON representation.
         *  @param output The destination track list.
         *  @param library Library used to resolve source info for ids.
         *  @param onlyIds Whether the JSON contains only identifying fields. */
        void TrackListFromJson(
            const nlohmann::json& input,
            musik::core::TrackList& output,
            musik::core::ILibraryPtr library,
            bool onlyIds);

        /** @brief Serializes an SDK track list to a JSON array of track ids.
         *  @param input The track list to serialize.
         *  @return The JSON array of ids. */
        nlohmann::json ITrackListToJsonIdList(
            const musik::core::sdk::ITrackList& input);

        /** @brief Fills a set from a JSON array.
         *  @tparam SetType The set container type.
         *  @tparam DataType The element type.
         *  @param input The JSON array.
         *  @param output The set to fill. */
        template <typename SetType, typename DataType>
        void JsonArrayToSet(const nlohmann::json& input, SetType& output) {
            for (auto& value : input) {
                output.insert(value.get<DataType>());
            }
        }

        /** @brief Serializes a duration map (index -> seconds) to JSON.
         *  @param input The map to serialize.
         *  @return The JSON representation. */
        nlohmann::json DurationMapToJsonMap(
            const std::map<size_t, size_t>& input);

        /** @brief Deserializes a duration map from JSON.
         *  @param input The JSON representation.
         *  @param output The deserialized map. */
        void JsonMapToDuration(
            const nlohmann::json& input,
            std::map<size_t, size_t>& output);
    }

} } } }
