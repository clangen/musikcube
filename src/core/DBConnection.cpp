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

#include <core/DBConnection.h>
#include <core/Common.h>

#include <boost/format.hpp>

using namespace musik::core;

//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
DBConnection::DBConnection(void):transactionCounter(0),oDB(NULL){
}

//////////////////////////////////////////
///\brief
///Destructor. Will automaticaly call CloseDB to make sure the db is closed.
///
///\see
///CloseDB
//////////////////////////////////////////
DBConnection::~DBConnection(void){
    this->CloseDB();
}

//////////////////////////////////////////
///\brief
///Begin a database transaction.
///
///The class will keep track of nested transactions
///and will only commit when the count is down to 0 again.
///
///\see
///TransactionEnd
//////////////////////////////////////////
void DBConnection::TransactionBegin(){
    if(this->transactionCounter==0){
        sqlite3_exec(this->oDB,"BEGIN TRANSACTION",NULL,NULL,NULL);
    }
    ++(this->transactionCounter);
}

//////////////////////////////////////////
///\brief
///End a database transaction.
///
///The class will keep track of nested transactions
///and will only commit when the count is down to 0 again.
///
///\see
///TransactionStart
//////////////////////////////////////////
void DBConnection::TransactionEnd(){
    --(this->transactionCounter);
    if(this->transactionCounter==0){
        sqlite3_exec(this->oDB,"COMMIT TRANSACTION",NULL,NULL,NULL);
    }
}

//////////////////////////////////////////
///\brief
///Open a connection to the database.
///
///\param iCacheSize
///Size of SQLites internal cache in kilobytes.
///
///\param bReadonly
///Should it be opened as read only?
///
///\returns
///true on success. This is expected.
///
///\see
///CloseDB
//////////////////////////////////////////
bool DBConnection::InitDB(int iCacheSize,bool bReadonly){
    std::string sDBA    = musik::core::ConvertUTF8(this->sDB);
    if(bReadonly){
        if( sqlite3_open_v2(sDBA.c_str(),&this->oDB,SQLITE_OPEN_READONLY,NULL) !=SQLITE_OK ){
            return false;
        }
    }else{
        if( sqlite3_open_v2(sDBA.c_str(),&this->oDB,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,NULL)!=SQLITE_OK ){
            return false;
        }
    }

    sqlite3_busy_timeout(this->oDB,10000);

    sqlite3_exec(this->oDB,"PRAGMA synchronous=OFF",NULL,NULL,NULL);        // Maybe FULL if multiuser document
    sqlite3_exec(this->oDB,"PRAGMA page_size=1024",NULL,NULL,NULL);            // According to windows standard page size
    sqlite3_exec(this->oDB,"PRAGMA auto_vacuum=0",NULL,NULL,NULL);            // No autovaccum.

    std::string sCacheSize(boost::str(boost::format("PRAGMA cache_size=%1%")%iCacheSize));
    sqlite3_exec(this->oDB,sCacheSize.c_str(),NULL,NULL,NULL);                // size * 1.5kb = 6Mb cache

    sqlite3_exec(this->oDB,"PRAGMA case_sensitive_like=0",NULL,NULL,NULL);    // More speed if case insensitive
    sqlite3_exec(this->oDB,"PRAGMA count_changes=0",NULL,NULL,NULL);        // If set it counts changes on SQL UPDATE. More speed when not.
//    sqlite3_exec(this->oDB,"PRAGMA encoding=UTF-16",NULL,NULL,NULL);        // Unicode as standard. Propbably not needed since sqlite3_open16 is used.
    sqlite3_exec(this->oDB,"PRAGMA legacy_file_format=OFF",NULL,NULL,NULL);    // Set compatible with ALL sqlite 3.x versions
    sqlite3_exec(this->oDB,"PRAGMA temp_store=MEMORY",NULL,NULL,NULL);        // MEMORY, not file. More speed.


    return true;
}

//////////////////////////////////////////
///\brief
///Close the database
///
///\returns
///true
///
///\see
///InitDB
//////////////////////////////////////////
bool DBConnection::CloseDB(){
    if(this->oDB){
        sqlite3_close(this->oDB);
        this->oDB    = NULL;
    }
    return true;
}


