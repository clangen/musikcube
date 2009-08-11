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

#include <core/http/Server.h>
#include <core/http/Responder.h>
#include <core/http/Responder.h>
#include <core/PluginFactory.h>
#include <core/http/TrackSender.h>
#include <core/http/AlbumCoverSender.h>

#include <boost/bind.hpp>


using namespace musik::core::http;


Server::Server(int port)
 :acceptor(ioService,boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
 ,thread(NULL)
 ,timer(ioService)
 ,exited(false)
 ,port(port)
 ,parent(NULL)
{
    // Check for plugins
    typedef IRequestPlugin PluginType;
    typedef boost::shared_ptr<PluginType> PluginPtr;
    typedef PluginFactory::DestroyDeleter<PluginType> Deleter;
    typedef std::vector<PluginPtr> PluginVector;
    //
    PluginVector plugins = PluginFactory::Instance().QueryInterface<PluginType, Deleter>("GetHTTPRequestPlugin");

    for(PluginVector::iterator plugin=plugins.begin();plugin!=plugins.end();++plugin){
        if((*plugin)->WatchPath()){
            this->requestPlugins[(*plugin)->WatchPath()]   = *plugin;
        }else{
            this->requestPlugins[""]   = *plugin;
        }
    }

    // Lets always add the TrackSender
    this->requestPlugins["track"]   = PluginPtr(new TrackSender());
    this->requestPlugins["cover"]   = PluginPtr(new AlbumCoverSender());

    // Set some options for the acceptor
    boost::asio::socket_base::reuse_address option(true);
    this->acceptor.set_option(option);

}

Server::~Server(){
    if(this->thread){
        this->Exit();
        this->thread->join();
        delete this->thread;
    }
}

bool Server::Startup(utfstring dbFilename){

    this->dbFilename    = dbFilename;

    // start the thread
    this->thread    = new boost::thread(boost::bind(&Server::ThreadLoop,this));
    return true;

}

void Server::ThreadLoop(){
    boost::system::error_code error;

    this->initAccept();
    this->initTimeout();

    this->ioService.run(error);
    
    this->Exit();

}


void Server::initAccept(){
    // Get a Responder
    this->waitingResponder  = this->GetResponder();

    // Accept a connection
    this->acceptor.async_accept(*this->waitingResponder->socket,boost::bind(&Server::handleAccept,this));
}

void Server::handleAccept(){
    this->waitingResponder->waitCondition.notify_all();
    this->initAccept();
}


void Server::initTimeout(){
    this->timer.expires_from_now(boost::posix_time::seconds(20));
    this->timer.async_wait(boost::bind(&Server::handleTimeout, this));
}

void Server::Exit(){
    boost::mutex::scoped_lock lock(this->mutex);
    this->exited    = true;
    this->ioService.stop();
}

bool Server::Exited(){
    boost::mutex::scoped_lock lock(this->mutex);
    return this->exited;
}

void Server::handleTimeout(){
    if(this->Exited()){
        this->ioService.stop();
    }else{
        this->initTimeout();
    }

}


ResponderPtr Server::GetResponder(){
    boost::mutex::scoped_lock lock(this->mutex);

    if(this->freeResponders.size()>0){
        ResponderPtr responder(this->freeResponders.front());
        this->freeResponders.pop();
        this->busyResponders.insert(responder);
        return responder;
    }else{
        ResponderPtr responder(new Responder(*this,this->ioService,this->dbFilename));
        responder->Startup();
        this->busyResponders.insert(responder);
        return responder;
    }
}

bool Server::FreeResponder(Responder *responder){
    boost::mutex::scoped_lock lock(this->mutex);

    ResponderSet::iterator tempResponder=this->busyResponders.begin();
    while(tempResponder!=this->busyResponders.end()){
        if(tempResponder->get()==responder){
            this->freeResponders.push(*tempResponder);
            this->busyResponders.erase(tempResponder);
            return true;
        }else{
            ++tempResponder;
        }
    }
    return false;
}


