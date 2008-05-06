//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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
#include "boost/asio.hpp"
#include "StdString.h"
#include <vector>
#include "boost/thread/thread.hpp"

namespace doe{
    namespace HTTP{
        class SocketListener;
    }
}

#define HTTP_MIN_CLIENTS    5

#include "typedefs.h"
#include "HTTPResponder.h"
#include "ThreadHelper.h"


namespace doe{
    namespace HTTP{
        class SocketListener : public doe::ThreadHelper{
            public:
                SocketListener(void);
                ~SocketListener(void);
                bool initialize();
                void runThread();
                void operator ()();
                CStdString sDB;

//                void exit(bool bNewExit);

            private:
//                sockaddr_in oListenSocket;
                int iPort;
//                SOCKET iSocket;
//                int iMaxSocket;
//                fd_set aNewSockets;

                boost::asio::io_service oIOService;
                boost::asio::ip::tcp::acceptor *oListenAcceptor;

                std::vector<doe::HTTP::Responder*> aResponders;
                boost::thread_group aThreads;

                SocketListener( const doe::HTTP::SocketListener& ){};
                const SocketListener& operator=( const doe::HTTP::SocketListener& ){};

            protected:
//                boost::mutex oExitMutex;
//                boost::mutex oNotificationMutex;
//                boost::condition oNotify;
//                bool exit();
//                bool bExit;

        };
    }
}
