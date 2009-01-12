//////////////////////////////////////////////////////////////////////////////
// Copyright  2007, Daniel Önnerby
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
#include "pch.h"
#include "DSPEcho.h"

DSPEcho::DSPEcho()
 :internalBuffer(NULL)
 ,channels(0)
 ,bufferSampleSize(0)
 ,bufferPosition(0)
 ,sampleRate(0)
{

}

DSPEcho::~DSPEcho(){
    delete this->internalBuffer;
}


void DSPEcho::Destroy(){
    delete this;
}

void DSPEcho::SetBuffer(const IBuffer *inputBuffer){
    if(inputBuffer->Channels()!=this->channels || inputBuffer->SampleRate()!=this->sampleRate){

        this->channels          = inputBuffer->Channels();
        this->sampleRate        = inputBuffer->SampleRate();
        this->bufferSampleSize  = this->sampleRate;    // 1 second
        
        delete this->internalBuffer;
        this->internalBuffer    = new float[this->channels*this->bufferSampleSize];

        // empty the internal buffer
        for(long i=0;i<this->channels*this->bufferSampleSize;++i){
            this->internalBuffer[i] = 0.0f;
        }
    }
}


bool DSPEcho::ProcessBuffers(const IBuffer *inputBuffer,IBuffer *outputBuffer){
    this->SetBuffer(inputBuffer);
    
    // loop though the buffer and apply echo
    long bufferLength( this->channels*inputBuffer->Samples() );
    float *inBuffer     = inputBuffer->BufferPointer();
    float *outBuffer    = outputBuffer->BufferPointer();
    long internalBufferSize(this->channels*this->bufferSampleSize);

    long echo1distance(internalBufferSize-((long)(0.3*(double)this->sampleRate))*this->channels);
    long echo2distance(internalBufferSize-((long)(0.2*(double)this->sampleRate))*this->channels);

    for(long i(0);i<bufferLength;++i){
        float inSample(inBuffer[i]);
        float outSample(inSample);
        //add a 0.5 of 0.3 seconds away
        //outSample   += 0.5f*this->internalBuffer[(this->bufferPosition+echo1distance)%internalBufferSize];
        //add a 0.2 of 0.6 seconds away
        outSample   += 0.2f*this->internalBuffer[(this->bufferPosition+echo2distance)%internalBufferSize];

        // set the out buffer
        outBuffer[i]    = outSample;
        // Save the insample to internal buffer
        this->internalBuffer[this->bufferPosition]    = outSample;
        this->bufferPosition    = (this->bufferPosition+1)%internalBufferSize;
    }

    return true;
}

