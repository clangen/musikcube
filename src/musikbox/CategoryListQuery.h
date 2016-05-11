#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>

using musik::core::query::QueryBase;
using musik::core::db::Connection;

class CategoryListQuery : public QueryBase {
    public:
        CategoryListQuery();
        ~CategoryListQuery();

        std::string Name() {
            return "CategoryListQuery";
        }

    protected:
        virtual bool OnRun(Connection &db);
};