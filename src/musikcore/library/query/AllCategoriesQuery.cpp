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

#include "AllCategoriesQuery.h"

#include <musikcore/library/query/util/Serialization.h>
#include <musikcore/db/Statement.h>

#pragma warning(push, 0)
#include <nlohmann/json.hpp>
#pragma warning(pop)

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core::db;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;

const std::string AllCategoriesQuery::kQueryName = "AllCategoriesQuery";

AllCategoriesQuery::AllCategoriesQuery() {
    this->result = std::make_shared<SdkValueList>();
}

AllCategoriesQuery::Result AllCategoriesQuery::GetResult() noexcept {
    return this->result;
}

musik::core::sdk::IValueList* AllCategoriesQuery::GetSdkResult() {
    return new SdkValueList(this->result);
}

bool AllCategoriesQuery::OnRun(Connection& db) {
    this->result = std::make_shared<SdkValueList>();
    Statement stmt("SELECT DISTINCT name FROM meta_keys ORDER BY name", db);

    this->result->Add(std::make_shared<SdkValue>("album", 0, "category"));
    this->result->Add(std::make_shared<SdkValue>("artist", 0, "category"));
    this->result->Add(std::make_shared<SdkValue>("album_artist", 0, "category"));
    this->result->Add(std::make_shared<SdkValue>("genre", 0, "category"));

    while (stmt.Step() == db::Row) {
        this->result->Add(std::make_shared<SdkValue>(
            stmt.ColumnText(0), 0, "category"
        ));
    }

    using Value = const SdkValue::Shared;
    this->result->Sort([](Value& a, Value& b) -> bool {
        return a->ToString() < b->ToString();
    });

    return true;
}

/* ISerializableQuery */

std::string AllCategoriesQuery::SerializeQuery() {
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", nlohmann::json() }
    };
    return output.dump();
}

std::string AllCategoriesQuery::SerializeResult() {
    nlohmann::json output = {
        { "result", ValueListToJson(this->result) }
    };
    return output.dump();
}

void AllCategoriesQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    auto json = nlohmann::json::parse(data);
    this->result = std::make_shared<SdkValueList>();
    ValueListFromJson(json["result"], *this->result);
    this->SetStatus(IQuery::Finished);
}

std::shared_ptr<AllCategoriesQuery> AllCategoriesQuery::DeserializeQuery(const std::string& data) {
    return std::make_shared<AllCategoriesQuery>();
}