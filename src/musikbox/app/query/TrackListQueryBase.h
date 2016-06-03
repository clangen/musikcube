#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include "CategoryListViewQuery.h"

namespace musik {
    namespace box {
        class TrackListQueryBase : public musik::core::query::QueryBase {
            public:
                typedef std::shared_ptr<
                    std::vector<musik::core::TrackPtr> > Result;

                typedef std::shared_ptr<std::set<size_t> > Headers;

                virtual ~TrackListQueryBase() { };
                virtual std::string Name() = 0;
                virtual Result GetResult() = 0;
                virtual Headers GetHeaders() = 0;
                virtual size_t GetQueryHash() = 0;
        };
    }
}
