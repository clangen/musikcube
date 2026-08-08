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

/** @file ScopedTransaction.h
 *  @brief RAII transaction guard that begins and commits a SQLite transaction.
 *  @details Begins a transaction on construction and commits it on destruction,
 *      unless Cancel() is called. Supports restarting the transaction to allow
 *      batches of work to be committed incrementally. */

#include <musikcore/config.h>
#include <map>

/** @namespace musik::core::db
 *  @brief SQLite database access layer: connections, statements and transactions. */
namespace musik { namespace core { namespace db {

    class Connection;

    /** @brief RAII wrapper for a SQLite transaction.
     *  @details A transaction is begun in the constructor and committed when the
     *      object goes out of scope. Call Cancel() to roll back instead. */
    class ScopedTransaction {
        public:
            DELETE_CLASS_DEFAULTS(ScopedTransaction)

            /** @brief Begins a transaction on the given connection.
             *  @param connection The connection to run the transaction on. */
            ScopedTransaction(Connection &connection);
            /** @brief Commits the transaction (unless it was cancelled). */
            ~ScopedTransaction();

            /** @brief Marks the transaction as cancelled; the destructor will roll back. */
            void Cancel() noexcept;
            /** @brief Commits the current transaction and immediately begins a new one. */
            void CommitAndRestart();

        private:
            /** @brief Begins the underlying SQLite transaction. */
            inline void Begin();
            /** @brief Ends the underlying SQLite transaction (commit or roll back). */
            inline void End();

            Connection *connection; /**< Connection the transaction runs on. */
            bool canceled;          /**< true if the transaction should roll back. */
    };

} } }

