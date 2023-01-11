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

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

#include "CategoryQueryUtil.h"
#include <musikcore/library/metadata/MetadataMapList.h>
#include <musikcore/library/query/util/SdkWrappers.h>
#include <musikcore/library/track/Track.h>
#include <musikcore/library/track/TrackList.h>
#include <musikcore/library/ILibrary.h>

namespace musik { namespace core { namespace library { namespace query {

    namespace serialization {

        EXPORT nlohmann::json PredicateListToJson(
            const musik::core::library::query::category::PredicateList& input);

        EXPORT void PredicateListFromJson(
            const nlohmann::json& input,
            musik::core::library::query::category::PredicateList& output);

        EXPORT nlohmann::json MetadataMapListToJson(
            const musik::core::MetadataMapList& input);

        EXPORT void MetadataMapListFromJson(
            const nlohmann::json& input,
            musik::core::MetadataMapList& output);

        EXPORT nlohmann::json ValueListToJson(
            const musik::core::library::query::SdkValueList& input);

        EXPORT void ValueListFromJson(
            const nlohmann::json& input,
            musik::core::library::query::SdkValueList& output);

        EXPORT nlohmann::json TrackToJson(
            const musik::core::TrackPtr input,
            bool onlyIds = false);

        EXPORT void TrackFromJson(
            const nlohmann::json& input,
            musik::core::TrackPtr output,
            bool onlyIds);

        EXPORT nlohmann::json TrackListToJson(
            const musik::core::TrackList& input,
            bool onlyIds);

        EXPORT void TrackListFromJson(
            const nlohmann::json& input,
            musik::core::TrackList& output,
            musik::core::ILibraryPtr library,
            bool onlyIds);

        EXPORT nlohmann::json ITrackListToJsonIdList(
            const musik::core::sdk::ITrackList& input);

        template <typename SetType, typename DataType>
        EXPORT void JsonArrayToSet(const nlohmann::json& input, SetType& output) {
            for (auto& value : input) {
                output.insert(value.get<DataType>());
            }
        }

        EXPORT nlohmann::json DurationMapToJsonMap(
            const std::map<size_t, size_t>& input);

        EXPORT void JsonMapToDuration(
            const nlohmann::json& input,
            std::map<size_t, size_t>& output);
    }

} } } }
