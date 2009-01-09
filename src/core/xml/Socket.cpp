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
#include <core/xml/Socket.h>

using namespace musik::core::xml;
    
SocketReader::SocketReader(boost::asio::ip::tcp::socket &socket)
 :socket(socket)
 ,exited(false)
 ,bufferSize(0)
{

}

SocketReader::~SocketReader(void){
}

bool SocketReader::Read(long recommendedBytes){
    boost::system::error_code error;
    this->bufferSize    = this->socket.read_some(boost::asio::buffer(this->buffer),error);

    if(!error){
        return true;
    }

    this->exited    = true;
    return false;
}

char* SocketReader::Buffer(){
    return this->buffer.c_array();
}

long SocketReader::BufferSize(){
    return this->bufferSize;
}

bool SocketReader::Exited(){
    return this->exited;
}


//////////////////////////////////////////////////////////////////////////////

SocketWriter::SocketWriter(boost::asio::ip::tcp::socket &socket)
 :socket(socket)
 ,bufferSize(0)
 ,exited(false)
 ,maxBufferSize(1024)
{

}

SocketWriter::~SocketWriter(void){
}

bool SocketWriter::Write(const char* buffer,long bytes){
    if(this->bufferSize+bytes>this->maxBufferSize){
        this->Flush();
    }

    CopyMemory(this->buffer.c_array()+this->bufferSize, buffer, bytes);
    this->bufferSize    += bytes;

    return !this->exited;
}

void SocketWriter::Flush(){
	if(this->bufferSize && !this->Exited()){
        boost::system::error_code error;
        try{
            boost::asio::write(this->socket,boost::asio::buffer(this->buffer,this->bufferSize));
        }catch(...){
            this->exited    = true;
        }
        this->bufferSize    = 0;
    }
}

bool SocketWriter::Exited(){
    return this->exited;
}

