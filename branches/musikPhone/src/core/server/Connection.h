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

//////////////////////////////////////////////////////////////////////////////
// Forward declare
namespace musik{ namespace core{
    class Server;
} }
//////////////////////////////////////////////////////////////////////////////


#include <core/config.h>
#include <core/Library/Base.h>
#include <core/db/Connection.h>
#include <core/server/UserSession.h>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <vector>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace server{

//////////////////////////////////////////////////////////////////////////////
// Forward declare
class Connection;
typedef boost::shared_ptr<Connection> ConnectionPtr;
typedef std::vector<ConnectionPtr> ConnectionVector;
//////////////////////////////////////////////////////////////////////////////


class Connection : public musik::core::Library::Base{
    public:
        Connection(boost::asio::io_service &ioService,musik::core::Server *server);
        ~Connection(void);
        utfstring GetInfo();

        bool Startup();

        boost::asio::ip::tcp::socket &Socket();
    private:
        // Methods:
        void ReadThread();
        void ParseThread();
        void WriteThread();

    protected:
        void CancelCurrentQuery( );
        virtual void Exit();

    private:
        boost::asio::ip::tcp::socket socket;
        boost::thread_group threads;

        musik::core::db::Connection db;

        UserSessionPtr userSession;
        std::string salt;
        boost::condition authCondition;

        musik::core::Server *server;
};


//////////////////////////////////////////////////////////////////////////////
} } }

