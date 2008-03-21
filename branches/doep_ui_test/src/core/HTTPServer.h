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

#include <core/config.h>

namespace musik{ namespace core{
    namespace HTTP{
        class Responder;
        typedef boost::shared_ptr<HTTP::Responder> ResponderPtr;
    }
} }

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <string>
#include <vector>
#include <queue>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include "ThreadHelper.h"

namespace musik{ namespace core{
    namespace HTTP{
        class Server : private boost::noncopyable,public ThreadHelper{
            public:
                Server(int iPort);
                ~Server(void);

                bool start();
                void ThreadLoop();

                utfstring sDB;
                int iPort;

            private:

                bool bExit;

                std::vector<HTTP::ResponderPtr> aBusyResponders;
                std::queue<HTTP::ResponderPtr> aFreeResponders;

                HTTP::ResponderPtr getResponder();

                HTTP::ResponderPtr oWaitingResponder;

                void initAccept();
                void handleAccept();
                void initTimeout();
                void handleTimeout();

                boost::mutex oResponderQueueMutex;

                boost::asio::io_service oIOService;
                boost::asio::ip::tcp::acceptor oAcceptor;
                boost::asio::deadline_timer oTimer;

                boost::thread *oThread;
            private:
                friend class HTTP::Responder;
                void freeResponder(HTTP::Responder *oResponder);
        };
    }
} }
