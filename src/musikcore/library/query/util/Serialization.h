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

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include "CategoryQueryUtil.h"
#include <musikcore/library/query/AlbumRecord.h>
#include <musikcore/library/query/CategoryValue.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/LibraryBackend.h>

namespace musik { namespace core { namespace library { namespace query {

    namespace serialization {

        nlohmann::json PredicateListToJson(
            const musik::core::library::query::category::PredicateList& input);

        void PredicateListFromJson(
            const nlohmann::json& input,
            musik::core::library::query::category::PredicateList& output);

        nlohmann::json MetadataMapsToJson(
            const std::vector<AlbumRecord>& input);

        void MetadataMapsFromJson(
            const nlohmann::json& input,
            std::vector<AlbumRecord>& output);

        nlohmann::json ValueListToJson(
            const musik::core::library::query::CategoryValues& input);

        void ValueListFromJson(
            const nlohmann::json& input,
            musik::core::library::query::CategoryValues& output);

        nlohmann::json TrackToJson(
            const musik::core::TrackPtr input,
            bool onlyIds = false);

        void TrackFromJson(
            const nlohmann::json& input,
            musik::core::TrackPtr output,
            bool onlyIds);

        nlohmann::json TrackListToJson(
            const musik::core::TrackList& input,
            bool onlyIds);

        void TrackListFromJson(
            const nlohmann::json& input,
            musik::core::TrackList& output,
            musik::core::LibraryBackend::Shared library,
            bool onlyIds);

        nlohmann::json TrackListToJsonIdList(
            const musik::core::TrackList& input);

        template <typename SetType, typename DataType>
        void JsonArrayToSet(const nlohmann::json& input, SetType& output) {
            for (auto& value : input) {
                output.insert(value.get<DataType>());
            }
        }

        nlohmann::json DurationMapToJsonMap(
            const std::map<size_t, size_t>& input);

        void JsonMapToDuration(
            const nlohmann::json& input,
            std::map<size_t, size_t>& output);
    }

} } } }
