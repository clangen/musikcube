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

#include <core/config.h>
#include <map>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

//////////////////////////////////////////
// Forward declare
struct sqlite3_stmt;
//////////////////////////////////////////


namespace musik{ namespace core{ namespace db{

    class Connection;
    class CachedStatement;


    //////////////////////////////////////////
    ///\brief
    ///Class for precompiling SQL statements
    //////////////////////////////////////////
    class Statement : boost::noncopyable{
        public: 
            Statement(const char* sql,Connection &connection);
            virtual ~Statement();
            void Reset();
            int Step();

            void BindInt(int position,int bindInt);
            void BindInt64(int position,UINT64 bindInt);
            void BindText(int position,const char* bindText);
            void BindText(int position,const std::string &bindText);
            void BindTextW(int position,const wchar_t* bindText);
            void BindTextW(int position,const std::wstring &bindText);
            void BindTextUTF(int position,const utfchar* bindText);
            void BindTextUTF(int position,const utfstring &bindText);

            int ColumnInt(int column);
            UINT64 ColumnInt64(int column);
            const char* ColumnText(int column);
            const wchar_t* ColumnTextW(int column);
            const utfchar* ColumnTextUTF(int column);

            void UnBindAll();
        private:
            friend class Connection;

            sqlite3_stmt *stmt;
            Connection *connection;

        private:
            friend class CachedStatement;
            Statement(Connection &connection);

    };


} } }

#include <core/db/CachedStatement.h>

