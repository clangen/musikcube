#pragma once

#include <core/library/query/QueryBase.h>
#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include "CategoryListViewQuery.h"

using musik::core::query::QueryBase;
using musik::core::db::Connection;
using musik::core::TrackPtr;
using musik::core::LibraryPtr;

class SingleTrackQuery : public QueryBase {
    public:
        SingleTrackQuery(const std::string& path);
        ~SingleTrackQuery();

        virtual std::string Name() { return "SingleTrackQuery"; }
        virtual TrackPtr GetResult();

    protected:
        virtual bool OnRun(Connection &db);

    private:
        TrackPtr result;
        std::string filename;
};