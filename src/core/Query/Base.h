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
#include <core/db/Connection.h>
#include <core/db/Statement.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>

namespace musik{ namespace core{
    namespace Library{
        class Base;
        class LocalDB;
    }
} }


#include <string>

namespace musik{ namespace core{
    namespace Query{
        class Base;
        typedef boost::shared_ptr<Query::Base> Ptr;

        enum Options:unsigned int{
            AutoCallback    = 1,
            Wait            = 2,
            Prioritize      = 4,
            CancelQueue     = 8,
            CancelSimilar   = 16,
            UnCanceable     = 32
        };

        //////////////////////////////////////////
        ///\brief
        ///Interface class for all queries.
        //////////////////////////////////////////
        class Base{
            public:
                Base(void);
                virtual ~Base(void);

            protected:
                friend class Library::Base;
                friend class Library::LocalDB;

                // Variables:

                //////////////////////////////////////////
                ///\brief
                ///The query id is a unique number for each query.
                ///
                ///Used for comparing queries and find similar queries.
                //////////////////////////////////////////
                unsigned int iQueryId;

                //////////////////////////////////////////
                ///\brief
                ///Current state of the query
                ///
                ///\remarks
                ///iState is protected by the Library::Base::oResultMutex
                ///
                ///\see
                ///musik::core::State
                //////////////////////////////////////////
                enum Status:int{
			        Started       = 1,
			        Ended         = 2,
			        Canceled      = 4,
			        OutputStarted = 8,
			        OutputEnded   = 16,
			        Finished      = 32
                };

                unsigned int status;

                //////////////////////////////////////////
                ///\brief
                ///Query options
                ///
                ///options is only set inside the AddQuery and should not be altered later.
                ///It is therefore considered threadsafe.
                ///
                ///\see
                ///musik::core::Library::Base::AddQuery
                //////////////////////////////////////////
                unsigned int options;

            protected:
                friend class Library::Base;
                friend class Library::LocalDB;

                // Methods:

                //////////////////////////////////////////
                ///\brief
                ///Copy method that is required to be implemented.
                ///
                ///\returns
                ///Shared pointer to Query::Base object.
                ///
                ///This method is required by all queries since they are
                ///copied every time a Library::Base::AddQuery is called.
                ///
                ///\see
                ///Library::Base::AddQuery
                //////////////////////////////////////////
                virtual Ptr copy() const=0;

                //////////////////////////////////////////
                ///\brief
                ///Method for calling all the querys signals
                ///
                ///\param oLibrary
                ///Pointer to the library running the query
                ///
                ///\returns
                ///Should return true if query is totaly finished. false otherwise.
                //////////////////////////////////////////
                virtual bool RunCallbacks(Library::Base *oLibrary){return true;};

                //////////////////////////////////////////
                ///\brief
                ///Method that do the acctual work.
                ///
                ///\param oLibrary
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
                virtual bool ParseQuery(Library::Base *oLibrary,db::Connection &db)=0;

                //////////////////////////////////////////
                ///\brief
                ///PreAddQuery is called from the Library::Base::AddQuery when the copied query is added to the library.
                ///
                ///\param library
                ///Pointer to library
                ///
                ///\see
                ///Library::Base::AddQuery
                //////////////////////////////////////////
                virtual void PreAddQuery(Library::Base *library){};
        };



    }
} }
