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

/** @file MasterLibrary.h
 *  @brief ILibrary facade that delegates to a single wrapped library.
 *  @details Wraps one underlying ILibrary and forwards all calls to it. When the
 *      wrapped library changes (e.g. switching between local and remote), the
 *      LibraryChanged signal fires and state is re-broadcast. */

#include <musikcore/config.h>
#include <musikcore/db/Connection.h>

#include <musikcore/library/ILibrary.h>
#include <musikcore/library/IIndexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/library/QueryBase.h>

#include <mutex>

#include <sigslot/sigslot.h>

/** @namespace musik::core::library
 *  @brief Library implementations and the query framework that runs against them. */
namespace musik { namespace core { namespace library {

    /** @brief Facade ILibrary that wraps a single concrete library.
     *  @details All ILibrary operations are delegated to the wrapped library.
     *      Query completions and connection state changes from the wrapped library
     *      are re-broadcast so listeners see a stable interface. */
    class MasterLibrary: public ILibrary, public sigslot::has_slots<> {
        public:
            /** @brief Emitted when the wrapped library is replaced.
             *  @details Arguments are the previous and new libraries. */
            sigslot::signal2<ILibraryPtr, ILibraryPtr> LibraryChanged;

            /** @brief Creates a master library with no wrapped library yet. */
            MasterLibrary();
            virtual ~MasterLibrary();

            /** @brief Queues a query on the wrapped library.
             *  @param query The query to run.
             *  @param cb Optional completion callback.
             *  @return The query id, or -1 if no library is wrapped. */
            int Enqueue(QueryPtr query, Callback cb = Callback()) override;
            /** @brief Runs a query synchronously on the wrapped library.
             *  @param query The query to run.
             *  @param timeoutMs Maximum wait; kWaitIndefinite to block forever.
             *  @param cb Optional completion callback.
             *  @return The query id, or -1 if no library is wrapped. */
            int EnqueueAndWait(QueryPtr query, size_t timeoutMs = kWaitIndefinite, Callback cb = Callback()) override;
            /** @return The wrapped library's indexer, or nullptr. */
            musik::core::IIndexer *Indexer() override;
            /** @return The wrapped library's id. */
            int Id() override;
            /** @return The wrapped library's name. */
            const std::string& Name() override;
            /** @brief Forwards the queue to the wrapped library.
             *  @param queue The queue to use. */
            void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) override;
            /** @return The wrapped library's message queue. */
            musik::core::runtime::IMessageQueue& GetMessageQueue() override;
            /** @return The wrapped library's resource locator. */
            IResourceLocator& GetResourceLocator() override;
            /** @return true if a library is wrapped and configured. */
            bool IsConfigured() override;
            /** @return The wrapped library's connection state. */
            ConnectionState GetConnectionState() const override;
            /** @return The wrapped library's type. */
            Type GetType() const override;
            /** @brief Closes the wrapped library. */
            void Close() override;

            /** @return The currently wrapped library, or nullptr. */
            ILibraryPtr Wrapped() const noexcept { return this->wrappedLibrary; }

            /** @brief Loads the default library (from LibraryFactory) and wraps it. */
            void LoadDefaultLibrary();

        private:
            /** @brief Forwards query completions from the wrapped library.
             *  @param query The completed query. */
            void OnQueryCompleted(musik::core::db::IQuery* query);
            /** @brief Forwards connection state changes from the wrapped library.
             *  @param state The new connection state. */
            void OnConectionStateChanged(ConnectionState state);

            ILibraryPtr wrappedLibrary; /**< The currently wrapped library. */
            mutable std::recursive_mutex libraryMutex; /**< Guards wrappedLibrary. */
    };

} } }
