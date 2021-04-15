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

#include "pch.hpp"
#include "LyricsQuery.h"

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::sdk;

const std::string LyricsQuery::kQueryName = "LyricsQuery";

/* IQuery */

LyricsQuery::LyricsQuery(const std::string& trackExternalId) {
    this->trackExternalId = trackExternalId;
}

std::string LyricsQuery::GetResult() {
    return this->result;
}

bool LyricsQuery::OnRun(musik::core::db::Connection &db) {
    Statement stmt(
        "SELECT value "
        "FROM extended_metadata "
        "WHERE external_id=? AND meta_key_id=("
        "  SELECT id "
        "  FROM meta_keys "
        "  WHERE name=?"
        ");",
        db);

    stmt.BindText(0, this->trackExternalId);
    stmt.BindText(1, "lyrics");

    if (stmt.Step() == db::Row) {
        this->result = stmt.ColumnText(0);
    }

    return true;
}

/* ISerializableQuery */

std::string LyricsQuery::SerializeQuery() {
    nlohmann::json query;
    query["name"] = this->Name();
    query["options"] = {{ "trackExternalId", this->trackExternalId }};
    return query.dump();
}

std::string LyricsQuery::SerializeResult() {
    nlohmann::json query;
    query["result"] = this->result;
    return query.dump();
}

void LyricsQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    this->result = nlohmann::json::parse(data).value("result", "");
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<LyricsQuery> LyricsQuery::DeserializeQuery(const std::string& data) {
    nlohmann::json query = nlohmann::json::parse(data);
    return std::make_shared<LyricsQuery>(query["options"].value("trackExternalId", ""));
}