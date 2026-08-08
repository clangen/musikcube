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

/** @file ILibrary.h
 *  @brief Abstract interface for a music library.
 *  @details A library stores indexed music metadata and executes asynchronous
 *      queries against it. Implementations include the local SQLite-backed
 *      library and the remote (server-backed) library. */

#include <string>
#include <vector>
#include <functional>
#include <limits>

#include <sigslot/sigslot.h>

#include <musikcore/library/IIndexer.h>
#include <musikcore/library/IQuery.h>
#include <musikcore/sdk/ITrack.h>
#include <musikcore/runtime/IMessageQueue.h>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief Sentinel value meaning "wait forever" for EnqueueAndWait(). */
    static size_t kWaitIndefinite = std::numeric_limits<size_t>::max();

    /** @brief Abstract music library interface.
     *  @details Libraries own an Indexer, process queued queries (asynchronously
     *      or synchronously), and report their connection state and type. */
    class ILibrary {
        public:
            using QueryPtr = std::shared_ptr<musik::core::db::IQuery>; /**< Shared query alias. */
            using Callback = std::function<void(QueryPtr)>; /**< Query completion callback alias. */

            /** @brief The kind of backing store a library uses. */
            enum class Type: int {
                Local = 1,  /**< A local SQLite-backed library. */
                Remote = 2  /**< A remote (server-backed) library. */
            };

            /** @brief Connectivity state of the library. */
            enum class ConnectionState: int {
                Disconnected = 0,        /**< Not connected. */
                Connected = 1,           /**< Connected and usable. */
                Connecting = 2,          /**< Connection is in progress. */
                AuthenticationFailure = 3 /**< Connection rejected due to credentials. */
            };

            /** @brief Resolves the playable URI for a given track.
             *  @details Implementations translate a library track into a concrete
             *      streamable URI, optionally using a caller-provided default. */
            class IResourceLocator {
                public:
                    /** @brief Returns the playable URI for the track.
                     *  @param track The track to resolve.
                     *  @param defaultUri Optional fallback URI.
                     *  @return The resolved URI. */
                    virtual std::string GetTrackUri(
                        musik::core::sdk::ITrack* track,
                        const std::string& defaultUri = "") = 0;
            };

            /** @brief Emitted when a query has finished executing. */
            sigslot::signal1<musik::core::db::IQuery*> QueryCompleted;
            /** @brief Emitted when the library connection state changes. */
            sigslot::signal1<ConnectionState> ConnectionStateChanged;

            virtual ~ILibrary() { }

            /** @brief Queues a query for asynchronous execution.
             *  @param query The query to run.
             *  @param cb Optional callback invoked when the query completes.
             *  @return The query id. */
            virtual int Enqueue(QueryPtr query, Callback cb = Callback()) = 0;
            /** @brief Runs a query and blocks until it completes.
             *  @param query The query to run.
             *  @param timeoutMs Maximum wait time; use kWaitIndefinite to block forever.
             *  @param cb Optional callback invoked when the query completes.
             *  @return The query id. */
            virtual int EnqueueAndWait(QueryPtr query, size_t timeoutMs = kWaitIndefinite, Callback cb = Callback()) = 0;
            /** @return The indexer associated with this library. */
            virtual IIndexer *Indexer() = 0;
            /** @return The unique identifier of this library. */
            virtual int Id() = 0;
            /** @return The display name of this library. */
            virtual const std::string& Name() = 0;
            /** @brief Binds a message queue used to deliver async events.
             *  @param queue The queue to use. */
            virtual void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) = 0;
            /** @return The message queue bound to this library. */
            virtual musik::core::runtime::IMessageQueue& GetMessageQueue() = 0;
            /** @return The resource locator used to resolve track URIs. */
            virtual IResourceLocator& GetResourceLocator() = 0;
            /** @return true if the library has been configured with a name/id. */
            virtual bool IsConfigured() = 0;
            /** @return The current connection state. */
            virtual ConnectionState GetConnectionState() const = 0;
            /** @return The type of this library. */
            virtual Type GetType() const = 0;
            /** @brief Closes the library and releases its resources. */
            virtual void Close() = 0;
    };

    typedef std::shared_ptr<ILibrary> ILibraryPtr; /**< Shared library alias. */

} }
