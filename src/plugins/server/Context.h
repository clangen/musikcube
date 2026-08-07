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

/// @file Context.h
/// @brief Shared context and read/write lock for the streaming server.
/// @details Bundles the SDK services (metadata proxy, preferences, playback,
/// environment, debug) used by the HTTP and WebSocket servers, together with a
/// read/write lock that protects the shared services from concurrent access.

#include <musikcore/sdk/IMetadataProxy.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/sdk/IEnvironment.h>
#include <musikcore/sdk/IDebug.h>

#include <shared_mutex>
#include <mutex>

/** @brief Simple read/write lock backed by a shared mutex.
 *  @details Write() returns an exclusive lock, Read() returns a shared lock.
 *  Both use RAII guard types so locks are released automatically. */
class ReadWriteLock {
    typedef std::shared_mutex Mutex;
    typedef std::unique_lock<Mutex> WriteLock;
    typedef std::shared_lock<Mutex> ReadLock;

    public:
        /** @brief Acquires an exclusive write lock.
         *  @return The write lock guard. */
        WriteLock Write() {
            WriteLock wl(stateMutex);
            return std::move(wl);
        }

        /** @brief Acquires a shared read lock.
         *  @return The read lock guard. */
        ReadLock Read() {
            ReadLock rl(stateMutex);
            return std::move(rl);
        }

    private:
        /** @brief The underlying shared mutex. */
        Mutex stateMutex;
};

/** @brief Services shared by the server's HTTP and WebSocket endpoints.
 *  @details Pointers are populated by the plugin host when the server is
 *  created. Access to the members should be guarded with the lock field. */
struct Context {
    /** @brief Initializes all service pointers to null. */
    Context() {
        this->metadataProxy = nullptr;
        this->prefs = nullptr;
        this->playback = nullptr;
        this->debug = nullptr;
    }

    /** @brief Proxy for reading track metadata. */
    musik::core::sdk::IMetadataProxy* metadataProxy;
    /** @brief Application preferences service. */
    musik::core::sdk::IPreferences* prefs;
    /** @brief The playback service being controlled. */
    musik::core::sdk::IPlaybackService* playback;
    /** @brief Environment information (paths, libraries). */
    musik::core::sdk::IEnvironment* environment;
    /** @brief Debug logging service. */
    musik::core::sdk::IDebug* debug;
    /** @brief Guards the shared services. */
    ReadWriteLock lock;
};