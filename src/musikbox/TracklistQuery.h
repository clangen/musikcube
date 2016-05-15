#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include "CategoryListQuery.h"

using musik::core::query::QueryBase;
using musik::core::db::Connection;
using musik::core::TrackPtr;

class TracklistQuery : public QueryBase {
    public:
        typedef std::shared_ptr<std::vector<TrackPtr>> Result;
        typedef std::shared_ptr<CategoryListQuery> Category;

        TracklistQuery();
        ~TracklistQuery();

        std::string Name() {
            return "TracklistQuery";
        }

        virtual Result GetResult();

    protected:
        virtual bool OnRun(Connection &db);

        Result result;

    private:
        std::vector<Category> categories;
        
};