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

#include "pch.hpp"
#include <core/db/Connection.h>
#include <boost/lexical_cast.hpp>
#include <sqlite/sqlite3.h>

using namespace musik::core::db;

//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Connection::Connection() : connection(NULL),transactionCounter(0) {
    this->Maintenance(true);
}


//////////////////////////////////////////
///\brief
///Destructor
///
///Will automatically close the connection if it's not closed before
//////////////////////////////////////////
Connection::~Connection(){
    this->Close();
    this->Maintenance(false);
}


//////////////////////////////////////////
///\brief
///Open a connection to the database
///
///\param database
///Connection string. In SQLite this is the filename
///
///\param options
///Bit options. Unused at the moment
///
///\param cache
///Cachesize in KB
///
///\returns
///Error code returned by SQLite
//////////////////////////////////////////
int Connection::Open(const utfchar *database,unsigned int options,unsigned int cache){
//    sqlite3_enable_shared_cache(1);

    int error;
    #ifdef UTF_WIDECHAR
        error   = sqlite3_open16(database,&this->connection);
    #else
        error   = sqlite3_open(database,&this->connection);
    #endif

    if(error==SQLITE_OK){
        this->Initialize(cache);
    }
    return error;
}

//////////////////////////////////////////
///\brief
///Open a connection to the database
///
///\param database
///Connection string. In SQLite this is the filename
///
///\param options
///Bit options. Unused at the moment
///
///\param cache
///Cachesize in KB
///
///\returns
///Error code returned by SQLite
//////////////////////////////////////////
int Connection::Open(const utfstring &database,unsigned int options,unsigned int cache){
//    sqlite3_enable_shared_cache(1);

    int error;
    #ifdef UTF_WIDECHAR
        error   = sqlite3_open16(database.c_str(),&this->connection);
    #else
        error   = sqlite3_open(database.c_str(),&this->connection);
    #endif

    if(error==SQLITE_OK){
        this->Initialize(cache);
    }
    return error;
}



//////////////////////////////////////////
///\brief
///Close connection to the database
///
///\returns
///Errorcode ( musik::core::db::ReturnCode )
//////////////////////////////////////////
int Connection::Close(){

    // Clear the cache
    for(StatementCache::iterator stmt=this->cachedStatements.begin();stmt!=this->cachedStatements.end();++stmt){
        sqlite3_finalize(stmt->second);
    }
    this->cachedStatements.clear();


    if(sqlite3_close(this->connection)==SQLITE_OK){
        this->connection    = 0;
        return musik::core::db::OK;
    }
    return musik::core::db::Error;
}


