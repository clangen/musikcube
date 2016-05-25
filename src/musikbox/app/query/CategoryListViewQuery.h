#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <memory>

namespace musik {
    namespace box {
        class CategoryListViewQuery : public musik::core::query::QueryBase {
            public:
                struct Result {
                    std::string displayValue;
                    DBID id;
                };

                typedef std::shared_ptr<std::vector<
                    std::shared_ptr<Result>>> ResultList;

                CategoryListViewQuery(const std::string& trackField);
                virtual ~CategoryListViewQuery();

                std::string Name() { return "CategoryListViewQuery"; }

                virtual ResultList GetResult();

            protected:
                virtual bool OnRun(musik::core::db::Connection &db);

                std::string trackField;
                ResultList result;
        };
    }
}