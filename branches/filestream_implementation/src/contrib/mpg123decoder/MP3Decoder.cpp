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
 ,cachedRate(44100)
 ,cachedChannels(2)
 ,fileStream(NULL)
{
    this->decoder       = mpg123_new(NULL,NULL);
    this->sampleSize    = this->cachedChannels*sizeof(float);
}


MP3Decoder::~MP3Decoder(void){
    if(this->decoder){

        mpg123_close(this->decoder);
        mpg123_delete(this->decoder);

        this->decoder   = NULL;

    }
}


void    MP3Decoder::Destroy(){
    delete this;
}


bool    MP3Decoder::GetLength(unsigned long * MS){
    if(!this->cachedLength){
        this->GuessLength();
    }

    if(this->cachedLength){
        *MS = (this->cachedLength/cachedRate);
        return true;
    }

    *MS = 2*60*1000;

    return true;
}

void    MP3Decoder::GuessLength(){
    this->cachedLength  = mpg123_length(this->decoder);
    if(this->cachedLength<0){
        this->cachedLength  = 0;
    }
}


bool    MP3Decoder::SetPosition(unsigned long * MS){
    return false;
}


bool    MP3Decoder::SetState(unsigned long State){
    return true;
}


bool    MP3Decoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels){
    *SampleRate     = this->cachedRate;
    *Channels       = this->cachedChannels;
    return true;
}


bool    MP3Decoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples){

    static float buffer[1200];
    int nofSamplesMax(1200/this->cachedChannels);
    *ppBuffer   = buffer;

    unsigned char* currentBuffer   = (unsigned char*)(&buffer);
    unsigned long bytesLeft(nofSamplesMax*this->sampleSize);
    bool done(false);

    do{
        size_t bytesWritten(0);
        int rc  = mpg123_read(this->decoder,currentBuffer,bytesLeft,&bytesWritten);
        bytesLeft-=(unsigned long)bytesWritten;
        currentBuffer+=bytesWritten;

        switch(rc){
            case MPG123_DONE:
                // Stream is finished
                done=true;
                break;
            case MPG123_NEED_MORE:
                if(!this->Feed()){
                    done=true;
                }
                break;
            case MPG123_ERR:
    			*ppBuffer	= NULL;
                *NumSamples = 0;
                return false;
                break;
        }

    }while(bytesLeft>0 && !done);

    *NumSamples = nofSamplesMax-bytesLeft/this->sampleSize;

    if(*NumSamples==0){
        *ppBuffer	= NULL;
        if(done){
            return false;
        }
    }
    return true;
}

bool MP3Decoder::Feed(){
    // Feed stuff to mpg123
    if(this->fileStream){
        unsigned char buffer[STREAM_FEED_SIZE];
        musik::core::filestreams::PositionType bytesRead    = this->fileStream->Read(&buffer,STREAM_FEED_SIZE);
        if(bytesRead){
            if(mpg123_feed(this->decoder,buffer,bytesRead)==MPG123_OK){
                return true;
            }
        }
    }
    return false;
}

bool    MP3Decoder::Open(musik::core::filestreams::IFileStream *fileStream){
    if(this->decoder && fileStream){
        this->fileStream    = fileStream;

        if(mpg123_open_feed(this->decoder)==MPG123_OK){

            // Set the format
            int encoding(0);
            bool continueFeed(true);

            // Loop until we have a format
            while(continueFeed){
                continueFeed    = continueFeed && this->Feed();
                if(continueFeed){
                    if(mpg123_getformat(this->decoder,&this->cachedRate,&this->cachedChannels,&encoding)==MPG123_OK){
                        continueFeed    = (this->cachedRate==0);
                    }
                }
            }



            // Format should not change
            mpg123_format_none(this->decoder);  
            // Force the encoding to float32
            int e=mpg123_format(this->decoder,this->cachedRate,this->cachedChannels,MPG123_ENC_FLOAT_32);
            int tjo = mpg123_format_support(this->decoder,44100,MPG123_ENC_FLOAT_32);
            int tjo2 = mpg123_format_support(this->decoder,44100,MPG123_ENC_FLOAT_32);
/*            while(this->cachedLength==0){
                this->Feed();
                this->GuessLength();
            }*/
        }
        
        return true;
    }
    return false;
}



