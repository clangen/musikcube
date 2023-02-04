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

#include "pch.hpp"
#include "MarkTrackPlayedQuery.h"

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::sdk;

const std::string MarkTrackPlayedQuery::kQueryName = "MarkTrackPlayedQuery";

MarkTrackPlayedQuery::MarkTrackPlayedQuery(const int64_t trackId) noexcept {
    this->trackId = trackId;
}

bool MarkTrackPlayedQuery::OnRun(musik::core::db::Connection &db) {
    Statement stmt(
        "UPDATE tracks "
        "SET play_count=(play_count+1), last_played=julianday('now') "
        "WHERE id=?",
        db);

    stmt.BindInt64(0, this->trackId);

    this->result = stmt.Step() == db::Done;
    return this->result;
}

/* ISerializableQuery */

std::string MarkTrackPlayedQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "trackId", this->trackId },
        }}
    };
    return output.dump();
}

std::string MarkTrackPlayedQuery::SerializeResult() {
    nlohmann::json output = { { "result", this->result } };
    return output.dump();
}

void MarkTrackPlayedQuery::DeserializeResult(const std::string& data) {
    auto input = nlohmann::json::parse(data);
    this->SetStatus(input["result"].get<bool>() == true
        ? IQuery::Finished : IQuery::Failed);
}

std::shared_ptr<MarkTrackPlayedQuery> MarkTrackPlayedQuery::DeserializeQuery(const std::string& data) {
    auto options = nlohmann::json::parse(data)["options"];
    return std::make_shared<MarkTrackPlayedQuery>(options["trackId"].get<int64_t>());
}