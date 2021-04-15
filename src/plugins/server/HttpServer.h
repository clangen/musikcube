//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

class HttpServer {
    public:
        HttpServer(Context& context);
        ~HttpServer();

        bool Start();
        bool Stop();
        void Wait();

    private:
        static MHD_Result HandleRequest(
            void *cls,
            struct MHD_Connection *connection,
            const char *url,
            const char *method,
            const char *version,
            const char *upload_data,
            size_t *upload_data_size,
            void **con_cls);

        static size_t HandleUnescape(
            void * cls,
            struct MHD_Connection *c,
            char *s);

        static int HandleAudioTrackRequest(
            HttpServer* server,
            MHD_Response*& response,
            MHD_Connection* connection,
            std::vector<std::string>& pathParts);

        static int HandleThumbnailRequest(
            HttpServer* server,
            MHD_Response*& response,
            MHD_Connection* connection,
            std::vector<std::string>& pathParts);

        struct MHD_Daemon *httpServer;
        Context& context;
        volatile bool running;
        std::condition_variable exitCondition;
        std::mutex exitMutex;
};
