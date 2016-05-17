#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <memory>

using musik::core::query::QueryBase;
using musik::core::db::Connection;

class CategoryListViewQuery : public QueryBase {
    public:
        struct Result {
            std::string displayValue;
            DBID id;
        };

        typedef std::shared_ptr<std::vector<
            std::shared_ptr<Result>>> ResultList;

        CategoryListViewQuery();
        ~CategoryListViewQuery();

        std::string Name() {
            return "CategoryListViewQuery";
        }

        virtual ResultList GetResult();

    protected:
        virtual bool OnRun(Connection &db);

        ResultList result;
};