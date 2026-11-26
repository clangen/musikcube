#pragma once
#include "LibraryQueryScope.h"
#include <nlohmann/json.hpp>

namespace musik { namespace core { namespace library { namespace query {
    inline nlohmann::json EncodeScope(const LibraryQueryScope& scope) {
        nlohmann::json value { { "sources", nullptr } };
        if (scope.SourceIds()) {
            value["sources"] = *scope.SourceIds();
        }
        return value;
    }

    inline LibraryQueryScope DecodeScope(const nlohmann::json& options) {
        if (!options.contains("scope")) {
            return {};
        }
        const auto& value = options.at("scope");
        if (!value.is_object()) {
            throw std::invalid_argument("scope must be an object");
        }
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() != "sources") {
                throw std::invalid_argument("unsupported library scope filter");
            }
        }
        if (!value.contains("sources") || value.at("sources").is_null()) {
            return {};
        }
        const auto& input = value.at("sources");
        if (!input.is_array() || input.size() > 256) {
            throw std::invalid_argument("invalid source selection size");
        }
        std::vector<int32_t> ids;
        for (const auto& entry : input) {
            if (!entry.is_number_integer()) {
                throw std::invalid_argument("source ID must be an integer");
            }
            const auto id = entry.get<int64_t>();
            if (id < 0 || id > INT32_MAX) {
                throw std::invalid_argument("invalid source ID");
            }
            ids.push_back(static_cast<int32_t>(id));
        }
        return LibraryQueryScope::ForSources(std::move(ids));
    }
} } } }
