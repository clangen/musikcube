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
#include <core/http/IResponder.h>
#include <core/db/Connection.h>

#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>


//////////////////////////////////////////////////////////////////////////////
// Forward declare
namespace musik{ namespace core{ namespace http{
    class Responder;
    class Server;
    class RequestParser;
    typedef boost::shared_ptr<Responder> ResponderPtr;
} } }
//////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////

namespace musik{ namespace core{ namespace http{

//////////////////////////////////////////////////////////////////////////////

class Responder : public IResponder{
    public:
        Responder(Server &server,boost::asio::io_service &ioService,utfstring dbFilename);
        virtual ~Responder();

        bool Startup();

        virtual void SendContent(const char* buffer,const std::size_t bufferSize);
        virtual bool Exited();
    private:
        friend class Server;

        void Exit();
        void ThreadLoop();
        bool ReadRequest(std::string &request);
        void CloseSocket();
//        bool GetFileName(utfstring &fileName,int &fileSize,const RequestParser &request);


        bool exited;
        musik::core::db::Connection db;

        boost::asio::ip::tcp::socket *socket;
        boost::condition waitCondition;
        boost::thread *thread;

        Server &server;

};

//////////////////////////////////////////////////////////////////////////////
} } }
//////////////////////////////////////////////////////////////////////////////


