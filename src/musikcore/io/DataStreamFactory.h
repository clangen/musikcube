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

/** @file DataStreamFactory.h
 *  @brief Opens data streams (files, URLs, ...) through registered factory plugins.
 *  @details Maintains a list of IDataStreamFactory implementations registered by
 *      plugins and routes OpenSharedDataStream()/OpenDataStream() to the first
 *      factory able to handle the given URI. */

#include <musikcore/config.h>
#include <musikcore/sdk/IDataStream.h>
#include <musikcore/sdk/IDataStreamFactory.h>
#include <vector>

/** @namespace musik::core::io
 *  @brief Input/output helpers: data streams and stream factories. */
namespace musik { namespace core { namespace io {

    /** @brief Dispatches data-stream opens to registered factory plugins.
     *  @details The first factory whose CanOpen() matches the URI is used. Factories
     *      are registered by plugins at load time. */
    class DataStreamFactory {
        public:
            using DataStreamPtr = std::shared_ptr<musik::core::sdk::IDataStream>; /**< Shared stream alias. */
            using OpenFlags = musik::core::sdk::OpenFlags; /**< Open flag alias. */

            /** @brief Opens a shared data stream for the given URI.
             *  @param uri The URI to open (file path, URL, etc.).
             *  @param flags Read/write access flags.
             *  @return A shared stream, or nullptr if no factory matched. */
            static DataStreamPtr OpenSharedDataStream(const char *uri, OpenFlags flags);
            /** @brief Opens an unmanaged data stream for the given URI.
             *  @param uri The URI to open (file path, URL, etc.).
             *  @param flags Read/write access flags.
             *  @return A raw stream (release with Release()), or nullptr. */
            static musik::core::sdk::IDataStream* OpenDataStream(const char* uri, OpenFlags flags);

        private:
            typedef std::vector<std::shared_ptr<musik::core::sdk::IDataStreamFactory> > DataStreamFactoryVector; /**< Registered factories. */

            DELETE_COPY_AND_ASSIGNMENT_DEFAULTS(DataStreamFactory)

            /** @brief Creates the factory (private). */
            DataStreamFactory();
            /** @return The process-wide factory singleton. */
            static DataStreamFactory* Instance();

            DataStreamFactoryVector dataStreamFactories; /**< Registered IDataStreamFactory instances. */
    };

} } }
