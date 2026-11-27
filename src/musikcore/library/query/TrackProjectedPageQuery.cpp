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
#include "TrackProjectedPageQuery.h"
#include <musikcore/library/query/util/Serialization.h>
#include <algorithm>
#include <stdexcept>
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <nlohmann/json.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

using namespace musik::core;
using namespace musik::core::db;
using namespace musik::core::library;
using namespace musik::core::library::query;
using namespace musik::core::library::query::serialization;

const std::string TrackProjectedPageQuery::kQueryName = "TrackPageQuery";

TrackProjectedPageQuery::TrackProjectedPageQuery(LibraryBackend::Shared library, const TrackPageQuery& query)
: filter(query.search)
, fields(query.fields)
, matchMode(query.matchMode)
, offset(query.offset)
, limit(query.limit)
, selections(query.selections) {
    this->scope = query.scope;
}

bool TrackProjectedPageQuery::OnRun(Connection& db) {
    TrackPageQuery query(this->filter, this->fields, this->offset, this->limit);
    query.matchMode = this->matchMode;
    query.selections = this->selections;
    query.scope = this->scope;
    query.includeTotal = true;
    return query.Run(db.NativeHandle(), this->page);
}
std::string TrackProjectedPageQuery::SerializeQuery() {
    // clang-format off
    nlohmann::json output = {
        { "name", kQueryName },
        { "options", {
            { "filter", this->filter },
            { "regex", this->matchMode == TrackMatchMode::Regex },
            { "selections", EncodeSelections(this->selections) },
            { "fields", this->fields },
            { "offset", this->offset },
            { "limit", this->limit },
        } },
    };
    // clang-format on
    output["options"]["scope"] = EncodeScope(this->scope);
    return output.dump();
}
std::string TrackProjectedPageQuery::SerializeResult() {
    nlohmann::json rows = nlohmann::json::array();
    for (const auto& row : this->page.rows) {
        rows.push_back({ { "id", row.id }, { "values", row.fields } });
    }
    // clang-format off
    nlohmann::json output = {
        { "projectedPage", {
            { "fields", this->page.names },
            { "rows", rows },
            { "total", this->page.total },
        } },
    };
    // clang-format on
    return output.dump();
}
void TrackProjectedPageQuery::DeserializeResult(const std::string& data) {
    this->SetStatus(IQuery::Failed);
    const auto projected = nlohmann::json::parse(data).at("projectedPage");
    ProjectedTrackPage page;
    page.names = projected.at("fields").get<std::vector<std::string>>();
    if (page.names != this->fields || projected.at("rows").size() > this->limit) {
        throw std::invalid_argument("Invalid projected fields");
    }
    for (const auto& row : projected.at("rows")) {
        ProjectedTrackPage::Row value {
            .id = row.at("id").get<int64_t>(),
            .fields = row.at("values").get<std::vector<std::vector<std::string>>>(),
        };
        if (value.id <= 0 || value.fields.size() != this->fields.size()) {
            throw std::invalid_argument("Invalid projected row");
        }
        page.rows.push_back(std::move(value));
    }
    std::sort(page.rows.begin(), page.rows.end(), [](const auto& a, const auto& b) {
        return a.id < b.id;
    });
    for (size_t i = 1; i < page.rows.size(); ++i) {
        if (page.rows[i - 1].id == page.rows[i].id) {
            throw std::invalid_argument("Duplicate projected row");
        }
    }
    if (!projected.at("total").is_number_unsigned()) {
        throw std::invalid_argument("Invalid track page total");
    }
    page.total = projected.at("total").get<size_t>();
    const auto expected = this->offset >= page.total ? 0 : std::min(this->limit, page.total - this->offset);
    if (page.rows.size() != expected) {
        throw std::invalid_argument("Invalid track page total");
    }
    this->page = std::move(page);
    this->SetStatus(IQuery::Finished);
}
std::shared_ptr<TrackProjectedPageQuery> TrackProjectedPageQuery::DeserializeQuery(
    LibraryBackend::Shared library, const std::string& data)
{
    const auto options = nlohmann::json::parse(data).at("options");
    const auto filter = options.at("filter").get<std::string>();
    const auto fields = options.at("fields").get<std::vector<std::string>>();
    TrackPageQuery page(filter, fields, options.at("offset").get<size_t>(), options.at("limit").get<size_t>());
    page.matchMode = options.at("regex").get<bool>() ? TrackMatchMode::Regex : TrackMatchMode::Substring;
    page.selections = DecodeSelections(options.at("selections").get<std::vector<std::pair<std::string, int64_t>>>());
    page.scope = DecodeScope(options);
    return std::make_shared<TrackProjectedPageQuery>(std::move(library), page);
}
