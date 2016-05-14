#pragma once

#include <string>
#include <vector>
#include <sigslot/sigslot.h>

#include <boost/shared_ptr.hpp>
#include <core/library/IIndexer.h>
#include <core/db/Connection.h>

namespace musik { namespace core {
    
    class IQuery;
    typedef std::shared_ptr<IQuery> QueryPtr;

    class IQuery {
        public:
            sigslot::signal2<int, QueryPtr> QueryCompleted;

            typedef enum {
                Idle = 1,
                Running = 2,
                Failed = 3,
                Finished = 4,
            } Status;

            virtual bool Run(db::Connection &db) = 0;
            virtual int GetStatus() = 0;
            virtual int GetId() = 0;
            virtual int GetOptions() = 0;
            virtual std::string Name() = 0;
    };

} }