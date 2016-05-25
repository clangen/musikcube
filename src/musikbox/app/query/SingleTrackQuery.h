#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include "CategoryListViewQuery.h"

namespace musik {
    namespace box {
        class SingleTrackQuery : public musik::core::query::QueryBase {
            public:
                SingleTrackQuery(const std::string& path);
                virtual ~SingleTrackQuery();

                virtual std::string Name() { return "SingleTrackQuery"; }
                virtual musik::core::TrackPtr GetResult();

            protected:
                virtual bool OnRun(musik::core::db::Connection &db);

            private:
                musik::core::TrackPtr result;
                std::string filename;
        };
    }
}
