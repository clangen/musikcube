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

///

/// @file HttpDataStreamFactory.h
/// @brief Factory that creates HttpDataStream instances for remote URLs.
/// @details Recognizes http:// and https:// URIs (plus the remote-track host
/// prefix) and produces the IDataStream used by the decoder to stream audio
/// over the network.

#include <musikcore/sdk/IDataStreamFactory.h>

using namespace musik::core::sdk;

/** @brief Creates HTTP data streams.
 *  @details Registers the "http"/"https" URI schemes so remote audio tracks are
 *  routed to the HttpDataStream implementation. */
class HttpDataStreamFactory : public IDataStreamFactory {
    public:
        /** @brief Open flags alias. */
        using OpenFlags = musik::core::sdk::OpenFlags;

        /** @brief Constructs the factory. */
        HttpDataStreamFactory();
        /** @brief Destroys the factory. */
        ~HttpDataStreamFactory();

        /** @brief Returns whether this factory can open the given URI.
         *  @param uri The URI to check.
         *  @return True for http/https URIs. */
        virtual bool CanRead(const char *uri);
        /** @brief Opens a stream for the given URI.
         *  @param uri The http/https URI to open.
         *  @param flags Open flags.
         *  @return A new HttpDataStream, or null on failure. */
        virtual IDataStream* Open(const char *uri, OpenFlags flags);
        /** @brief Destroys the factory instance. */
        virtual void Release();
};