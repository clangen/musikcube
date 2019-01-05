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

#include <core/db/Statement.h>
#include <core/db/Connection.h>
#include <sqlite/sqlite3.h>

using namespace musik::core::db;

Statement::Statement(const char* sql, Connection &connection)
: connection(&connection)
, stmt(nullptr)
, modifiedRows(0) {
    std::unique_lock<std::mutex> lock(connection.mutex);

    sqlite3_prepare_v2(
        this->connection->connection, sql, -1, &this->stmt, nullptr);
}

Statement::Statement(Connection &connection)
: connection(&connection)
, stmt(nullptr) {
}

Statement::~Statement() {
    sqlite3_finalize(this->stmt);
}

void Statement::Reset() {
    sqlite3_reset(this->stmt);
}

void Statement::Unbind() {
    sqlite3_clear_bindings(this->stmt);
}

void Statement::ResetAndUnbind() {
    sqlite3_reset(this->stmt);
    sqlite3_clear_bindings(this->stmt);
}

int Statement::Step() {
    int result = this->connection->StepStatement(this->stmt);

    if (result == SQLITE_OK) {
        this->modifiedRows = this->connection->LastModifiedRowCount();
    }

    return result;
}

void Statement::BindInt32(int position, int bindInt) {
    sqlite3_bind_int(this->stmt, position + 1, bindInt);
}

void Statement::BindInt64(int position, int64_t bindInt) {
    sqlite3_bind_int64(this->stmt, position + 1, (sqlite3_int64) bindInt);
}

void Statement::BindFloat(int position, float bindFloat) {
    sqlite3_bind_double(this->stmt, position + 1, bindFloat);
}

void Statement::BindText(int position, const std::string& bindText) {
    std::string sanitized;
    utf8::replace_invalid(
        bindText.begin(),
        bindText.end(),
        std::back_inserter(sanitized),
        (uint32_t) '?');

    sqlite3_bind_text(
        this->stmt, position + 1,
        sanitized.c_str(),
        -1,
        SQLITE_TRANSIENT);
}

void Statement::BindNull(int position) {
    sqlite3_bind_null(this->stmt, position + 1);
}

int Statement::ColumnInt32(int column) {
    return sqlite3_column_int(this->stmt, column);
}

int64_t Statement::ColumnInt64(int column) {
    return sqlite3_column_int64(this->stmt, column);
}

float Statement::ColumnFloat(int column) {
    return (float) sqlite3_column_double(this->stmt, column);
}

const char* Statement::ColumnText(int column) {
    const char* text = (char*) sqlite3_column_text(this->stmt, column);
    return text ? text : "";
}

const wchar_t* Statement::ColumnTextW(int column) {
    const wchar_t* text = (wchar_t*) sqlite3_column_text16(this->stmt, column);
    return text ? text : L"";
}
