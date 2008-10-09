//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, Daniel Önnerby
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

#include "MP3decoder.h"


MP3Decoder::MP3Decoder(void)
 :cachedLength(0)
 ,decoder(NULL)
{
    this->decoder   = mpg123_new(NULL,NULL);
}


MP3Decoder::~MP3Decoder(void){
    if(this->decoder){
        mpg123_delete(this->decoder);
        this->decoder   = NULL;
    }
}


void    MP3Decoder::Destroy(){
    delete this;
}


bool    MP3Decoder::GetLength(unsigned long * MS){
    if(this->cachedLength){
        *MS = this->cachedLength;
        return true;
    }
    return false;
}


bool    MP3Decoder::SetPosition(unsigned long * MS){
    return false;
}


bool    MP3Decoder::SetState(unsigned long State){
    return false;
}


bool    MP3Decoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels){
    return false;
}


bool    MP3Decoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples){
    return false;
}


bool    MP3Decoder::Open(musik::core::filestreams::IFileStream *fileStream){
    if(this->decoder && fileStream){
        return true;
    }
    return false;
}



