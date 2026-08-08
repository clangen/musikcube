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

/// @file HttpServer.h
/// @brief HTTP server exposing audio and thumbnails to remote clients.
/// @details Built on GNU libmicrohttpd. Serves transcoded audio streams (via
/// the TranscodingAudioDataStream / cache) and track thumbnails over HTTP so
/// clients can stream tracks and fetch artwork.

extern "C" {
    #pragma warning(push, 0)
    #include <microhttpd.h>
    #pragma warning(pop)
}

#include "Context.h"
#include <condition_variable>
#include <mutex>
#include <vector>

#if MHD_VERSION < 0x00097001
#define MHD_Result int
#endif

/** @brief HTTP server for remote audio and thumbnail streaming.
 *  @details Starts a libmicrohttpd daemon on the configured port. Requests
 *  under the "audio" fragment stream transcoded audio for a track, requests
 *  under "thumbnail" serve album artwork. The server shuts down cleanly when
 *  Stop() is called and Wait() blocks until it exits. */
class HttpServer {
    public:
        /** @brief Constructs a server bound to the shared context.
         *  @param context Shared server context. */
        HttpServer(Context& context);
        /** @brief Destroys the server, stopping it if running. */
        ~HttpServer();

        /** @brief Starts the HTTP daemon.
         *  @return True if the server started. */
        bool Start();
        /** @brief Stops the HTTP daemon.
         *  @return True if the server was stopped. */
        bool Stop();
        /** @brief Blocks until the server thread exits. */
        void Wait();

    private:
        /** @brief libmicrohttpd callback handling incoming requests.
         *  @return MHD_Result indicating how the request was handled. */
        static MHD_Result HandleRequest(
            void *cls,
            struct MHD_Connection *connection,
            const char *url,
            const char *method,
            const char *version,
            const char *upload_data,
            size_t *upload_data_size,
            void **con_cls);

        /** @brief libmicrohttpd callback unescaping URL components.
         *  @return The length of the unescaped string. */
        static size_t HandleUnescape(
            void * cls,
            struct MHD_Connection *c,
            char *s);

        /** @brief Serves a transcoded audio track.
         *  @param server The server instance.
         *  @param response Receives the created response.
         *  @param connection The connection.
         *  @param pathParts Path components of the request.
         *  @return The HTTP response code. */
        static int HandleAudioTrackRequest(
            HttpServer* server,
            MHD_Response*& response,
            MHD_Connection* connection,
            std::vector<std::string>& pathParts);

        /** @brief Serves a track thumbnail image.
         *  @param server The server instance.
         *  @param response Receives the created response.
         *  @param connection The connection.
         *  @param pathParts Path components of the request.
         *  @return The HTTP response code. */
        static int HandleThumbnailRequest(
            HttpServer* server,
            MHD_Response*& response,
            MHD_Connection* connection,
            std::vector<std::string>& pathParts);

        /** @brief The libmicrohttpd daemon handle. */
        struct MHD_Daemon *httpServer;
        /** @brief Shared server context. */
        Context& context;
        /** @brief Whether the server is running. */
        volatile bool running;
        /** @brief Signals the thread when the server stops. */
        std::condition_variable exitCondition;
        /** @brief Guards the exit condition. */
        std::mutex exitMutex;
};
