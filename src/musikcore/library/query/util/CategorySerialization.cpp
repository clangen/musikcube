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
#include "Serialization.h"

using namespace musik::core;
using namespace musik::core::library::query;

namespace musik { namespace core { namespace library { namespace query {
    namespace serialization {
        nlohmann::json PredicateListToJson(const category::PredicateList& input) {
            nlohmann::json result;
            for (auto& it : input) {
                result.push_back({ it.first, it.second });
            }
            return result;
        }

        void PredicateListFromJson(const nlohmann::json& input, category::PredicateList& output) {
            for (auto& it : input) {
                output.push_back({ it[0], it[1] });
            }
        }

        nlohmann::json ValueListToJson(const CategoryValues& input) {
            nlohmann::json result;
            for (const auto& value : input) {
                result.push_back({
                    { "value", value.value },
                    { "id", value.id },
                    { "type", value.type },
                    { "source_id", value.sourceId ? nlohmann::json(*value.sourceId) : nlohmann::json() },
                });
            }
            return result;
        }

        void ValueListFromJson(const nlohmann::json& input, CategoryValues& output) {
            output.clear();
            for (const auto& value : input) {
                output.push_back({
                    .value = value["value"].get<std::string>(),
                    .id = value["id"].get<int64_t>(),
                    .type = value["type"].get<std::string>(),
                    .sourceId = value.contains("source_id") && !value["source_id"].is_null()
                        ? std::optional<int32_t>(value["source_id"].get<int32_t>()) : std::nullopt,
                });
            }
        }
    }
} } } }