//////////////////////////////////////////
///\brief
///Execute a SQL string
///
///\param sql
///SQL to execute
///
///\returns
///Errorcode musik::core::db::ReturnCode
///
///\see
///musik::core::db::ReturnCode
//////////////////////////////////////////
int Connection::Execute(const char* sql){
    sqlite3_stmt *stmt  = NULL;

    // Prepaire seems to give errors when interrupted
    {
        boost::mutex::scoped_lock lock(this->mutex);
        if(sqlite3_prepare_v2(this->connection,sql,-1,&stmt,NULL)!=SQLITE_OK){
            sqlite3_finalize(stmt);
            return ReturnCode::Error;
        }
    }

    // Execute the statement
    int error   = sqlite3_step(stmt);
    if(error!=SQLITE_OK && error!=SQLITE_DONE){
        sqlite3_finalize(stmt);
        return ReturnCode::Error;
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return ReturnCode::OK;
}


//////////////////////////////////////////
///\brief
///Execute a SQL string
///
///\param sql
///SQL to execute
///
///\returns
///Errorcode musik::core::db::ReturnCode
///
///\see
///musik::core::db::ReturnCode
//////////////////////////////////////////
int Connection::Execute(const wchar_t* sql){
    sqlite3_stmt *stmt  = NULL;
    {
        boost::mutex::scoped_lock lock(this->mutex);
        int err = sqlite3_prepare16_v2(this->connection,sql,-1,&stmt,NULL);
        if(err!=SQLITE_OK){
            sqlite3_finalize(stmt);
            return ReturnCode::Error;
        }
    }

    // Execute the statement
    int error   = sqlite3_step(stmt);
    if(error!=SQLITE_OK && error!=SQLITE_DONE){
        sqlite3_finalize(stmt);
        return ReturnCode::Error;
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return ReturnCode::OK;
}


//////////////////////////////////////////
///\brief
///Get the last inserted row ID
///
///\returns
///Last inserted row ID
///
///\see
///http://www.sqlite.org/c3ref/last_insert_rowid.html
//////////////////////////////////////////
int Connection::LastInsertedId(){
    return sqlite3_last_insert_rowid(this->connection);
}


//////////////////////////////////////////
///\brief
///Initializes the database.
///
///\param cache
///Size of the cache to use in kilobytes
///
///This will set all the initial PRAGMAS
//////////////////////////////////////////
void Connection::Initialize(unsigned int cache){
    sqlite3_busy_timeout(this->connection,10000);

    sqlite3_exec(this->connection,"PRAGMA synchronous=OFF",NULL,NULL,NULL);         // Not a critical DB. Sync set to OFF
    sqlite3_exec(this->connection,"PRAGMA page_size=4096",NULL,NULL,NULL);          // According to windows standard page size
    sqlite3_exec(this->connection,"PRAGMA auto_vacuum=0",NULL,NULL,NULL);           // No autovaccum.

    if(cache!=0){
        // Divide by 4 to since the page_size is 4096
        // Total cache is the same as page_size*cache_size
        cache   = cache/4;

        std::string cacheSize("PRAGMA cache_size=" + boost::lexical_cast<std::string>(cache));
        sqlite3_exec(this->connection,cacheSize.c_str(),NULL,NULL,NULL);                // size * 1.5kb = 6Mb cache
    }


    sqlite3_exec(this->connection,"PRAGMA case_sensitive_like=0",NULL,NULL,NULL);   // More speed if case insensitive
    sqlite3_exec(this->connection,"PRAGMA count_changes=0",NULL,NULL,NULL);         // If set it counts changes on SQL UPDATE. More speed when not.
    sqlite3_exec(this->connection,"PRAGMA legacy_file_format=OFF",NULL,NULL,NULL);  // No reason to be backwards compatible :)
    sqlite3_exec(this->connection,"PRAGMA temp_store=MEMORY",NULL,NULL,NULL);       // MEMORY, not file. More speed.

}


//////////////////////////////////////////
///\brief
///Internal method used by the CachedStatement to locate if a statement already exists
///
///\param sql
///SQL to check for
///
///\returns
///The cached or newly created statement
///
///\see
///musik::core::db::CachedStatment
//////////////////////////////////////////
sqlite3_stmt *Connection::GetCachedStatement(const char* sql){
    sqlite3_stmt *newStmt(NULL);

    StatementCache::iterator stmt   = this->cachedStatements.find(sql);
    if(stmt==this->cachedStatements.end()){

        boost::mutex::scoped_lock lock(this->mutex);

        int err = sqlite3_prepare_v2(this->connection,sql,-1,&newStmt,NULL);
        if(err!=SQLITE_OK){
            return NULL;
        }
        return newStmt;
    }

    newStmt = stmt->second;
    this->cachedStatements.erase(stmt);
    return newStmt;
}


//////////////////////////////////////////
///\brief
///Used by CachedStatement when destructed to return it's statement.
///
///\param sql
///SQL string
///
///\param stmt
///Statement to return
///
///\see
///musik::core::db::CachedStatment
//////////////////////////////////////////
void Connection::ReturnCachedStatement(const char* sql,sqlite3_stmt *stmt){
    StatementCache::iterator cacheStmt   = this->cachedStatements.find(sql);
    if(cacheStmt==this->cachedStatements.end()){
        // Insert the stmt in cache
        this->cachedStatements[sql] = stmt;
    }else{
        // Stmt already exists. Finalize it
        DB_ASSERT(sqlite3_finalize(stmt));
    }
}

//////////////////////////////////////////
///\brief
///Interrupts the current running statement(s)
//////////////////////////////////////////
void Connection::Interrupt(){
    boost::mutex::scoped_lock lock(this->mutex);
    sqlite3_interrupt(this->connection);
}

void Connection::Maintenance(bool init){

    // Need to be locked throuout all Connections
    static boost::mutex tempMutex;
    boost::mutex::scoped_lock lock(tempMutex);

    static int counter(0);

    if(init){
        if(counter==0){
            sqlite3_initialize();
        }
        ++counter;
    }else{
        --counter;
        if(counter==0){
            sqlite3_shutdown();
        }
    }
}
