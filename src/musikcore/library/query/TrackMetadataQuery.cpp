//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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
#include "TrackMetadataQuery.h"
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/query/util/TrackQueryFragments.h>
#include <musikcore/sdk/ReplayGain.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;
using namespace musik::core::sdk;

const std::string TrackMetadataQuery::kQueryName = "TrackMetadataQuery";

TrackMetadataQuery::TrackMetadataQuery(TrackPtr target, ILibraryPtr library, Type type) noexcept {
    this->result = target;
    this->library = library;
    this->type = type;
}

bool TrackMetadataQuery::OnRun(Connection& db) {
    result->SetMetadataState(MetadataState::Loading);

    const bool queryById = this->result->GetId() != 0;

    std::string query;

    if (this->type == Type::Full) {
        query = queryById
            ? tracks::kAllMetadataQueryById
            : tracks::kAllMetadataQueryByExternalId;
    }
    else {
        query = queryById
            ? tracks::kIdsOnlyQueryById
            : tracks::kIdsOnlyQueryByExternalId;
    }

    Statement trackQuery(query.c_str(), db);

    if (queryById) {
        trackQuery.BindInt64(0, this->result->GetId());
    }
    else {
        const std::string& externalId = this->result->GetString("external_id");
        if (!externalId.size()) {
            return false;
        }

        trackQuery.BindText(0, externalId);
    }

    if (trackQuery.Step() == Row) {
        if (this->type == Type::Full) {
            tracks::ParseFullTrackMetadata(result, trackQuery);
        }
        else {
            tracks::ParseIdsOnlyTrackMetadata(result, trackQuery);
        }
        result->SetMetadataState(MetadataState::Loaded);
        return true;
    }

    result->SetMetadataState(MetadataState::Missing);
    return false;
}

/* ISerializableQuery */

std::string TrackMetadataQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "type", this->type },
            { "track", TrackToJson(this->result, true) }
        }}
    };
    return output.dump();
}

std::string TrackMetadataQuery::SerializeResult() {
    nlohmann::json output = {
        { "result", TrackToJson(this->result, this->type == Type::IdsOnly) }
    };
    return output.dump();
}

void TrackMetadataQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    auto input = nlohmann::json::parse(data);
    auto parsedResult = std::make_shared<LibraryTrack>(-1LL, this->library);
    TrackFromJson(input["result"], parsedResult, false);
    this->result = parsedResult;
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<TrackMetadataQuery> TrackMetadataQuery::DeserializeQuery(
    musik::core::ILibraryPtr library, const std::string& data)
{
    auto json = nlohmann::json::parse(data);
    auto parsedTrack = std::make_shared<LibraryTrack>(-1LL, library);
    TrackFromJson(json["options"]["track"], parsedTrack, true);
    Type type = json["options"]["type"].get<Type>();
    return std::make_shared<TrackMetadataQuery>(parsedTrack, library, type);
}
