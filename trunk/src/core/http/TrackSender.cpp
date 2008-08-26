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
#include <core/http/TrackSender.h>
#include <core/Common.h>


using namespace musik::core::http;

TrackSender::TrackSender()
 :watchPath("track") 
{
}


const char* TrackSender::WatchPath(){
    return this->watchPath.c_str();
}

void TrackSender::Execute(musik::core::http::IResponder* responder,const musik::core::http::IRequestParser* request,const musik::core::ITrack* track){
    #define BUFFER_SIZE 4096
    char buffer[BUFFER_SIZE];
    int buffersize(0);

    // TODO: Should rewrite this in a multiplatform manner
    if(track && responder){
        FILE *file    = _wfopen(track->GetValue("path"),UTF("rb"));

        if(file){
            // Send header
            std::string header( "HTTP/1.1 200 OK\r\nContent-Type: audio/mpeg\r\nContent-Length: " );
            header.append( musik::core::ConvertUTF8(track->GetValue("filesize")) );
            header.append("\r\n\r\n");

            responder->SendContent(header.c_str(),header.size());

            while(!feof(file) && file && !responder->Exited()){
                buffersize=0;
                while(buffersize<BUFFER_SIZE && !feof(file)){
                    buffersize += fread(buffer,sizeof(char),BUFFER_SIZE-buffersize,file);
                }

                // send buffer
                responder->SendContent(buffer,buffersize);

            }
            fclose(file);
        }
    }
}

void TrackSender::Destroy(){
    delete this;
}
