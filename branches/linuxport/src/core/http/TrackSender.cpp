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
#include <core/filestreams/Factory.h>


using namespace musik::core::http;

TrackSender::TrackSender()
 :watchPath("track") 
{
}


const char* TrackSender::WatchPath(){
    return this->watchPath.c_str();
}

void TrackSender::Execute(musik::core::http::IResponder* responder,musik::core::http::IRequestParser* request,musik::core::ITrack* track){
    #define BUFFER_SIZE 4096
    char buffer[BUFFER_SIZE];
    std::size_t buffersize(0);

    // TODO: Should rewrite this in a multiplatform manner
    if(track && responder){
        musik::core::filestreams::FileStreamPtr file( musik::core::filestreams::Factory::OpenFile(track->URL()) );

        if(file){
            const char *seekOffset   = request->Attribute("seek");
            if(seekOffset){
                try{
                    long seekTo = boost::lexical_cast<long>(seekOffset);
                    file->SetPosition(seekTo);
                }
                catch(...){
                }
            }

            // Send header
            std::string header( "HTTP/1.1 200 OK\r\nContent-Length: " );
            header.append( UTF_TO_UTF8(track->GetValue("filesize")) + "\r\n" );

            // Send content type as filename extension
            // TODO: Fix to send mimetype
            header.append( "Content-Type: " + UTF_TO_UTF8(file->Type()) + "\r\n" );
            header.append("\r\n");

            responder->SendContent(header.c_str(),header.size());

            while(!file->Eof() && !responder->Exited()){
                buffersize=0;
                while(buffersize<BUFFER_SIZE && !file->Eof()){
                    buffersize += file->Read(buffer,BUFFER_SIZE-buffersize);
                }

                // send buffer
                responder->SendContent(buffer,buffersize);

            }
        }
    }
}

void TrackSender::Destroy(){
    delete this;
}
