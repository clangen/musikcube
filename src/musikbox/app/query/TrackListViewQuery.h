#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include "CategoryListViewQuery.h"

namespace musik {
    namespace box {
        class TrackListViewQuery : public musik::core::query::QueryBase {
            public:
                typedef std::shared_ptr<
                    std::vector<musik::core::TrackPtr> > Result;

                TrackListViewQuery(
                    musik::core::LibraryPtr library,
                    const std::string& column, DBID id);

                virtual ~TrackListViewQuery();

                std::string Name() { return "TrackListViewQuery"; }

                virtual Result GetResult();

            protected:
                virtual bool OnRun(musik::core::db::Connection &db);

                Result result;

            private:
                musik::core::LibraryPtr library;
                std::string column;
                DBID id;
        };
    }
}
