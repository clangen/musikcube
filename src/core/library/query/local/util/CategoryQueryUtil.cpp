//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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
#include "CategoryQueryUtil.h"

#include <mutex>
#include <map>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::core::db::local;

namespace musik { namespace core { namespace db { namespace local { namespace category {

    struct Id : public Argument {
        int64_t id;
        Id(int64_t id) : id(id) { }
        virtual void Bind(Statement& stmt, int pos) const { stmt.BindInt64(pos, id); }
    };

    struct String : public Argument {
        std::string str;
        String(const std::string& str) : str(str) { }
        virtual void Bind(Statement& stmt, int pos) const { stmt.BindText(pos, str.c_str()); }
    };

    std::shared_ptr<Argument> IdArgument(int64_t id) {
        return std::make_shared<Id>(id);
    }

    std::shared_ptr<Argument> StringArgument(const std::string str) {
        return std::make_shared<String>(str);
    }

    void ReplaceAll(
        std::string& input,
        const std::string& find,
        const std::string& replace)
    {
        size_t pos = input.find(find);
        while (pos != std::string::npos) {
            input.replace(pos, find.size(), replace);
            pos = input.find(find, pos + replace.size());
        }
    }

    PropertyType GetPropertyType(const std::string& property) {
        auto found = REGULAR_PROPERTY_MAP.find(property);
        auto end = REGULAR_PROPERTY_MAP.end();

        return (found == end)
            ? PropertyType::Extended
            : PropertyType::Regular;
    }

    size_t Hash(const PredicateList& input) {
        std::string key = "";
        for (auto p : input) {
            key += p.first + std::to_string(p.second);
        }
        return std::hash<std::string>()(key);
    }

    void SplitPredicates(
        const PredicateList& input,
        PredicateList& regular,
        PredicateList& extended)
    {
        auto end = REGULAR_PROPERTY_MAP.end();

        for (auto p : input) {
            if (p.first.size() && p.second != 0 && p.second != -1) {
                if (REGULAR_PROPERTY_MAP.find(p.first) != end) {
                    regular.push_back(p);
                }
                else {
                    extended.push_back(p);
                }
            }
        }
    }

    std::string JoinRegular(
        const PredicateList& pred,
        ArgumentList& args,
        const std::string& prefix)
    {
        std::string result;
        if (pred.size()) {
            for (size_t i = 0; i < pred.size(); i++) {
                if (i > 0) { result += " AND "; }
                auto p = pred[i];
                auto str = REGULAR_PREDICATE;
                auto map = REGULAR_PROPERTY_MAP[p.first];
                ReplaceAll(str, "{{fk_id}}", map.second);
                result += str;
                args.push_back(std::make_shared<Id>(p.second));
            }

            if (prefix.size()) {
                result = prefix + result;
            }
        }
        return result;
    }

    std::string InnerJoinExtended(
        const PredicateList& pred, ArgumentList& args)
    {
        std::string result;

        std::string joined = JoinExtended(pred, args);
        if (joined.size()) {
            result = EXTENDED_INNER_JOIN;
            ReplaceAll(result, "{{extended_predicates}}", joined);
            ReplaceAll(result, "{{extended_predicate_count}}", std::to_string(pred.size()));
        }

        return result;
    }

    std::string JoinExtended(
        const PredicateList& pred, ArgumentList& args)
    {
        std::string result;
        for (size_t i = 0; i < pred.size(); i++) {
            if (i > 0) { result += " OR "; }
            result += EXTENDED_PREDICATE;
            auto p = pred[i];
            args.push_back(std::make_shared<String>(p.first));
            args.push_back(std::make_shared<Id>(p.second));
        }
        return result;
    }

    void Apply(Statement& stmt, const ArgumentList& args) {
        for (size_t i = 0; i < args.size(); i++) {
            args[i]->Bind(stmt, (int)i);
        }
    }

} } } } }
