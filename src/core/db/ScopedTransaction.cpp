//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/db/ScopedTransaction.h>
#include <core/db/Connection.h>

using namespace musik::core::db;

ScopedTransaction::ScopedTransaction(Connection &connection)
: canceled(false) {
    this->connection = &connection;
    this->Begin();
}

ScopedTransaction::~ScopedTransaction() {
    this->End();
}

void ScopedTransaction::Cancel() {
    this->canceled = true;
}

void ScopedTransaction::CommitAndRestart() {
    this->End();
    this->Begin();
}

void ScopedTransaction::Begin() {
    /* we use an IMMEDIATE transaction because we have write-ahead-logging
    enabled on this instance, this generally results in faster queries
    and also allows reads while writing */
    if (this->connection->transactionCounter == 0) {
        this->connection->Execute("BEGIN IMMEDIATE TRANSACTION");
    }

    ++this->connection->transactionCounter;
}

void ScopedTransaction::End() {
    --this->connection->transactionCounter;

    if (this->connection->transactionCounter == 0) {
        if (this->canceled) {
            this->connection->Execute("ROLLBACK TRANSACTION");
        }
        else {
            this->connection->Execute("COMMIT TRANSACTION");
            //this->connection->Checkpoint();
        }
    }

    this->canceled = false;
}
