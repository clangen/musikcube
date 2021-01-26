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
#include "GetPlaylistQuery.h"

#include <musikcore/library/track/LibraryTrack.h>
#include <musikcore/library/LocalLibraryConstants.h>
#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/db/Statement.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using musik::core::db::Statement;
using musik::core::db::Row;
using musik::core::TrackPtr;
using musik::core::LibraryTrack;
using musik::core::ILibraryPtr;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;

const std::string GetPlaylistQuery::kQueryName = "GetPlaylistQuery";

GetPlaylistQuery::GetPlaylistQuery(ILibraryPtr library, int64_t playlistId) {
    this->library = library;
    this->playlistId = playlistId;
    this->result = std::make_shared<TrackList>(library);
    this->headers = std::make_shared<std::set<size_t>>();
    this->hash = std::hash<int64_t>()(this->playlistId);
}

GetPlaylistQuery::Result GetPlaylistQuery::GetResult() noexcept {
    return this->result;
}

GetPlaylistQuery::Headers GetPlaylistQuery::GetHeaders() noexcept {
    return this->headers;
}

size_t GetPlaylistQuery::GetQueryHash() noexcept {
    return this->hash;
}

bool GetPlaylistQuery::OnRun(Connection& db) {
    if (result) {
        this->result = std::make_shared<TrackList>(library);
        this->headers = std::make_shared<std::set<size_t>>();
    }

    std::string query =
        "SELECT tracks.id "
        "FROM tracks, playlist_tracks "
        "WHERE tracks.external_id=track_external_id AND tracks.visible=1 AND playlist_id=? "
        "ORDER BY sort_order " +
        this->GetLimitAndOffset();

    Statement trackQuery(query.c_str(), db);
    trackQuery.BindInt64(0, this->playlistId);

    while (trackQuery.Step() == Row) {
        result->Add(trackQuery.ColumnInt64(0));
    }

    return true;
}

/* ISerializableQuery */

std::string GetPlaylistQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "playlistId", playlistId },
        }}
    };
    return FinalizeSerializedQueryWithLimitAndOffset(output);
}

std::string GetPlaylistQuery::SerializeResult() {
    return InitializeSerializedResultWithHeadersAndTrackList().dump();
}

void GetPlaylistQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    nlohmann::json result = nlohmann::json::parse(data)["result"];
    this->DeserializeTrackListAndHeaders(result, this->library, this);
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<GetPlaylistQuery> GetPlaylistQuery::DeserializeQuery(
    musik::core::ILibraryPtr library, const std::string& data)
{
    auto options = nlohmann::json::parse(data)["options"];
    auto result = std::make_shared<GetPlaylistQuery>(
        library, options["playlistId"].get<int64_t>());
    result->ExtractLimitAndOffsetFromDeserializedQuery(options);
    return result;
}