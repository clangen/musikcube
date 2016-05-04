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

    //////////////////////////////////////////////////////////////////////////////

    //////////////////////////////////////////
    ///\brief
    ///This is the base class that all Libraries should extend.
    ///
    ///The library::LibraryBase is the main interface for all libraries
    ///and contains the main functionallity for parsing the query queue
    ///and is responsible for calling callbacks in the right order.
    ///
    ///\remarks
    ///library::LibraryBase is only the interface and can not be used directly.
    ///library::LibraryBase is a noncopyable object.
    ///
    ///\see
    ///musik::core::library::LocalLibrary
    //////////////////////////////////////////
    class LibraryBase : boost::noncopyable {
        protected:
            LibraryBase(std::string name,int id);

	    public:
            virtual ~LibraryBase();

            //////////////////////////////////////////
            ///\brief
            ///Start up the Library thread(s)
            ///
            ///\returns
            ///True if successfully started.
            ///
            ///The Startup method will start all the librarys threads.
            //////////////////////////////////////////
            virtual bool Startup() = 0;

            //////////////////////////////////////////
            ///\brief
            ///Get state of the library
            ///
            ///\returns
            ///Status of the library. May be empty.
            ///
            ///Get the current status of the Library.
            ///Will for instance report current status of the indexer in the LocalLibrary.
            ///
            ///\remarks
            ///Empty string means that the library thread is holding.
            //////////////////////////////////////////
            virtual std::string GetInfo() = 0;
        
            virtual bool AddQuery( const query::QueryBase &query,unsigned int options=0 );
            virtual bool RunCallbacks();
            std::string GetLibraryDirectory();
            bool QueryCanceled(query::QueryBase *query);
            virtual musik::core::Indexer *Indexer();
            virtual std::string BasePath();
            bool Exited();
		    const std::string& Identifier();
		    int Id();
		    const std::string& Name();
            virtual const std::string& AuthorizationKey();

            static bool IsStaticMetaKey(std::string &metakey);
            static bool IsSpecialMTOMetaKey(std::string &metakey);
            static bool IsSpecialMTMMetaKey(std::string &metakey);

            static void CreateDatabase(db::Connection &db);

        protected:
            virtual void Exit();
            virtual void CancelCurrentQuery();
            query::Ptr GetNextQuery();
            std::string GetDBPath();

        private:
            bool ClearFinishedQueries();

        public:
            // Variables:

            //////////////////////////////////////////
            ///\brief
            ///Signal called when query queue goes from empty to not empty.
            ///
            ///The OnQueryQueueStart signal will be called when the query queue is
            ///filled with queries. This signal is usefull when you are calling the
            ///RunCallbacks method from a windows timer function. Just connect a slot
            ///to this signal and start the timer when it's called. The timer can then
            ///be removed on the OnQueryQueueEnd signal is called.
            ///
            ///\see
            ///OnQueryQueueEnd
            //////////////////////////////////////////
            sigslot::signal0<> OnQueryQueueStart;

            //////////////////////////////////////////
            ///\brief
            ///Signal called when query queue goes from not empty to empty.
            ///
            ///The OnQueryQueueEnd signal will be called when the query queue is
            ///empty again and all callbacks has been called.
            ///This signal is usefull when you are calling the
            ///RunCallbacks method from a windows timer function. Just connect a slot
            ///to the OnQueryQueueStart signal and start the timer when it's called.
            ///The timer can then be removed on the OnQueryQueueEnd signal is called.
            ///
            ///\see
            ///OnQueryQueueStart
            //////////////////////////////////////////
            sigslot::signal0<> OnQueryQueueEnd;

            //////////////////////////////////////////
            ///\brief
            ///This mutex is used by the queries for protecting the outgoing results.
            ///
            ///\remarks
            ///This mutex needs to be public
            //////////////////////////////////////////
            boost::mutex resultMutex;

            //////////////////////////////////////////
            ///\brief
            ///This mutex is used by the LibraryTrack to protect the metadata map.
            ///
            ///\remarks
            ///This mutex needs to be public
            //////////////////////////////////////////
            boost::mutex trackMutex;


        protected:
            typedef std::list<query::Ptr> QueryList;

            //////////////////////////////////////////
            ///\brief
            ///queue (std::list) for incoming queries.
            //////////////////////////////////////////
            QueryList incomingQueries;

            //////////////////////////////////////////
            ///\brief
            ///queue (std::list) for finished queries that havn't
            ///been run through the callbacks yet.
            //////////////////////////////////////////
            QueryList outgoingQueries;

            //////////////////////////////////////////
            ///\brief
            ///Current running query
            //////////////////////////////////////////
            query::Ptr runningQuery;

            //////////////////////////////////////////
            ///\brief
            ///Identifier of the library.
            ///This is used for directory name where the library is located.
            ///
            ///\see
            ///GetLibraryDirectory
            //////////////////////////////////////////
            std::string identifier;
            int id;

            //////////////////////////////////////////
            ///\brief
            ///Name of the library.
            //////////////////////////////////////////
            std::string name;

            //////////////////////////////////////////
            ///\brief
            ///A thread_group of all the librarys running threads.
            ///
            ///\see
            ///ThreadHelper
            //////////////////////////////////////////
            boost::thread_group threads;

            //////////////////////////////////////////
            ///\brief
            ///Internal state of the library
            //////////////////////////////////////////
            bool queueCallbackStarted;

            bool exit;
            boost::condition waitCondition;

        public:
            boost::mutex libraryMutex;
		    LibraryWeakPtr self;
            LibraryPtr GetSelfPtr();
            int userId;
    };


} } }

namespace musik { namespace core { 
    typedef boost::shared_ptr<musik::core::library::LibraryBase> LibraryPtr;
	typedef boost::weak_ptr<library::LibraryBase> LibraryWeakPtr;
} }
