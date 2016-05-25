#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include "CategoryListViewQuery.h"

using musik::core::query::QueryBase;
using musik::core::db::Connection;
using musik::core::TrackPtr;
using musik::core::LibraryPtr;

namespace musik {
    namespace box {
        class TrackListViewQuery : public QueryBase {
            public:
                typedef std::shared_ptr<std::vector<TrackPtr>> Result;

                TrackListViewQuery(LibraryPtr library, const std::string& column, DBID id);
                virtual ~TrackListViewQuery();

                std::string Name() { return "TrackListViewQuery"; }

                virtual Result GetResult();

            protected:
                virtual bool OnRun(Connection &db);

                Result result;

            private:
                LibraryPtr library;
                std::string column;
                DBID id;
        };
    }
}
