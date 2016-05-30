#pragma once

#include <string>
#include <vector>
#include <sigslot/sigslot.h>

#include <core/library/IIndexer.h>
#include <core/library/IQuery.h>

namespace musik { namespace core {

    class ILibrary;
    typedef std::shared_ptr<ILibrary> LibraryPtr;

    class ILibrary {
        public:
            sigslot::signal1<QueryPtr> QueryCompleted;

            virtual ~ILibrary() { }

            virtual int Enqueue(QueryPtr query, unsigned int options = 0) = 0;
            virtual IIndexer *Indexer() = 0;
            virtual int Id() = 0;
            virtual const std::string& Name() = 0;
    };

} }
