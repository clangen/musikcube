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
//////////////////////////////////////////////////////////////////////////////
// Forward declare
namespace musik{ namespace core{
    class MUSIK_EXPORT Indexer;

    namespace Query{
        class MUSIK_EXPORT Base;
        typedef boost::shared_ptr<musik::core::Query::Base> Ptr;
    }
    namespace Library{
        class MUSIK_EXPORT Base;
    }
	typedef boost::shared_ptr<Library::Base> LibraryPtr;
	typedef boost::weak_ptr<Library::Base> LibraryWeakPtr;
} }
//////////////////////////////////////////////////////////////////////////////

#include <core/config.h>
#include <core/db/Connection.h>
//#include <core/tracklist/IRandomAccess.h>

#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>
#include <sigslot/sigslot.h>
#include <string>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace Library{

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///This is the base class that all Libraries should extend.
///
///The Library::Base is the main interface for all libraries
///and contains the main functionallity for parsing the query queue
///and is responsible for calling callbacks in the right order.
///
///\remarks
///Library::Base is only the interface and can not be used directly.
///Library::Base is a noncopyable object.
///
///\see
///musik::core::Library::LocalDB
//////////////////////////////////////////
class MUSIK_EXPORT Base : boost::noncopyable{
    protected:
        Base(utfstring name,int id);
	public:
        virtual ~Base(void);

        //////////////////////////////////////////
        ///\brief
        ///Start up the Library thread(s)
        ///
        ///\returns
        ///True if successfully started.
        ///
        ///The Startup method will start all the librarys threads.
        //////////////////////////////////////////
        virtual bool Startup()=0;

        //////////////////////////////////////////
        ///\brief
        ///Get state of the library
        ///
        ///\returns
        ///Status of the library. May be empty.
        ///
        ///Get the current status of the Library.
        ///Will for instance report current status of the indexer in the LocalDB.
        ///
        ///\remarks
        ///Empty string means that the library thread is holding.
        //////////////////////////////////////////
        virtual utfstring GetInfo()=0;
        
        virtual bool AddQuery( const Query::Base &query,unsigned int options=0 );

        virtual bool RunCallbacks();

        utfstring GetLibraryDirectory();

        bool QueryCanceled(Query::Base *query);

        virtual musik::core::Indexer *Indexer();

        virtual utfstring BasePath();

        bool Exited();

		const utfstring& Identifier();
		int Id();
		const utfstring& Name();

//		musik::core::tracklist::Ptr NowPlaying();

        virtual const std::string& AuthorizationKey();


        static bool IsStaticMetaKey(std::string &metakey);
        static bool IsSpecialMTOMetaKey(std::string &metakey);
        static bool IsSpecialMTMMetaKey(std::string &metakey);

        static void CreateDatabase(db::Connection &db);

    protected:
        // Methods:
        virtual void Exit();

        Query::Ptr GetNextQuery();

        virtual void CancelCurrentQuery( );

        utfstring GetDBPath();

    private:
        // Methods:

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
        typedef std::list<Query::Ptr> QueryList;
        // Variables:

        //////////////////////////////////////////
        ///\brief
        ///Is the current query canceled?
        ///
        ///\see
        ///CancelCurrentQuery|QueryCanceled
        //////////////////////////////////////////
//        bool bCurrentQueryCanceled;

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
        Query::Ptr runningQuery;

        //////////////////////////////////////////
        ///\brief
        ///Identifier of the library.
        ///This is used for directory name where the library is located.
        ///
        ///\see
        ///GetLibraryDirectory
        //////////////////////////////////////////
        utfstring identifier;
        int id;

        //////////////////////////////////////////
        ///\brief
        ///Name of the library.
        //////////////////////////////////////////
        utfstring name;

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

//		musik::core::tracklist::WeakPtr nowPlaying;

    public:
        boost::mutex libraryMutex;
		LibraryWeakPtr self;

        LibraryPtr GetSelfPtr();

        int userId;

};

//////////////////////////////////////////////////////////////////////////////
} } } // musik::core::Library
//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ 
    typedef boost::shared_ptr<musik::core::Library::Base> LibraryPtr;
	typedef boost::weak_ptr<Library::Base> LibraryWeakPtr;
} }
