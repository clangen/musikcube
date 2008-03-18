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

#include "pch.hpp"

#include <core/HTTPServer.h>
#include <core/config.h>
#include <core/HTTPResponder.h>

#include <boost/bind.hpp>

using namespace musik::core;

HTTP::Server::Server(int iPort):oAcceptor(oIOService,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), iPort)),oThread(NULL),oTimer(oIOService),bExit(false){
    this->iPort    = iPort;
}

HTTP::Server::~Server(void){
    if(this->oThread){
        this->Exit(true);
        this->oThread->join();
        delete this->oThread;
    }
}

bool HTTP::Server::start(){

    // start the thread
    this->oThread    = new boost::thread(boost::bind(&HTTP::Server::ThreadLoop,this));

    return true;
}

void HTTP::Server::ThreadLoop(){
    boost::system::error_code oError;

    this->initAccept();
    this->initTimeout();

    this->oIOService.run(oError);
}

void HTTP::Server::initAccept(){
    // Get a Responder
    this->oWaitingResponder    = this->getResponder();
    // Accept a connection
    this->oAcceptor.async_accept(this->oWaitingResponder->oSocket,boost::bind(&HTTP::Server::handleAccept,this));
}

void HTTP::Server::handleAccept(){
    this->oWaitingResponder->oWaitCondition.notify_one();
    this->initAccept();
}


void HTTP::Server::initTimeout(){
    this->oTimer.expires_from_now(boost::posix_time::seconds(2));
    this->oTimer.async_wait(boost::bind(&HTTP::Server::handleTimeout, this));
}

void HTTP::Server::handleTimeout(){
    if(this->Exit()){
        this->oIOService.stop();
    }else{
        this->initTimeout();
    }

}


HTTP::ResponderPtr HTTP::Server::getResponder(){
    boost::mutex::scoped_lock oLock(this->oResponderQueueMutex);

    if(this->aFreeResponders.size()>0){
        HTTP::ResponderPtr oResponder(this->aFreeResponders.front());
        this->aFreeResponders.pop();
        this->aBusyResponders.push_back(oResponder);
        return oResponder;
    }else{
        HTTP::ResponderPtr oResponder(new HTTP::Responder(this,this->oIOService));
        oResponder->Startup();
        this->aBusyResponders.push_back(oResponder);
        return oResponder;
    }
}

void HTTP::Server::freeResponder(HTTP::Responder *oResponder){
    boost::mutex::scoped_lock oLock(this->oResponderQueueMutex);

    for(std::vector<HTTP::ResponderPtr>::iterator oCurrentResponder=this->aBusyResponders.begin();oCurrentResponder!=this->aBusyResponders.end();++oCurrentResponder){
        if(oResponder==(*oCurrentResponder).get()){
            this->aFreeResponders.push(HTTP::ResponderPtr(*oCurrentResponder));
            this->aBusyResponders.erase(oCurrentResponder);
            return;
        }
    }

}


