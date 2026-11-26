#pragma once
#include "CategoryQueryUtil.h"
#include <common/Contract.h>
#include "../LibraryQueryScope.h"

namespace musik { namespace core { namespace library { namespace query { namespace category {
    /* column names come from fixed fragments in typed queries. values stay bound. */
    inline std::string SourcePredicate(const LibraryQueryScope& scope, const char* column,
        ArgumentList& arguments, bool includeGlobal = false)
    {
        if (scope.IsUnrestricted()) {
            return {};
        }
        std::string clause = " AND (";
        if (includeGlobal) {
            clause += std::string(column) + " IS NULL OR ";
        }
        if (scope.SourceIds()->empty()) {
            musik::contract::Assert(includeGlobal, "empty track scopes must return before building SQL");
            return " AND " + std::string(column) + " IS NULL ";
        }
        clause += std::string(column) + " IN (";
        for (const auto id : *scope.SourceIds()) {
            if (clause.back() != '(') {
                clause += ",";
            }
            clause += "?";
            arguments.push_back(IdArgument(id));
        }
        return clause + ")) ";
    }
    inline void ApplySourceScope(std::string& sql, const LibraryQueryScope& scope,
        const char* column, ArgumentList& arguments)
    {
        constexpr const char* marker = "{{source_predicate}}";
        const auto position = sql.find(marker);
        musik::contract::Assert(position != std::string::npos && sql.find(marker, position + 1) == std::string::npos,
            "typed source query must contain exactly one source predicate slot");
        sql.replace(position, std::char_traits<char>::length(marker), SourcePredicate(scope, column, arguments));
    }

} } } } }
