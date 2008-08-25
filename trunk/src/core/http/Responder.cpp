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

#include <core/http/Responder.h>
#include <core/http/Server.h>
#include <core/http/RequestParser.h>
#include <core/Common.h>

#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>

using namespace musik::core::http;


Responder::Responder(Server &server,boost::asio::io_service &ioService,utfstring dbFilename) 
 :socket(ioService)
 ,thread(NULL)
 ,server(server)
{
    this->db.Open(dbFilename,0,256);
}

Responder::~Responder(void){
    if(this->thread){
        this->Exit();

        this->thread->join();
        delete this->thread;
    }
}

bool Responder::Startup(){
    // start the thread
    this->thread    = new boost::thread(boost::bind(&Responder::ThreadLoop,this));
    return true;
}

void Responder::ThreadLoop(){
    RequestParser requester;

//    this->InitDB(256);

    while(!this->Exited()){
        // Wait for start signal
        if(!this->socket.is_open()){
            boost::mutex::scoped_lock lock(this->server.mutex);
            this->waitCondition.wait(lock);
        }

        // OK, GO
        if(!this->Exited()){

            std::string request;

            // First, read request
            if(this->ReadRequest(request)){
                // Second, Find out what is requested.
                requester.Parse(request);
                utfstring fileName;
                int fileSize(0);

                if(this->GetFileName(fileName,fileSize,requester)){
                    char buffer[1024];
                    int buffersize(0);


                    FILE *file    = _wfopen(fileName.c_str(),UTF("rb"));

                    // Send header
                    std::string header( boost::str( boost::format("HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\nContent-Length: %1%\r\n\r\n")%fileSize ));

                    //send(this->iSocket,sHeader.c_str(),sHeader.size(),0);
                    try{
                        boost::asio::write(this->socket,boost::asio::buffer(header.c_str(),header.size()));
                    }
                    catch(...){
                    }

                    while(!feof(file) && file && !this->Exited()){
                        buffersize=0;
                        while(buffersize<1024 && !feof(file)){
                            buffersize += fread(buffer,sizeof(char),1024-buffersize,file);
                        }
                            // send buffer
    //                    send(this->iSocket,buffer,iBuffersize,0);
                        boost::asio::write(this->socket,boost::asio::buffer(buffer,buffersize));

                    }
                    fclose(file);
                }else{
                    std::string send("HTTP/1.1 404 OK\r\nContent-Type: text/html\r\n\r\n<html><body bgcolor=\"#ff0000\">testar: ");
                    send    += musik::core::ConvertUTF8(fileName);
                    send    += "<br><pre>";
                    send    += request;
                    send    += "</pre></body></html>";

                    boost::asio::write(this->socket,boost::asio::buffer(send.c_str(),send.size()));
                }
            }
        }

        this->CloseSocket();

        // Everything is finished, free this responder.
        if(!this->Exited()){
            this->server.FreeResponder(this);
        }
    }
}


bool Responder::GetFileName(utfstring &fileName,int &fileSize,const RequestParser &request){
    if(request.splitPath.size()>0){
        std::string sRequestId  = request.splitPath.front();
        int requestId           = atoi(sRequestId.c_str());

        musik::core::db::CachedStatement stmt("SELECT t.id,(p.path||f.relative_path||'/'||t.filename) AS file,t.filesize FROM tracks t,folders f,paths p WHERE t.id=? AND t.folder_id=f.id AND f.path_id=p.id",this->db);

        stmt.BindInt(0,requestId);

        if(stmt.Step()==musik::core::db::Row){

            fileName.assign( stmt.ColumnTextUTF(1) );
            fileSize       = stmt.ColumnInt(2);

            return true;
        }
    }
    return false;
}

bool Responder::ReadRequest(std::string &request){
    char buffer[512];
    int bytesRead(0);

    while(!this->Exited()){

        boost::system::error_code error;

        bytesRead    = this->socket.read_some(boost::asio::buffer(buffer,512),error);

        if(error){
            return false;
        }

        request.append(buffer,bytesRead);
        if(request.substr(request.size()-4)=="\r\n\r\n"){
            return true;
        }
    }

    return false;

}

void Responder::CloseSocket(){
    // Close socket
/*    int iError    = shutdown(this->iSocket,SHUT_RDWR);
    ASSERT(iError==0);
    iError    = closesocket(this->iSocket);
    ASSERT(iError==0);

    this->iSocket    = -1;
*/
    this->socket.close();
}


void Responder::Exit(){
    boost::try_mutex::scoped_lock lock(this->server.mutex);
    this->exited    = true;
    
    this->CloseSocket();

    this->waitCondition.notify_all();

}


bool Responder::Exited(){
    boost::mutex::scoped_lock lock(this->server.mutex);
    return this->exited;
}
