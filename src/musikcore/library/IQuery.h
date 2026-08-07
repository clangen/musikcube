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

/** @file IQuery.h
 *  @brief Abstract interfaces for library queries and serializable queries.
 *  @details A query is a unit of work executed against a library, typically on
 *      the library's query thread. Serializable queries additionally support
 *      marshaling their input and result across a network connection (used by
 *      the remote library). */

#include <string>
#include <vector>
#include <sigslot/sigslot.h>

#include <musikcore/library/IIndexer.h>
#include <musikcore/db/Connection.h>
#include <musikcore/support/DeleteDefaults.h>

/** @namespace musik::core::db
 *  @brief Library query framework: unit-of-work queries against a library. */
namespace musik { namespace core { namespace db {

    /** @brief Base interface for all library queries.
     *  @details Queries are created by callers, queued on a library, and executed
     *      asynchronously. The library drives their lifecycle and notifies callers
     *      on completion. */
    class IQuery {
        public:
            /** @brief Lifecycle status of a query. */
            typedef enum {
                Idle = 1,     /**< Created but not yet run. */
                Running = 2,  /**< Currently executing. */
                Failed = 3,   /**< Execution failed. */
                Finished = 4, /**< Execution completed successfully. */
                Canceled = 5  /**< Execution was cancelled. */
            } Status;

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS_WITH_DEFAULT_CTOR(IQuery)

            virtual ~IQuery() { }

            /** @return The current status of the query. */
            virtual int GetStatus() = 0;
            /** @return The unique id assigned to this query. */
            virtual int GetId() = 0;
            /** @return The option flags associated with this query. */
            virtual int GetOptions() = 0;
            /** @return A human-readable name for the query type. */
            virtual std::string Name() = 0;
    };

    /** @brief A query whose input and result can be serialized.
     *  @details Extends IQuery with methods to serialize the query description and
     *      its result, and to deserialize a result received from a remote library. */
    class ISerializableQuery: public IQuery {
        public:
            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS_WITH_DEFAULT_CTOR(ISerializableQuery)

            virtual ~ISerializableQuery() { }

            /** @return A serialized representation of the query's parameters. */
            virtual std::string SerializeQuery() = 0;
            /** @return A serialized representation of the query's result. */
            virtual std::string SerializeResult() = 0;
            /** @brief Populates this query's result from serialized data.
             *  @param data The serialized result. */
            virtual void DeserializeResult(const std::string& data) = 0;
            /** @brief Marks the query's result as stale/invalid. */
            virtual void Invalidate() = 0;
    };

} } }
