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

#include <core/HTTPResponder.h>
#include <core/ThreadHelper.h>
#include <core/HTTPServer.h>
#include <core/HTTPRequestParser.h>
#include <core/Common.h>

#include <iostream>
#include <stdlib.h>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>

using namespace musik::core;


HTTP::Responder::Responder(HTTP::Server *oParent,boost::asio::io_service &oIOService) : oSocket(oIOService),oThread(NULL){
    this->oParent    = oParent;
    this->sDB        = this->oParent->sDB;
}

HTTP::Responder::~Responder(void){
    if(this->oThread){
        this->ExitResponder();

        this->oThread->join();
        delete this->oThread;
    }
}

bool HTTP::Responder::Startup(){
    // start the thread
    this->oThread    = new boost::thread(boost::bind(&HTTP::Responder::ThreadLoop,this));
    return true;
}

void HTTP::Responder::ThreadLoop(){
    boost::try_mutex::scoped_lock oLock(this->oWaitMutex);
    HTTP::RequestParser oRequest;

    this->InitDB(256);

    while(!this->Exit()){
        // Wait for start signal
        this->oWaitCondition.wait(oLock);

        // OK, GO
        if(!this->Exit()){

            std::string sRequest;

            // First, read request
            if(this->readRequest(sRequest)){
                // Second, Find out what is requested.
                oRequest.parse(sRequest);
                utfstring sFileName;
                int iFileSize(0);

                if(this->getFileName(sFileName,iFileSize,oRequest,this->oDB)){
                    char buffer[1024];
                    int iBuffersize(0);


                    FILE *oFile    = _wfopen(sFileName.c_str(),UTF("rb"));

                    // Send header
                    std::string sHeader( boost::str( boost::format("HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\nContent-Length: %1%\r\n\r\n")%iFileSize ));

                    //send(this->iSocket,sHeader.c_str(),sHeader.size(),0);
                    try{
                        boost::asio::write(this->oSocket,boost::asio::buffer(sHeader.c_str(),sHeader.size()));
                    }
                    catch(...){
                    }

                    while(!feof(oFile) && oFile && !this->Exit()){
                        iBuffersize=0;
                        while(iBuffersize<1024 && !feof(oFile)){
                            iBuffersize+=fread(buffer,sizeof(char),1024-iBuffersize,oFile);
                        }
                            // send buffer
    //                    send(this->iSocket,buffer,iBuffersize,0);
                        boost::asio::write(this->oSocket,boost::asio::buffer(buffer,iBuffersize));

                    }
                    fclose(oFile);
                }else{
                    std::string sSend("HTTP/1.1 404 OK\r\nContent-Type: text/html\r\n\r\n<html><body bgcolor=\"#ff0000\">testar: ");
                    sSend    += musik::core::ConvertUTF8(sFileName);
                    sSend    += "<br><pre>";
                    sSend    += sRequest;
                    sSend    += "</pre></body></html>";

                    boost::asio::write(this->oSocket,boost::asio::buffer(sSend.c_str(),sSend.size()));
                }
            }
        }

        this->closeSocket();

        // Everything is finished, free this responder.
        if(!this->Exit()){
            this->oParent->freeResponder(this);
        }
    }
}


bool HTTP::Responder::getFileName(utfstring &sFileName,int &iFileSize,const HTTP::RequestParser &oRequest,sqlite3 *oDB){
    if(oRequest.aSplitPath.size()>0){
        std::string sRequestId    = oRequest.aSplitPath.front();
        int iRequestId    = atoi(sRequestId.c_str());

        sqlite3_stmt *oStmt;
        int iError    = sqlite3_prepare_v2(oDB,"SELECT s.id,(f.fullpath || '/' || s.filename) AS file,s.filesize FROM songs s,folders f WHERE s.id=? AND s.folder_id=f.id",-1,&oStmt,NULL);
        sqlite3_bind_int(oStmt,1,iRequestId);

        if(sqlite3_step(oStmt)==SQLITE_ROW){

            sFileName.assign( (utfchar *)sqlite3_column_text16(oStmt,1) );
            iFileSize    = sqlite3_column_int(oStmt,2);

            sqlite3_reset(oStmt);
            sqlite3_finalize(oStmt);
            return true;
        }
        sqlite3_reset(oStmt);
        sqlite3_finalize(oStmt);

    }
    return false;
}

bool HTTP::Responder::readRequest(std::string &sRequest){
    char sBuffer[512];
    int iBytesRead(0);

    while(!this->Exit()){

        boost::system::error_code oError;

        iBytesRead    = this->oSocket.read_some(boost::asio::buffer(sBuffer,512),oError);

        if(oError){
            return false;
        }

        sRequest.append(sBuffer,iBytesRead);
        if(sRequest.substr(sRequest.size()-4)=="\r\n\r\n"){
            return true;
        }
    }

    return false;

}

void HTTP::Responder::closeSocket(){
    // Close socket
/*    int iError    = shutdown(this->iSocket,SHUT_RDWR);
    ASSERT(iError==0);
    iError    = closesocket(this->iSocket);
    ASSERT(iError==0);

    this->iSocket    = -1;
*/
    this->oSocket.close();
}


void HTTP::Responder::ExitResponder(){
    this->Exit(true);

    boost::try_mutex::scoped_try_lock oWaitLock(this->oWaitMutex,false);
    if(oWaitLock.try_lock()){
        this->oWaitCondition.notify_all();
    }

}


