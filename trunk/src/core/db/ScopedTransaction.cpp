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

#ifdef WIN32
#include "pch.hpp"
#else
#include <core/pch.hpp>
#endif

#include <core/db/ScopedTransaction.h>
#include <core/db/Connection.h>

using namespace musik::core::db;


//////////////////////////////////////////
///\brief
///Constructor
///
///\param connection
///Connection to run transaction on
//////////////////////////////////////////
ScopedTransaction::ScopedTransaction(Connection &connection) : canceled(false){
    this->connection    = &connection;
    this->Begin();
}


//////////////////////////////////////////
///\brief
///Destructor will end the transaction if it's the last nested transaction
//////////////////////////////////////////
ScopedTransaction::~ScopedTransaction(){
    this->End();
}


//////////////////////////////////////////
///\brief
///If canceled, this all statements in the transaction scope will be canceled
//////////////////////////////////////////
void ScopedTransaction::Cancel(){
    this->canceled  = true;
}


//////////////////////////////////////////
///\brief
///Sometimes it's a good option to be able to commit a transaction and restart it all over.
//////////////////////////////////////////
void ScopedTransaction::CommitAndRestart(){
    this->End();
    this->Begin();
}

//////////////////////////////////////////
///\brief
///Runs the acctual BEGIN TRANSACTION on the database
//////////////////////////////////////////
void ScopedTransaction::Begin(){
    if(this->connection->transactionCounter==0){
        this->connection->Execute("BEGIN TRANSACTION");
    }
    ++this->connection->transactionCounter;
}

//////////////////////////////////////////
///\brief
///Runs the COMMIT or ROLLBACK on the database
//////////////////////////////////////////
void ScopedTransaction::End(){
    --this->connection->transactionCounter;
    if(this->connection->transactionCounter==0){
        if(this->canceled){
            this->connection->Execute("ROLLBACK TRANSACTION");
        }else{
            this->connection->Execute("COMMIT TRANSACTION");
        }
    }
}


