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

#include <core/config.h>
#include <sigslot/sigslot.h>
#include <boost/shared_ptr.hpp>

/* forward decl */
namespace musik { namespace core {
    namespace db {
        class Connection;
    }

    namespace library {
        class  Base;
        class  LocalDB;
        class  Remote;
    }
} }

namespace musik { namespace core { namespace query {

    class Base;
    typedef boost::shared_ptr<query::Base> Ptr;

    typedef enum{
        AutoCallback = 1,
        Wait = 2,
        Prioritize = 4,
        CancelQueue = 8,
        CancelSimilar = 16,
        UnCanceable = 32,
        CopyUniqueId = 64
    } Options;

    //////////////////////////////////////////
    ///\brief
    ///Interface class for all queries.
    //////////////////////////////////////////
    class  Base : public sigslot::has_slots<> {
        public:
            Base(void);
            virtual ~Base(void);

        protected:
            friend class library::Base;
            friend class library::Remote;
            friend class library::LocalDB;

            typedef enum {
                Started       = 1,
                Ended         = 2,
                Canceled      = 4,
                OutputStarted = 8,
                OutputEnded   = 16,
                Finished      = 32
            } Status;

            //////////////////////////////////////////
            ///\brief
            ///Current status of the query
            ///
            ///\remarks
            ///status is protected by the library::Base::libraryMutex
            //////////////////////////////////////////
            unsigned int status;

            //////////////////////////////////////////
            ///\brief
            ///The query id is a unique number for each query.
            ///
            ///Used for comparing queries and find similar queries.
            //////////////////////////////////////////
            unsigned int queryId;
            unsigned int uniqueId;

            //////////////////////////////////////////
            ///\brief
            ///Query options
            ///
            ///options is only set inside the AddQuery and should not be altered later.
            ///It is therefore considered threadsafe.
            ///
            ///\see
            ///musik::core::library::Base::AddQuery
            //////////////////////////////////////////
            unsigned int options;

        protected:
            virtual std::string Name();

            //////////////////////////////////////////
            ///\brief
            ///Copy method that is required to be implemented.
            ///
            ///\returns
            ///Shared pointer to query::Base object.
            ///
            ///This method is required by all queries since they are
            ///copied every time a library::Base::AddQuery is called.
            ///
            ///\see
            ///library::Base::AddQuery
            //////////////////////////////////////////
            virtual Ptr copy() const=0;

            //////////////////////////////////////////
            ///\brief
            ///Method for calling all the querys signals
            ///
            ///\param library
            ///Pointer to the library running the query
            ///
            ///\returns
            ///Should return true if query is totaly finished. false otherwise.
            //////////////////////////////////////////
            virtual bool RunCallbacks(library::Base *library){return true;};

            //////////////////////////////////////////
            ///\brief
            ///Method that do the acctual work.
            ///
            ///\param library
            ///Pointer to executing library
            ///
            ///\param oDB
            ///Pointer to DBConnection
            ///
            ///\returns
            ///true when successfully executed.
            ///
            ///The ParseQuery should consider that all sqlite
            ///calls could be interrupted by the sqlite3_interrupt call.
            //////////////////////////////////////////
            virtual bool ParseQuery(library::Base *library,db::Connection &db)=0;

            //////////////////////////////////////////
            ///\brief
            ///PreAddQuery is called from the library::Base::AddQuery when the copied query is added to the library.
            ///
            ///\param library
            ///Pointer to library
            ///
            ///\see
            ///library::Base::AddQuery
            //////////////////////////////////////////
            virtual void PreAddQuery(library::Base *library){};

        public:
            void PostCopy();

            //////////////////////////////////////////
            typedef sigslot::signal3<query::Base*,library::Base*,bool> QueryFinishedEvent;
            //////////////////////////////////////////
            ///\brief
            ///A signal called before the query is totaly removed from the library queue
            ///The bool will indicate if the query was finished successfully
            //////////////////////////////////////////
            QueryFinishedEvent QueryFinished;
    };


//////////////////////////////////////////////////////////////////////////////
} } }   // musik::core::query
//////////////////////////////////////////////////////////////////////////////
