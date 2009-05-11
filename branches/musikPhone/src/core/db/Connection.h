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
#include <core/db/dbconfig.h>
#include <core/db/Statement.h>
#include <core/db/ScopedTransaction.h>

#include <map>
#include <boost/utility.hpp>
#include <boost/thread/mutex.hpp>

//////////////////////////////////////////
// Forward declare
struct sqlite3;
struct sqlite3_stmt;
//////////////////////////////////////////


namespace musik{ namespace core{ namespace db{

    //////////////////////////////////////////
    ///\brief
    ///Database Wrapper
    ///
    ///A Connection to the database
    //////////////////////////////////////////
    class Connection : boost::noncopyable{
        public: 
            Connection();
            ~Connection();
            int Open(const utfchar *database,unsigned int options=0,unsigned int cache=0);
            int Open(const utfstring &database,unsigned int options=0,unsigned int cache=0);
            int Close();
            int Execute(const char* sql);
            int Execute(const wchar_t* sql);
            int LastInsertedId();

            void Interrupt();
            void Analyze();

        private:

            void Initialize(unsigned int cache);

            typedef std::map<std::string,sqlite3_stmt*> StatementCache;
            StatementCache cachedStatements;

            friend class Statement;
            friend class CachedStatement;
            friend class ScopedTransaction;

            sqlite3_stmt *GetCachedStatement(const char* sql);
            void ReturnCachedStatement(const char* sql,sqlite3_stmt *stmt);

            int StepStatement(sqlite3_stmt *stmt);

            int transactionCounter;            

            sqlite3 *connection;

            boost::mutex mutex;
            static boost::mutex globalMutex;

            void Maintenance(bool init);

    };


} } }

