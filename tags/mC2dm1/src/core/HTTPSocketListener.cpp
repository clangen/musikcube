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

#include "HTTPSocketListener.h"

#include <iostream>
#include <stdlib.h>
#include "boost/ref.hpp"

doe::HTTP::SocketListener::SocketListener(void):iPort(3456),oListenAcceptor(NULL){

}

doe::HTTP::SocketListener::~SocketListener(void){
    // 1. Exit all Responder-threads
    for(std::vector<doe::HTTP::Responder*>::iterator oResponder=this->aResponders.begin();oResponder!=this->aResponders.end();++oResponder){
        (*oResponder)->exitResponder();
    }

    // 2. Join the threads (wait for all threads to exit)
    this->aThreads.join_all();

    // 3. Delete the Responders (no need to delete the vector, it will self-terminate)
    for(std::vector<doe::HTTP::Responder*>::iterator oResponder=this->aResponders.begin();oResponder!=this->aResponders.end();++oResponder){
        delete (*oResponder);
    }

}

bool doe::HTTP::SocketListener::initialize(){


    return true;

}

void doe::HTTP::SocketListener::operator ()(){
    this->initialize();
    this->runThread();
}


void doe::HTTP::SocketListener::runThread(){

    boost::asio::deadline_timer oTimer(this->oIOService);
    // Add the listener
    this->oListenAcceptor    = new boost::asio::ip::tcp::acceptor(this->oIOService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), this->iPort));

    try{
        while(!this->exit()){

            oTimer.expires_from_now(boost::posix_time::seconds(3));
            boost::system::error_code oError;

            boost::asio::ip::tcp::socket *oNewSocket    = new boost::asio::ip::tcp::socket(this->oIOService);
            oListenAcceptor->accept(*oNewSocket,oError);

            if(oError){
                std::cout << "ERROR.. Timeout" << std::endl;
                delete oNewSocket;
            }else{

                // Do stuff with it
                // First find a responder
                doe::HTTP::Responder *oResponder=NULL;
                int iTemp(0);
                for(std::vector<doe::HTTP::Responder*>::iterator oCheckResponder=this->aResponders.begin();oCheckResponder!=this->aResponders.end() && oResponder==NULL;++oCheckResponder){
                    try{
                        boost::try_mutex::scoped_try_lock oCheckLock( (*oCheckResponder)->oBusyLock,false);
                        if(oCheckLock.try_lock()){
                            oResponder    = (*oCheckResponder);
                        }
                    }catch(...){
    //                                    std::wcout << _T("ERROR") << iTemp << std::endl;
                    }
                    ++iTemp;
                }

                if(oResponder==NULL){
                    // None found, create a new thread.
                    oResponder    = new doe::HTTP::Responder(this,oNewSocket);
                    this->aResponders.push_back(oResponder);

                    // Startup the thread
                    this->aThreads.create_thread(boost::ref(*oResponder));
                }else{
                    // Found one
                    oResponder->setSocket(oNewSocket);
                }
            }
        }
    }

    catch(...){
    }

}

