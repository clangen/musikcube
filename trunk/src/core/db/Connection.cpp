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
#include "core/db/Connection.h"
#include <boost/lexical_cast.hpp>

using namespace musik::core::db;

Connection::Connection() : connection(NULL),transactionCounter(0) {
}


Connection::~Connection(){
    this->Close();
}


int Connection::Open(const utfchar *database,unsigned int options,unsigned int cache){
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

int Connection::Open(const utfstring &database,unsigned int options,unsigned int cache){
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


int Connection::Execute(const char* sql){
    sqlite3_stmt *stmt  = NULL;
    if(sqlite3_prepare_v2(this->connection,sql,-1,&stmt,NULL)!=SQLITE_OK){
        sqlite3_finalize(stmt);
        return ReturnCode::Error;
    }

    // Execute the statement
    int error   = sqlite3_step(stmt);
    if(error==SQLITE_OK || error==SQLITE_DONE){
        sqlite3_finalize(stmt);
        return ReturnCode::Error;
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return ReturnCode::OK;
}


int Connection::Execute(const wchar_t* sql){
    sqlite3_stmt *stmt  = NULL;
    if(sqlite3_prepare16_v2(this->connection,sql,-1,&stmt,NULL)!=SQLITE_OK){
        sqlite3_finalize(stmt);
        return ReturnCode::Error;
    }

    // Execute the statement
    int error   = sqlite3_step(stmt);
    if(error==SQLITE_OK || error==SQLITE_DONE){
        sqlite3_finalize(stmt);
        return ReturnCode::Error;
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);
    return ReturnCode::OK;
}


int Connection::LastInsertedId(){
    return sqlite3_last_insert_rowid(this->connection);
}


void Connection::Initialize(unsigned int cache){
    sqlite3_busy_timeout(this->connection,10000);

    sqlite3_exec(this->connection,"PRAGMA synchronous=OFF",NULL,NULL,NULL);        // Maybe FULL if multiuser document
    sqlite3_exec(this->connection,"PRAGMA page_size=1024",NULL,NULL,NULL);            // According to windows standard page size
    sqlite3_exec(this->connection,"PRAGMA auto_vacuum=0",NULL,NULL,NULL);            // No autovaccum.

    if(cache==0){
        cache=4096;
    }
    std::string cacheSize("PRAGMA cache_size=" + boost::lexical_cast<std::string>(cache));
    sqlite3_exec(this->connection,cacheSize.c_str(),NULL,NULL,NULL);                // size * 1.5kb = 6Mb cache

    sqlite3_exec(this->connection,"PRAGMA case_sensitive_like=0",NULL,NULL,NULL);    // More speed if case insensitive
    sqlite3_exec(this->connection,"PRAGMA count_changes=0",NULL,NULL,NULL);        // If set it counts changes on SQL UPDATE. More speed when not.
    sqlite3_exec(this->connection,"PRAGMA legacy_file_format=OFF",NULL,NULL,NULL);    // Set compatible with ALL sqlite 3.x versions
    sqlite3_exec(this->connection,"PRAGMA temp_store=MEMORY",NULL,NULL,NULL);        // MEMORY, not file. More speed.

}


sqlite3_stmt *Connection::GetCachedStatement(const char* sql){
    sqlite3_stmt *newStmt(NULL);

    StatementCache::iterator stmt   = this->cachedStatements.find(sql);
    if(stmt==this->cachedStatements.end()){
        DB_ASSERT(sqlite3_prepare_v2(this->connection,sql,-1,&newStmt,NULL));
        return newStmt;
    }

    newStmt = stmt->second;
    this->cachedStatements.erase(stmt);
    return newStmt;
}


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



