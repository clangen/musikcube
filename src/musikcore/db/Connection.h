//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

/** @file Connection.h
 *  @brief RAII wrapper around a single sqlite3 database connection.
 *  @details Provides thread-safe open/close/execute operations, prepares and steps
 *      SQL statements, tracks an active transaction counter and supports WAL
 *      checkpointing and long-running query interruption. */

#include <musikcore/config.h>
#include <musikcore/db/Statement.h>
#include <musikcore/db/ScopedTransaction.h>

#include <map>
#include <mutex>

struct sqlite3;
struct sqlite3_stmt;

/** @namespace musik::core::db
 *  @brief SQLite database access layer: connections, statements and transactions. */
namespace musik { namespace core { namespace db {

    /** @brief Result codes returned by connection and statement operations. */
    typedef enum {
        Okay = 0,  /**< Operation completed successfully. */
        Row = 100, /**< A data row is available from the statement. */
        Done = 101,/**< The statement has finished iterating its rows. */
        Error = 1  /**< An error occurred. */
    } ReturnCode;

    /** @brief Wraps an sqlite3 connection with reference-counted lifecycle.
     *  @details Each open database is represented by one shared sqlite3 handle.
     *      The handle stays alive as long as Statements and ScopedTransactions
     *      referencing it exist. All operations are serialized through an internal
     *      mutex. */
    class Connection {
        public:
            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(Connection)

            /** @brief Creates an unopened connection. */
            Connection() noexcept;
            /** @brief Closes the connection if it is still open. */
            ~Connection();

            /** @brief Opens (or reuses) the database at the given path.
             *  @param database Path to the SQLite database file, or ":memory:".
             *  @param options sqlite3 open flags (0 for defaults).
             *  @param cache Size of the page cache, in KiB (0 for defaults).
             *  @return ReturnCode::Okay on success, or an error code. */
            int Open(const std::string &database, unsigned int options = 0, unsigned int cache = 0);
            /** @brief Closes the connection.
             *  @return ReturnCode::Okay on success, or an error code. */
            int Close() noexcept;
            /** @brief Executes one or more SQL statements without results.
             *  @param sql The SQL to execute.
             *  @return ReturnCode::Okay on success, or an error code. */
            int Execute(const char* sql);

            /** @return The row id of the most recent successful insert on this connection. */
            int64_t LastInsertedId() noexcept;

            /** @return The number of rows modified by the last statement. */
            int LastModifiedRowCount() noexcept;

            /** @brief Requests cancellation of a long-running query on another thread. */
            void Interrupt();
            /** @brief Runs a WAL checkpoint to flush committed pages to the database file. */
            void Checkpoint() noexcept;

        private:
            /** @brief Initializes the connection with the given page cache size. */
            void Initialize(unsigned int cache);
            /** @brief Tracks references to the underlying sqlite3 handle.
             *  @param init true to acquire a reference, false to release one. */
            void UpdateReferenceCount(bool init);
            /** @brief Steps a prepared statement and maps sqlite3 results to ReturnCode.
             *  @param stmt The prepared statement to step.
             *  @return A ReturnCode value. */
            int StepStatement(sqlite3_stmt *stmt) noexcept;

            friend class Statement;
            friend class ScopedTransaction;

            int transactionCounter; /**< Nesting depth of active transactions. */
            sqlite3 *connection;    /**< Underlying sqlite3 handle. */
            std::mutex mutex;       /**< Serializes all connection access. */
    };

} } }

