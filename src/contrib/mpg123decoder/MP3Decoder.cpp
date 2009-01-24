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


#define MP3_IN_BUFFSIZE     4096
#define MP3_OUT_BUFFSIZE     32768


MP3Decoder::MP3Decoder(void)
 :cachedLength(0)
 ,decoder(NULL)
 ,cachedRate(44100)
 ,cachedChannels(2)
 ,fileStream(NULL)
 ,lastMpg123Status(MPG123_NEED_MORE)
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

double MP3Decoder::SetPosition(double second,double totalLength){

    off_t seekToFileOffset(0);
    off_t seekToSampleOffset( (second*(double)this->cachedRate) );

    off_t *indexOffset(NULL);
    off_t indexSet(0);
    size_t indexFill(0);
    int err=mpg123_index(this->decoder,&indexOffset,&indexSet,&indexFill);

    // Try to feed it some to be able to seek
    off_t seekedTo(0);
    int feedMore(20);
    while( (seekedTo=mpg123_feedseek(this->decoder,seekToSampleOffset,SEEK_SET,&seekToFileOffset))==MPG123_NEED_MORE && feedMore>0){
        if(!this->Feed()){
            feedMore=0;
        }
        feedMore--;
    }

    if(seekedTo>=0){
        if(this->fileStream->SetPosition(seekToFileOffset)){
            return second;
        }
    }

    return -1;
}

bool    MP3Decoder::GetBuffer(IBuffer *buffer){
    long nofSamplesMax = 1024*2;

    // Start by setting the buffer format
    buffer->SetChannels(this->cachedChannels);
    buffer->SetSampleRate(this->cachedRate);
    buffer->SetSamples(nofSamplesMax);

    unsigned char* currentBuffer   = (unsigned char*)(buffer->BufferPointer());

    bool done(false);

    size_t bytesWritten(0);
    do{
        int rc  = mpg123_read(this->decoder,currentBuffer,nofSamplesMax*this->sampleSize,&bytesWritten);

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
                return false;
                break;
            case MPG123_NEW_FORMAT:
                int encoding(0);
                int gfCode  = mpg123_getformat(this->decoder,&this->cachedRate,&this->cachedChannels,&encoding);
                if( gfCode==MPG123_OK ){
                    this->sampleSize    = this->cachedChannels*sizeof(float);

                    // Format should not change
                    mpg123_format_none(this->decoder);  
                    // Force the encoding to float32
                    int e=mpg123_format(this->decoder,this->cachedRate,this->cachedChannels,MPG123_ENC_FLOAT_32);

                    buffer->SetChannels(this->cachedChannels);
                    buffer->SetSampleRate(this->cachedRate);
                    buffer->SetSamples(nofSamplesMax);
                }

                break;
        }

    }while(bytesWritten==0 && !done);

    buffer->SetSamples( bytesWritten/this->sampleSize );


    return bytesWritten>0;
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

            mpg123_param(this->decoder,MPG123_ADD_FLAGS,MPG123_FUZZY|MPG123_SEEKBUFFER,0);

            // Set filelength to decoder for better seeking
            mpg123_set_filesize(this->decoder,this->fileStream->Filesize());
            return true;
        }
    }
    return false;
}



