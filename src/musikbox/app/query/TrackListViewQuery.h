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

                typedef std::shared_ptr<std::set<size_t> > Headers;

                TrackListViewQuery(
                    musik::core::LibraryPtr library,
                    const std::string& column, DBID id);

                virtual ~TrackListViewQuery();

                std::string Name() { return "TrackListViewQuery"; }

                virtual Result GetResult();
                virtual Headers GetHeaders();

            protected:
                virtual bool OnRun(musik::core::db::Connection &db);

                Result result;
                Headers headers;

            private:
                musik::core::LibraryPtr library;
                std::string column;
                DBID id;
        };
    }
}
