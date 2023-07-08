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

#include "pch.hpp"

#pragma warning(push, 0)
extern "C" {
    #include <sqlean/unicode/extension.h>
    #include <sqlite/sqlite3.h>
}
#include <musikcore/db/SqliteExtensions.h>
#pragma warning(pop)

#include <unordered_map>
#include <regex>

/* NOTE: nearly all of the source here was pulled in from sqlite3.c except for the
extension registration and accentToChar mapping; this is basically a copy/paste of
the default LIKE operator that supports coercing accented characters to standard
english characters to make it easier to search. */

#ifndef UINT32_TYPE
# ifdef HAVE_UINT32_T
#  define UINT32_TYPE uint32_t
# else
#  define UINT32_TYPE unsigned int
# endif
#endif
#ifndef UINT8_TYPE
# ifdef HAVE_UINT8_T
#  define UINT8_TYPE uint8_t
# else
#  define UINT8_TYPE unsigned char
# endif
#endif

typedef UINT8_TYPE u8;             /* 1-byte unsigned integer */
typedef UINT32_TYPE u32;           /* 4-byte unsigned integer */

/*
** Function to delete compiled regexp objects. Registered as
** a destructor function with sqlite3_set_auxdata().
*/
static void regexpDelete(void* p) {
    std::regex* regex = (std::regex*) p;
    delete regex;
}

/*
** Implementation of SQLite REGEXP operator. This scalar function takes
** two arguments. The first is a regular expression pattern to compile
** the second is a string to match against that pattern. If either
** argument is an SQL NULL, then NULL Is returned. Otherwise, the result
** is 1 if the string matches the pattern, or 0 otherwise.
**
** SQLite maps the regexp() function to the regexp() operator such
** that the following two are equivalent:
**
**     zString REGEXP zPattern
**     regexp(zPattern, zString)
**
** Uses standard C++ std::regex
*/
static void regexpFunc(sqlite3_context* context, int nArg, sqlite3_value** apArg) {
    static auto kRegexFlags =
        std::regex::icase |
        std::regex::optimize |
        std::regex::collate |
        std::regex::ECMAScript;

    static auto kMatchFlags =
        std::regex_constants::match_any;

    const char* matchAgainst = (const char*) sqlite3_value_text(apArg[1]);

    if (!matchAgainst) {
        return;
    }

    std::regex* regex = (std::regex*) sqlite3_get_auxdata(context, 0);
    if (!regex) {
        const char* pattern = (const char*) sqlite3_value_text(apArg[0]);
        if (!pattern) {
            return;
        }

        try {
            regex = new std::regex(pattern, kRegexFlags);
        }
        catch (std::regex_error) {
            return;
        }

        sqlite3_set_auxdata(context, 0, regex, regexpDelete);
    }

    /* Return 1 or 0. */
    sqlite3_result_int(context, std::regex_search(matchAgainst, *regex, kMatchFlags) ? 1 : 0);
}

namespace musik { namespace core { namespace db {

    namespace SqliteExtensions {

        int Register(sqlite3* db) {
            static const struct Scalar {
                const char* zName; /* Function name */
                unsigned char nArg; /* Number of arguments */
                unsigned int enc; /* Optimal text encoding */
                void (*xFunc)(sqlite3_context*, int, sqlite3_value**);
            } scalars[] = {
                {"regexp", 2, SQLITE_ANY | SQLITE_DETERMINISTIC| SQLITE_INNOCUOUS, regexpFunc},
            };
            int rc = SQLITE_OK;
            for (int i = 0; rc == SQLITE_OK && i < (int)(sizeof(scalars) / sizeof(scalars[0])); i++) {
                const struct Scalar* p = &scalars[i];
                rc = sqlite3_create_function(
                    db,
                    p->zName,
                    p->nArg,
                    p->enc,
                    nullptr,
                    p->xFunc,
                    0,
                    0);
            }
            if (rc != SQLITE_OK) {
                return rc;
            }
            return unicode_init(db);
        }

    }

} } }

