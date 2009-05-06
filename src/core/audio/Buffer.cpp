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
#ifdef WIN32
#include "pch.hpp"
#else
#include <core/pch.hpp>
#endif

#include <core/audio/Buffer.h>

using namespace musik::core::audio;

//////////////////////////////////////////
///\brief
///Constructor
//////////////////////////////////////////
Buffer::Buffer(void)
 :buffer(NULL)
 ,sampleSize(0)
 ,internalBufferSize(0)
 ,sampleRate(44100)
 ,channels(2)
{
}

//////////////////////////////////////////
///\brief
///Destructor
//////////////////////////////////////////
Buffer::~Buffer(void)
{
    delete this->buffer;
}

//////////////////////////////////////////
///\brief
///Create a new Buffer
//////////////////////////////////////////
BufferPtr Buffer::Create(){
    return BufferPtr(new Buffer());
}

//////////////////////////////////////////
///\brief
///Get the samplerate of the buffer
//////////////////////////////////////////
long Buffer::SampleRate() const{
    return this->sampleRate;
}

//////////////////////////////////////////
///\brief
///Set the buffers samplerate
//////////////////////////////////////////
void Buffer::SetSampleRate(long sampleRate){
    this->sampleRate    = sampleRate;
}

//////////////////////////////////////////
///\brief
///Get the number of channels of the buffer
//////////////////////////////////////////
int Buffer::Channels() const{
    return this->channels;
}

//////////////////////////////////////////
///\brief
///Set the number of channels of the buffer
//////////////////////////////////////////
void Buffer::SetChannels(int channels){
    this->channels  = channels;
    this->ResizeBuffer();
}

//////////////////////////////////////////
///\brief
///Get the pointer to the real buffer.
///
///The pointer may change when you set any of the buffers
///properties like samplerate, samples and channels
//////////////////////////////////////////
float* Buffer::BufferPointer() const{
    return this->buffer;
}

//////////////////////////////////////////
///\brief
///Get the number of samples in the buffer
///
///To clairify, one sample = one sample for each channel
///and that means that one sample = sizeof(float)*channels bytes big
//////////////////////////////////////////
long Buffer::Samples() const{
    return this->sampleSize;
}

//////////////////////////////////////////
///\brief
///Set the number of samples in the buffer
//////////////////////////////////////////
void Buffer::SetSamples(long samples){
    this->sampleSize    = samples;
    this->ResizeBuffer();
}

//////////////////////////////////////////
///\brief
///Copies all the formats from one buffer to another
//////////////////////////////////////////
void Buffer::CopyFormat(BufferPtr fromBuffer){
    this->sampleSize    = fromBuffer->Samples();
    this->channels      = fromBuffer->Channels();
    this->sampleRate    = fromBuffer->SampleRate();
    this->ResizeBuffer();
}

//////////////////////////////////////////
///\brief
///Resize the internal buffer to match the formats
//////////////////////////////////////////
void Buffer::ResizeBuffer(){
    long requiredBufferSize( this->sampleSize * this->channels );
    if(requiredBufferSize>this->internalBufferSize){
        // Only resize when internalBufferSize if too small
        if(this->buffer){
            // Delete old buffer
            delete this->buffer;
            this->buffer    = NULL;
        }
        // Create a new buffer
        this->buffer    = new float[requiredBufferSize];
        this->internalBufferSize    = requiredBufferSize;
    }
}

//////////////////////////////////////////
///\brief
///How many bytes does this object take
//////////////////////////////////////////
long Buffer::Bytes() const{
    return this->internalBufferSize*sizeof(float);
}

//////////////////////////////////////////
///\brief
///What position in a track is this buffer (in seconds)
//////////////////////////////////////////
double Buffer::Position() const{
    return this->position;
}

//////////////////////////////////////////
///\brief
///Append another buffer to this one
//////////////////////////////////////////
bool Buffer::Append(BufferPtr appendBuffer){
    if(this->SampleRate()==appendBuffer->SampleRate() && this->Channels()==appendBuffer->Channels()){
        long newBufferSize      = (this->Samples()+appendBuffer->Samples())*this->channels;

        if(newBufferSize>this->internalBufferSize){
            // Internal buffer too small. We need to make a new buffer
            // Create a new buffer
            float *newBuffer    = new float[newBufferSize];

            CopyFloat(newBuffer,this->buffer,this->sampleSize*this->channels);

            float *dst  = &newBuffer[this->sampleSize*this->channels];
            float *src  = appendBuffer->BufferPointer();
            long si     = appendBuffer->Samples()*this->channels;
            CopyFloat(dst,src,si);

            if(this->buffer){
                // Delete old buffer
                delete this->buffer;
            }
            // Set the new buffer
            this->buffer                = newBuffer;
            this->internalBufferSize    = newBufferSize;
        }else{
            // append the appendBuffer
            float *dst  = &this->buffer[this->sampleSize*this->channels];
            float *src  = appendBuffer->BufferPointer();
            long si     = appendBuffer->Samples()*this->channels;
//            CopyFloat( this->buffer + this->sampleSize*this->channels*sizeof(float),appendBuffer->BufferPointer(),appendBuffer->Samples()*this->channels);
            CopyFloat( dst,src,si);
        }

        this->sampleSize    = newBufferSize/this->channels;
        return true;

    }
    return false;
}
