//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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
#include <sqlite/sqlite3.h>

static std::mutex globalMutex;

using namespace musik::core::db;

Connection::Connection()
: connection(nullptr)
, transactionCounter(0) {
    this->UpdateReferenceCount(true);
}

Connection::~Connection() {
    this->Close();
    this->UpdateReferenceCount(false);
}

int Connection::Open(const std::string &database, unsigned int options, unsigned int cache) {
    int error;

    #ifdef WIN32
        std::wstring wdatabase = u8to16(database);
        error = sqlite3_open16(wdatabase.c_str(), &this->connection);
    #else
        error = sqlite3_open(database.c_str(), &this->connection);
    #endif

    if (error == SQLITE_OK) {
        this->Initialize(cache);
    }

    return error;
}

int Connection::Close() {
    if (sqlite3_close(this->connection) == SQLITE_OK) {
        this->connection = 0;
        return Okay;
    }

    return Error;
}

int Connection::Execute(const char* sql) {
    sqlite3_stmt *stmt  = nullptr;

    /* prepare seems to give errors when interrupted */
    {
        std::unique_lock<std::mutex> lock(this->mutex);

        if (sqlite3_prepare_v2(this->connection, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return Error;
        }
    }

    int error = this->StepStatement(stmt);
    if (error != SQLITE_OK && error != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return Error;
    }

    sqlite3_reset(stmt);
    sqlite3_finalize(stmt);

    return Okay;
}

void Connection::Checkpoint() {
    sqlite3_wal_checkpoint(this->connection, nullptr);
}

int64_t Connection::LastInsertedId() {
    return sqlite3_last_insert_rowid(this->connection);
}

int Connection::LastModifiedRowCount() {
    return (int) sqlite3_changes(this->connection);
}

void Connection::Initialize(unsigned int cache) {
    sqlite3_enable_shared_cache(1);
    sqlite3_busy_timeout(this->connection, 10000);

    sqlite3_exec(this->connection, "PRAGMA optimize", nullptr, nullptr, nullptr);           // Optimize the database when applicable
    sqlite3_exec(this->connection, "PRAGMA synchronous=NORMAL", nullptr, nullptr, nullptr); // NORMAL useful for auto-checkpointing with WAL
    sqlite3_exec(this->connection, "PRAGMA page_size=4096", nullptr, nullptr, nullptr);	    // According to windows standard page size
    sqlite3_exec(this->connection, "PRAGMA auto_vacuum=0", nullptr, nullptr, nullptr);	    // No autovaccum.
    sqlite3_exec(this->connection, "PRAGMA journal_mode=WAL", nullptr, nullptr, nullptr);   // Allow reading while writing (write-ahead-logging)

    if (cache != 0) {
        // Divide by 4 to since the page_size is 4096
        // Total cache is the same as page_size*cache_size
        cache = cache / 4;
        std::string cacheSize("PRAGMA cache_size=" + std::to_string(cache));
        sqlite3_exec(this->connection,cacheSize.c_str(), nullptr, nullptr, nullptr); // size * 1.5kb = 6Mb cache
    }

    sqlite3_exec(this->connection, "PRAGMA case_sensitive_like=0", nullptr, nullptr, nullptr);   // More speed if case insensitive
    sqlite3_exec(this->connection, "PRAGMA count_changes=0", nullptr, nullptr, nullptr);         // If set it counts changes on SQL UPDATE. More speed when not.
    sqlite3_exec(this->connection, "PRAGMA legacy_file_format=OFF", nullptr, nullptr, nullptr);  // No reason to be backwards compatible :)
    sqlite3_exec(this->connection, "PRAGMA temp_store=MEMORY", nullptr, nullptr, nullptr);       // MEMORY, not file. More speed.
}

void Connection::Interrupt() {
    std::unique_lock<std::mutex> lock(this->mutex);
    sqlite3_interrupt(this->connection);
}

void Connection::UpdateReferenceCount(bool init) {
    std::unique_lock<std::mutex> lock(this->mutex);

    static int count = 0;

    if (init) {
        if (count == 0) {
            sqlite3_initialize();
        }

        ++count;
    }
    else {
        --count;
        if (count <= 0) {
            sqlite3_shutdown();
            count = 0;
        }
    }
}

int Connection::StepStatement(sqlite3_stmt *stmt) {
    return sqlite3_step(stmt);
}
