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

/** @file Statement.h
 *  @brief A prepared and reusable SQL statement bound to a Connection.
 *  @details Compiles a SQL string into a sqlite3_stmt, exposes typed bind and
 *      column-read helpers, and steps through result rows. Statements are bound
 *      to a Connection for their lifetime. */

#include <musikcore/config.h>
#include <map>

struct sqlite3_stmt;

/** @namespace musik::core::db
 *  @brief SQLite database access layer: connections, statements and transactions. */
namespace musik { namespace core { namespace db {

    class Connection;

    /** @brief A prepared SQL statement with typed parameter binding.
     *  @details The statement is compiled lazily against its connection and can be
     *      stepped repeatedly. Bind*() assigns values to parameters by position;
     *      Column*() reads typed values from the current result row. */
    class Statement {
        public:
            DELETE_CLASS_DEFAULTS(Statement)

            /** @brief Prepares the given SQL for execution.
             *  @param sql The SQL text to compile.
             *  @param connection The connection to execute on. */
            Statement(const char* sql, Connection &connection) noexcept;
            virtual ~Statement() noexcept;

            /** @brief Binds a 32-bit integer parameter.
             *  @param position One-based parameter index.
             *  @param bindInt The value to bind. */
            void BindInt32(int position, int bindInt) noexcept;
            /** @brief Binds a 64-bit integer parameter.
             *  @param position One-based parameter index.
             *  @param bindInt The value to bind. */
            void BindInt64(int position, int64_t bindInt) noexcept;
            /** @brief Binds a float parameter.
             *  @param position One-based parameter index.
             *  @param bindFloat The value to bind. */
            void BindFloat(int position, float bindFloat) noexcept;
            /** @brief Binds a text parameter.
             *  @param position One-based parameter index.
             *  @param bindText The value to bind. */
            void BindText(int position, const std::string& bindText);
            /** @brief Binds a NULL parameter.
             *  @param position One-based parameter index. */
            void BindNull(int position) noexcept;

            /** @return The 32-bit integer value of the given column.
             *  @param column Zero-based column index. */
            const int ColumnInt32(int column) noexcept;
            /** @return The 64-bit integer value of the given column.
             *  @param column Zero-based column index. */
            const int64_t ColumnInt64(int column) noexcept;
            /** @return The float value of the given column.
             *  @param column Zero-based column index. */
            const float ColumnFloat(int column) noexcept;
            /** @return The text value of the given column, or nullptr.
             *  @param column Zero-based column index. */
            const char* ColumnText(int column) noexcept;
            /** @return true if the given column holds a NULL value.
             *  @param column Zero-based column index. */
            const bool IsNull(int column) noexcept;

            /** @brief Executes one step of the statement.
             *  @return ReturnCode::Row when a row is available, ReturnCode::Done when
             *      iteration completes, or ReturnCode::Error on failure. */
            int Step();

            /** @brief Resets the statement so it can be stepped again. */
            void Reset() noexcept;
            /** @brief Clears all bound parameter values. */
            void Unbind() noexcept;
            /** @brief Resets the statement and clears all bound parameters. */
            void ResetAndUnbind() noexcept;

        private:
            friend class Connection;

            /** @brief Creates a statement without SQL text (used internally).
             *  @param connection The connection to execute on. */
            Statement(Connection &connection) noexcept;

            sqlite3_stmt *stmt;    /**< Compiled sqlite3 statement handle. */
            Connection *connection;/**< Owning connection. */
            int modifiedRows;      /**< Rows modified by the last step. */
    };

} } }

