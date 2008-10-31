//////////////////////////////////////////////////////////////////////////////
// Copyright © 2007, mC2 team
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
#include "StdAfx.h"
#include "FLACDecoder.h"
#include <complex> 

FLACDecoder::FLACDecoder()
 :decoder(NULL)
 ,outputBufferSize(0)
 ,outputBuffer(NULL)
 ,channels(0)
 ,sampleRate(0)
 ,bps(0)
 ,totalSamples(0)
{
/*    this->flacCallbacks.read        = &FlacRead;
    this->flacCallbacks.seek        = &FlacSeek;
    this->flacCallbacks.tell        = &FlacTell;
    this->flacCallbacks.close       = &FlacClose;
*/
    this->decoder   = FLAC__stream_decoder_new();

}


FLACDecoder::~FLACDecoder(){
    if(this->decoder){
        FLAC__stream_decoder_delete(this->decoder);
        this->decoder   = NULL;
    }
    if(this->outputBuffer){
        delete this->outputBuffer;
        this->outputBuffer   = NULL;
    }

}


FLAC__StreamDecoderReadStatus    FLACDecoder::FlacRead(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[],size_t *bytes,void *clientData){
    size_t readBytes    = (size_t)((FLACDecoder*)clientData)->fileStream->Read(buffer,(long)(*bytes));
    *bytes  = readBytes;
    if(readBytes==0){
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    if(readBytes<0){
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__bool FLACDecoder::FlacEof(const FLAC__StreamDecoder *decoder, void *clientData){
    if( ((FLACDecoder*)clientData)->fileStream->Eof() ){
        return 1;
    }
    return 0;
}

FLAC__StreamDecoderSeekStatus FLACDecoder::FlacSeek(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *clientData){
    if( ((FLACDecoder*)clientData)->fileStream->SetPosition((long)absolute_byte_offset)){
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }
    // Unsuccessfull
    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}


FLAC__StreamDecoderTellStatus FLACDecoder::FlacTell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *clientData){
    *absolute_byte_offset   = (FLAC__uint64)((FLACDecoder*)clientData)->fileStream->Position();
    if(*absolute_byte_offset>=0){
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
    }
    return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
}

FLAC__StreamDecoderLengthStatus FLACDecoder::FlacFileSize(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *clientData){
    *stream_length   = (FLAC__uint64)((FLACDecoder*)clientData)->fileStream->Filesize();
    if(*stream_length<=0){
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    }
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

/*int      FLACDecoder::FlacClose(void *datasource){
    if( ((FLACDecoder*)datasource)->fileStream->Close() ){
        return 0;
    }
    return -1;
}*/


bool FLACDecoder::Open(musik::core::filestreams::IFileStream *fileStream){
    this->fileStream    = fileStream;

	FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(this->decoder,
        FLACDecoder::FlacRead,
        FLACDecoder::FlacSeek,
        FLACDecoder::FlacTell,
        FLACDecoder::FlacFileSize,
        FLACDecoder::FlacEof,
        FLACDecoder::FlacWrite,
        FLACDecoder::FlacMeta,
        FLACDecoder::FlacError,
        this);

    if(init_status == FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        // Process until we have metadata
        FLAC__stream_decoder_process_until_end_of_metadata(this->decoder);
        return true;    
    }

    return false;
}

void FLACDecoder::FlacError(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *clientData){

}

void FLACDecoder::FlacMeta(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *clientData){
    FLACDecoder *thisPtr    = (FLACDecoder*)clientData;

	if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        thisPtr->totalSamples   = metadata->data.stream_info.total_samples;
        thisPtr->sampleRate     = metadata->data.stream_info.sample_rate;
        thisPtr->channels       = metadata->data.stream_info.channels;
        thisPtr->bps            = metadata->data.stream_info.bits_per_sample;
    }
}


FLAC__StreamDecoderWriteStatus FLACDecoder::FlacWrite(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,const FLAC__int32 *const buffer[], void *clientData){

    FLACDecoder *thisPtr    = (FLACDecoder*)clientData;
    int nofSamples  = thisPtr->channels*frame->header.blocksize;

    // First, lets create a buffer
    // If there is already a buffer, delete it
    if(thisPtr->outputBufferSize==0){
        delete thisPtr->outputBuffer;
        thisPtr->outputBuffer   = NULL;
        thisPtr->outputBufferSize   = 0;
        thisPtr->outputBuffer       = new float[nofSamples];
    }
    if(thisPtr->outputBuffer && thisPtr->outputBufferSize>0){
        float *oldBuffer    = thisPtr->outputBuffer;
        thisPtr->outputBuffer       = new float[nofSamples+thisPtr->outputBufferSize];
        CopyMemory(thisPtr->outputBuffer, oldBuffer, thisPtr->outputBufferSize * sizeof(float));
        delete oldBuffer;
    }


    // What is the max amplitude
    float maxAmplitude  = pow(2.0f,(thisPtr->bps-1));

    // Convert the buffer (16bit int) to the outputBuffer (float)
	for(unsigned int i(0); i<frame->header.blocksize; ++i){
        for(int j(0); j<thisPtr->channels; ++j){
            thisPtr->outputBuffer[thisPtr->outputBufferSize]    = (((float)buffer[j][i])/maxAmplitude);
            thisPtr->outputBufferSize++;
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}


void		FLACDecoder::Destroy(void){
	delete this;
}

bool		FLACDecoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels){
    *SampleRate         = this->sampleRate;
    *Channels           = this->channels;

    if(*SampleRate && *Channels){
        return true;
    }

    return false;
}

bool		FLACDecoder::GetLength(unsigned long * MS){
    if(this->totalSamples && this->sampleRate){
        *MS = (this->totalSamples*1000)/this->sampleRate;
        return true;
    }

    return false;
}

bool		FLACDecoder::SetPosition(unsigned long * MS,unsigned long totalMS){
/*	double pos = *MS / 1000.0;

    if(ov_seekable(&this->oggFile)){
		if(!ov_time_seek(&this->oggFile, pos)){
			double time = ov_time_tell(&this->oggFile);
			*MS = (unsigned long)(time * 1000.0);
			return true;
		}
	}
*/
    return false;
}

bool        FLACDecoder::SetState(unsigned long State){
	return true;
}

bool        FLACDecoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples){
    if(this->outputBuffer && this->outputBufferSize>0){
        *ppBuffer   = this->outputBuffer;
        *NumSamples = this->outputBufferSize;
        this->outputBufferSize  = 0;
        return true;
    }

    if( FLAC__stream_decoder_process_single(this->decoder) ){
        if(this->outputBuffer && this->outputBufferSize>0){
            *ppBuffer   = this->outputBuffer;
            *NumSamples = this->outputBufferSize;
            this->outputBufferSize  = 0;
            return true;
        }
    }

    return false;
}
