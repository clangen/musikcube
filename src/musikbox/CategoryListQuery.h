#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>

using musik::core::query::QueryBase;
using musik::core::db::Connection;

class CategoryListQuery : public QueryBase {
    public:
        typedef boost::shared_ptr<std::vector<std::string>> Result;

        CategoryListQuery();
        ~CategoryListQuery();

        std::string Name() {
            return "CategoryListQuery";
        }

        virtual Result GetResult();

    protected:
        virtual bool OnRun(Connection &db);

        Result result;
};