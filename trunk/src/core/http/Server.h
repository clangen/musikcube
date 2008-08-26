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
#include <core/http/Responder.h>
#include <core/http/IRequestPlugin.h>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

#include <set>
#include <queue>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace http {

//////////////////////////////////////////////////////////////////////////////
class Server : private boost::noncopyable{
    public:
        Server(int port);
        ~Server();

        bool Startup(utfstring dbFilename);

        int port;

    private:
        void ThreadLoop();

        bool exited;
        void Exit();
        bool Exited();

        ResponderPtr GetResponder();

        void initAccept();
        void handleAccept();
        void initTimeout();
        void handleTimeout();

        boost::mutex mutex;

        boost::asio::io_service ioService;
        boost::asio::ip::tcp::acceptor acceptor;
        boost::asio::deadline_timer timer;

        boost::thread *thread;
        utfstring dbFilename;
    private:
        friend class Responder;
        void FreeResponder(Responder *responder);

//        
        typedef std::map<std::string,boost::shared_ptr<IRequestPlugin>> PluginPathMap;

        PluginPathMap requestPlugins;

        typedef std::set<ResponderPtr> ResponderSet;
        typedef std::queue<ResponderPtr> ResponderQueue;
        ResponderSet busyResponders;
        ResponderQueue freeResponders;
        ResponderPtr waitingResponder;
};

//////////////////////////////////////////////////////////////////////////////
} } } // musik::core::http
//////////////////////////////////////////////////////////////////////////////

