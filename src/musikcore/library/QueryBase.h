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

/** @file QueryBase.h
 *  @brief Common base class for all local library queries.
 *  @details Provides the shared lifecycle (id assignment, status tracking,
 *      cancellation, options) used by concrete query classes. Derived classes
 *      implement OnRun() to perform their actual work against a Connection. */

#include <musikcore/config.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/db/Connection.h>

#include <sigslot/sigslot.h>

#include <mutex>
#include <atomic>

/** @namespace musik::core::library::query
 *  @brief Query classes and helpers executed against a library. */
namespace musik { namespace core { namespace library { namespace query {

    /** @brief Base class implementing shared query mechanics.
     *  @details Tracks status (IQuery::Status), a globally unique id, an option
     *      bitmask and a cancel flag. Run() drives the lifecycle around the pure
     *      virtual OnRun(). */
    class QueryBase:
        public musik::core::db::ISerializableQuery,
        public sigslot::has_slots<>
    {
        public:
            /** @brief How string filters are matched against track text. */
            enum class MatchType : int {
                Substring = 1, /**< Case-insensitive substring match. */
                Regex = 2      /**< Regular expression match. */
            };

            /** @brief Creates a query with a fresh unique id. */
            QueryBase() noexcept
            : status(IQuery::Idle)
            , options(0)
            , queryId(nextId())
            , cancel(false) {
            }

            /** @brief Executes the query against the given connection.
             *  @param db The database connection to run on.
             *  @return true if the query finished successfully (or was cancelled). */
            bool Run(musik::core::db::Connection &db) {
                this->SetStatus(Running);
                try {
                    if (this->IsCanceled()) {
                        this->SetStatus(Canceled);
                        return true;
                    }
                    else if (OnRun(db)) {
                        this->SetStatus(Finished);
                        return true;
                    }
                }
                catch (...) {
                }

                this->SetStatus(Failed);
                return false;
            }

            /** @brief Requests cancellation of the query.
             *  @note The query is checked for cancellation before OnRun(). */
            virtual void Cancel() noexcept {
                this->cancel = true;
            }

            /** @return true if the query has been cancelled. */
            virtual bool IsCanceled() noexcept {
                return cancel;
            }

            /* IQuery */

            /** @return The current query status. */
            int GetStatus() override {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                return this->status;
            }

            /** @return The unique id assigned to this query. */
            int GetId() noexcept override {
                return this->queryId;
            }

            /** @return The option flags associated with this query. */
            int GetOptions() override {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                return this->options;
            }

            /* ISerializableQuery */

            /** @brief Default implementation throws; overridden by network queries.
             *  @return Never returns. */
            std::string SerializeQuery() override {
                throw std::runtime_error("not implemented");
            }

            /** @brief Default implementation throws; overridden by network queries.
             *  @return Never returns. */
            std::string SerializeResult() override {
                throw std::runtime_error("not implemented");
            }

            /** @brief Default implementation throws; overridden by network queries.
             *  @param data The serialized result. */
            void DeserializeResult(const std::string& data) override {
                throw std::runtime_error("not implemented");
            }

            /** @brief Marks the query as failed (used when data becomes stale). */
            void Invalidate() override {
                this->SetStatus(IQuery::Failed);
            }

        protected:
            /** @brief Sets the current status.
             *  @param status The new IQuery::Status value. */
            void SetStatus(int status) {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                this->status = status;
            }

            /** @brief Sets the option flags.
             *  @param options The new option bitmask. */
            void SetOptions(int options) {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                this->options = options;
            }

            /** @brief Performs the query's actual work.
             *  @param db The database connection to run against.
             *  @return true on success. */
            virtual bool OnRun(musik::core::db::Connection& db) = 0;

        private:
            /** @return The next globally unique query id. */
            static int nextId() noexcept {
                static std::atomic<int> next(0);
                return ++next;
            }

            unsigned int status;     /**< Current IQuery::Status. */
            unsigned int queryId;    /**< Unique query id. */
            unsigned int options;    /**< Option flags. */
            volatile bool cancel;    /**< Cancellation flag. */
            std::mutex stateMutex;   /**< Guards status and options. */
    };

} } } }
