//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/lexical_cast.hpp>

#include <core/config.h>
#include <core/db/Connection.h>

#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>
#include <sigslot/sigslot.h>
#include <string>

/* forward decl */
namespace musik { namespace core {
    class  Indexer;

    namespace query {
        class QueryBase;
        typedef boost::shared_ptr<musik::core::query::QueryBase> Ptr;
    }

    namespace library {
        class LibraryBase;
    }

    typedef boost::shared_ptr<library::LibraryBase> LibraryPtr;
    typedef boost::weak_ptr<library::LibraryBase> LibraryWeakPtr;
} }

namespace musik { namespace core { namespace library {

    class LibraryBase : boost::noncopyable {
        protected:
            LibraryBase(std::string name, int id);

        public:
            virtual ~LibraryBase();

            virtual bool Startup() = 0;

            virtual bool AddQuery(const query::QueryBase &query, unsigned int options = 0);
            virtual bool RunCallbacks();

            std::string GetLibraryDirectory();
            std::string GetDatabasePath();

            virtual musik::core::Indexer *Indexer();

            const std::string& Identifier();
            const std::string& Name();
            int Id();

            bool Exited();

            static bool IsStaticMetaKey(std::string &metakey);
            static bool IsSpecialMTOMetaKey(std::string &metakey);
            static bool IsSpecialMTMMetaKey(std::string &metakey);
            static void CreateDatabase(db::Connection &db);

        protected:
            virtual void Exit();

        private:
            typedef std::list<query::Ptr> QueryList;

            QueryList incomingQueries;
            QueryList outgoingQueries;

            std::string identifier;
            int id;
            std::string name;
            bool exit;

            boost::thread_group threads;
            boost::condition waitCondition;
            boost::mutex libraryMutex;
    };


} } }

namespace musik { namespace core { 
    typedef boost::shared_ptr<musik::core::library::LibraryBase> LibraryPtr;
} }
