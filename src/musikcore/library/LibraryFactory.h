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

/** @file LibraryFactory.h
 *  @brief Registry and factory for the application's music libraries.
 *  @details Creates, tracks and looks up ILibrary instances (local and remote).
 *      The factory is a process-wide singleton that must be initialized with a
 *      message queue before use. */

#include <musikcore/config.h>
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/runtime/IMessageQueue.h>
#include <sigslot/sigslot.h>
#include <map>
#include <vector>

/** @namespace musik::core
 *  @brief Core application services: libraries, indexing, playback and utilities. */
namespace musik { namespace core {

    /** @brief Creates and manages all active library instances.
     *  @details Libraries are registered by id and type. The default local and
     *      remote libraries are lazily created on first access. A signal notifies
     *      listeners whenever the set of libraries changes. */
    class LibraryFactory {
        public:
            using LibraryVector = std::vector<ILibraryPtr>; /**< List of libraries. */
            using LibraryMap = std::map<int, ILibraryPtr>;  /**< Libraries indexed by id. */
            using LibrariesUpdatedEvent = sigslot::signal0<>; /**< Fired when the library set changes. */
            using IMessageQueue = musik::core::runtime::IMessageQueue; /**< Queue alias. */

            /** @brief Emitted whenever libraries are added or removed. */
            LibrariesUpdatedEvent LibrariesUpdated;

            ~LibraryFactory();

            /** @brief Initializes the factory singleton.
             *  @param messageQueue The queue used by all created libraries. */
            static void Initialize(IMessageQueue& messageQueue);
            /** @return The process-wide factory singleton. */
            static LibraryFactory& Instance();
            /** @brief Shuts down and clears all libraries. */
            static void Shutdown();

            /** @return The default local library (creating it if needed). */
            ILibraryPtr DefaultLocalLibrary();
            /** @return The default remote library (creating it if needed). */
            ILibraryPtr DefaultRemoteLibrary();
            /** @return The default library of the given type.
             *  @param type Type::Local or Type::Remote. */
            ILibraryPtr DefaultLibrary(ILibrary::Type type);

            /** @return All registered libraries. */
            LibraryVector Libraries();
            /** @brief Creates and registers a new library.
             *  @param name The library name.
             *  @param type The library type.
             *  @return The created library. */
            ILibraryPtr CreateLibrary(const std::string& name, ILibrary::Type type);

            /** @brief Looks up a library by id.
             *  @param identifier The library id.
             *  @return The library, or nullptr if not found. */
            ILibraryPtr GetLibrary(int identifier);

        private:
            /** @brief Creates the factory (private). */
            LibraryFactory();

            /** @brief Creates, registers and returns a library.
             *  @param id The library id.
             *  @param type The library type.
             *  @param name The library name.
             *  @return The created library. */
            ILibraryPtr AddLibrary(int id, ILibrary::Type type, const std::string& name);

            LibraryVector libraries; /**< Ordered list of libraries. */
            LibraryMap libraryMap;   /**< Libraries indexed by id. */
    };

} }
