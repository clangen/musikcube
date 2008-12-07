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

#include "pch.hpp"
#include <core/db/Statement.h>
#include <core/db/Connection.h>
#include <sqlite/sqlite3.h>

using namespace musik::core::db;

//////////////////////////////////////////
///\brief
///Constructor
///
///\param sql
///SQL to be precomiled
///
///\param connection
///database Connection
//////////////////////////////////////////
Statement::Statement(const char* sql,Connection &connection) : connection(&connection),stmt(NULL){
    boost::mutex::scoped_lock lock(connection.mutex);
    int err    = sqlite3_prepare_v2(this->connection->connection,sql,-1,&this->stmt,NULL);
    if(err!=SQLITE_OK){
        return;
    }
}

//////////////////////////////////////////
///\brief
///Constructor used by the CachedStatement
//////////////////////////////////////////
Statement::Statement(Connection &connection) : connection(&connection),stmt(NULL) {
}


//////////////////////////////////////////
///\brief
///Destructor that will finalize the statement
//////////////////////////////////////////
Statement::~Statement(){
    int err=sqlite3_finalize(this->stmt);
}


//////////////////////////////////////////
///\brief
///Reset a statment to be able to re-execute it later
//////////////////////////////////////////
void Statement::Reset(){
    int err    = sqlite3_reset(this->stmt);
}

//////////////////////////////////////////
///\brief
///Unbinds all previously binded parameters 
//////////////////////////////////////////
void Statement::UnBindAll(){
    DB_ASSERT(sqlite3_clear_bindings(this->stmt));
}

//////////////////////////////////////////
///\brief
///Execute/Step through the statment
///
///\returns
///musik::core::db::ReturnCode
//////////////////////////////////////////
int Statement::Step(){
    return this->connection->StepStatement(this->stmt);
}


//////////////////////////////////////////
///\brief
///Bind a integer to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindInt
///Integer to bind
//////////////////////////////////////////
void Statement::BindInt(int position,int bindInt){
    DB_ASSERT(sqlite3_bind_int(this->stmt,position+1,bindInt));
}


//////////////////////////////////////////
///\brief
///Bind a 64bit integer to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindInt
///Integer to bind
//////////////////////////////////////////
void Statement::BindInt64(int position,UINT64 bindInt){
    DB_ASSERT(sqlite3_bind_int64(this->stmt,position+1,bindInt));
}


//////////////////////////////////////////
///\brief
///Bind a text to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindText
///Text to bind
//////////////////////////////////////////
void Statement::BindText(int position,const char* bindText){
    DB_ASSERT(sqlite3_bind_text(this->stmt,position+1,bindText,-1,SQLITE_STATIC));
}

//////////////////////////////////////////
///\brief
///Bind a text to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindText
///Text to bind
//////////////////////////////////////////
void Statement::BindText(int position,const std::string &bindText){
    DB_ASSERT(sqlite3_bind_text(this->stmt,position+1,bindText.c_str(),-1,SQLITE_STATIC));
}


//////////////////////////////////////////
///\brief
///Bind a text to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindText
///Text to bind
//////////////////////////////////////////
void Statement::BindTextW(int position,const wchar_t* bindText){
    DB_ASSERT(sqlite3_bind_text16(this->stmt,position+1,bindText,-1,SQLITE_STATIC));
}

//////////////////////////////////////////
///\brief
///Bind a text to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindText
///Text to bind
//////////////////////////////////////////
void Statement::BindTextW(int position,const std::wstring &bindText){
    DB_ASSERT(sqlite3_bind_text16(this->stmt,position+1,bindText.c_str(),-1,SQLITE_STATIC));
}


//////////////////////////////////////////
///\brief
///Bind a text to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindText
///Text to bind
//////////////////////////////////////////
void Statement::BindTextUTF(int position,const utfchar* bindText){
    #ifdef UTF_WIDECHAR
        DB_ASSERT(sqlite3_bind_text16(this->stmt,position+1,bindText,-1,SQLITE_STATIC));
    #else
        DB_ASSERT(sqlite3_bind_text(this->stmt,position-1,bindText,-1,SQLITE_STATIC));
    #endif
}

//////////////////////////////////////////
///\brief
///Bind a text to a statment parameter
///
///\param position
///Position of the parameter (0 is the first)
///
///\param bindText
///Text to bind
//////////////////////////////////////////
void Statement::BindTextUTF(int position,const utfstring &bindText){
    #ifdef UTF_WIDECHAR
        DB_ASSERT(sqlite3_bind_text16(this->stmt,position+1,bindText.c_str(),-1,SQLITE_STATIC));
    #else
        DB_ASSERT(sqlite3_bind_text(this->stmt,position-1,bindText.c_str(),-1,SQLITE_STATIC));
    #endif
    const char *error = sqlite3_errmsg(this->connection->connection);


}


//////////////////////////////////////////
///\brief
///Get the results of a column if Step() return a musik::core::db::Row
///
///\param column
///Column to get (0 is the first)
///
///\returns
///Column data
//////////////////////////////////////////
int Statement::ColumnInt(int column){
    return sqlite3_column_int(this->stmt,column);
}


//////////////////////////////////////////
///\brief
///Get the results of a column if Step() return a musik::core::db::Row
///
///\param column
///Column to get (0 is the first)
///
///\returns
///Column data
//////////////////////////////////////////
UINT64 Statement::ColumnInt64(int column){
    return sqlite3_column_int64(this->stmt,column);
}


//////////////////////////////////////////
///\brief
///Get the results of a column if Step() return a musik::core::db::Row
///
///\param column
///Column to get (0 is the first)
///
///\returns
///Column data
//////////////////////////////////////////
const char* Statement::ColumnText(int column){
    return (char*)sqlite3_column_text(this->stmt,column);
}


//////////////////////////////////////////
///\brief
///Get the results of a column if Step() return a musik::core::db::Row
///
///\param column
///Column to get (0 is the first)
///
///\returns
///Column data
//////////////////////////////////////////
const wchar_t* Statement::ColumnTextW(int column){
    return (wchar_t*)sqlite3_column_text16(this->stmt,column);
}


//////////////////////////////////////////
///\brief
///Get the results of a column if Step() return a musik::core::db::Row
///
///\param column
///Column to get (0 is the first)
///
///\returns
///Column data
//////////////////////////////////////////
const utfchar* Statement::ColumnTextUTF(int column){
    #ifdef UTF_WIDECHAR
        return (utfchar*)sqlite3_column_text16(this->stmt,column);
    #else
        return (utfchar*)sqlite3_column_text(this->stmt,column);
    #endif
}

