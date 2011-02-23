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

#include <core/config_filesystem.h>
#include <core/server/Connection.h>
#include <core/db/Connection.h>

#include <core/Indexer.h>
#include <core/http/Server.h>
#include <core/server/User.h>
#include <core/server/UserSession.h>

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>

#include <vector>
#include <sigslot/sigslot.h>

//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////
///\brief
///musik::core::Server is the main class for the musikServer
///
///the Server contains everything from an Indexer,
///a musik::core::server::Connection (Library::Base derivate) for each connected user
///and a HTTP server for streaming tracks.
///
///\see
///Indexer | http::Server | server::Connection
//////////////////////////////////////////
class MUSIK_EXPORT Server{
    public:
        // Methods
        Server(unsigned int port,unsigned int httpPort);
        ~Server(void);
        bool Startup();

        bool CreateUser(const utfstring username,const utfstring plainTextPassword,const utfstring name);
        bool DeleteUser(const utfstring username);

        server::UserVector AllUsers();
        server::UserSessionVector ConnectedUserSessions();

        bool UserIsAuthorized(const char *authorizationKey,boost::asio::ip::tcp::socket &socket);
    public:
        // Events
        typedef sigslot::signal0<> UserUpdatedEvent;
        UserUpdatedEvent UsersUpdated;
        UserUpdatedEvent UserSessionsUpdated;

    public:
        Indexer indexer;    
        http::Server httpServer;

    private:
        // Methods
        void Exit();
        bool Exited();
        void ThreadLoop();
        void AcceptConnection(const boost::system::error_code& error);
        void SetNextConnection();
        void CleanupConnections();

        utfstring ServerIdentifier();

        friend class server::Connection;
        server::UserPtr GetUser(std::string username);

        bool RemoveUserSession(server::UserSessionPtr userSession);
        bool AddUserSession(server::UserSessionPtr userSession);

    private:
        // Variables
        boost::asio::io_service ioService;
        boost::asio::ip::tcp::acceptor acceptor;

        bool exitThread;
        boost::mutex serverMutex;
        boost::thread_group threads;

        musik::core::server::ConnectionVector connections;
        musik::core::server::ConnectionPtr nextConnection;

        server::UserSessionMap connectedUsers;

        db::Connection db;
};

typedef boost::shared_ptr<Server> ServerPtr;

//////////////////////////////////////////////////////////////////////////////
} }


