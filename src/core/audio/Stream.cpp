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

#include "pch.hpp"

#include <core/audio/Stream.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/PluginFactory.h>

using namespace musik::core::audio;
using musik::core::PluginFactory;

Stream::Stream(unsigned int options)
 :preferedBufferSampleSize(4096)
 ,options(options)
 ,decoderSampleRate(0)
 ,decoderSamplePosition(0)
{
    if((this->options&NoDSP) == 0){
        typedef PluginFactory::DestroyDeleter<IDSP> Deleter;
        this->dsps = PluginFactory::Instance().QueryInterface<IDSP, Deleter>("GetDSP");
    }
}

Stream::~Stream(void)
{
}

StreamPtr Stream::Create(unsigned int options){
    return StreamPtr(new Stream(options));
}

BufferPtr Stream::NextBuffer() {
    return this->GetNextBuffer();
}

bool Stream::PreCache(){

    return false;
}

double Stream::SetPosition(double seconds){
    double newPosition  = this->decoder->SetPosition(seconds,0);

    if(newPosition!=-1){
        // Calculate new sample position
        this->decoderSamplePosition = (UINT64)(newPosition*((double)this->decoderSampleRate));
    }
    return newPosition;
}

bool Stream::OpenStream(utfstring uri) {
    /* use our file stream abstraction to open the data at the 
    specified URI */
    this->fileStream = musik::core::filestreams::Factory::OpenFile(uri.c_str());
    if (!this->fileStream) {
        return false;
    }

    /* find a DecoderFactory we can use for this type of data*/
    StreamHelper::DecoderFactoryPtr decoderFactory;
    StreamHelper::DecoderFactories::iterator factories = Stream::Helper()->decoderFactories.begin();
    StreamHelper::DecoderFactories::iterator end = Stream::Helper()->decoderFactories.end();

    for( ; factories != end && !decoderFactory; ++factories) {
        if((*factories)->CanHandle(this->fileStream->Type())){
            decoderFactory  = (*factories);
        }
    }

    if (!decoderFactory) {
        /* nothing can decode this type of file */
        return false;
    }

    IDecoder *decoder = decoderFactory->CreateDecoder();
    if (!decoder) {
        /* shouldn't ever happen, the factory said it can handle this file */
        return false;
    }

    typedef PluginFactory::DestroyDeleter<IDecoder> Deleter;

    /* ask the decoder to open the data stream. if it returns true we're
    good to start pulling data out of it! */
    this->decoder.reset(decoder, Deleter());
    if (!this->decoder->Open(this->fileStream.get())) {
        return false;
    }

    return true;
}

BufferPtr Stream::GetNextDecoderBuffer() {
    /* get a spare buffer, then ask the decoder for some data */
    BufferPtr buffer = this->NewBuffer();
    if(!this->decoder->GetBuffer(buffer.get())) {
        return BufferPtr();
    }

    /* remember the samplerate so we can calculate the current time-position */
    if(!this->decoderSampleRate){
        this->decoderSampleRate = buffer->SampleRate();
    }

    /* calculate the current offset, in samples */
    this->decoderSamplePosition += buffer->Samples();

    /* calculate the position (seconds) in the buffer */
    buffer->position = ((double)this->decoderSamplePosition) / ((double)this->decoderSampleRate);

    return buffer;
}

BufferPtr Stream::GetNextBuffer() {
    /* ask the decoder for the next buffer */
    BufferPtr currentBuffer = this->GetNextDecoderBuffer();

    if(currentBuffer) {
        /* try to fill the buffer to its optimal size; if the decoder didn't return
        a full buffer, ask it for some more data. */
        bool moreBuffers = true;
        while (currentBuffer->Samples() < this->preferedBufferSampleSize && moreBuffers) {
            BufferPtr bufferToAppend = this->GetNextDecoderBuffer();
            if (bufferToAppend) {
                currentBuffer->Append(bufferToAppend);
                this->DeleteBuffer(bufferToAppend);
            }
            else {
                moreBuffers = false;
            }
        }

        /* let DSP plugins process the buffer */
        if (this->dsps.size() > 0) {
            BufferPtr oldBuffer = this->NewBuffer();

            for (Dsps::iterator dsp = this->dsps.begin(); dsp != this->dsps.end(); ++dsp) {
                oldBuffer->CopyFormat(currentBuffer);
                oldBuffer->position = currentBuffer->position;

                if ((*dsp)->ProcessBuffers(currentBuffer.get(), oldBuffer.get())) {
                    currentBuffer.swap(oldBuffer);
                }
            }

            this->DeleteBuffer(oldBuffer);
        }

    }

    return currentBuffer;
}

BufferPtr Stream::NewBuffer(){
    BufferPtr buffer;
    if(!this->availableBuffers.empty()){
        buffer  = this->availableBuffers.front();
        this->availableBuffers.pop_front();
    }else{
        buffer  = Buffer::Create();
    }
    return buffer;
}

void Stream::DeleteBuffer(BufferPtr oldBuffer) {
    this->availableBuffers.push_back(oldBuffer);
}

Stream::StreamHelperPtr Stream::Helper(){
    static StreamHelperPtr helper;
    if(!helper){
        helper.reset(new Stream::StreamHelper());
    }
    return helper;
}

Stream::StreamHelper::StreamHelper() {
    PluginFactory::DestroyDeleter<IDecoderFactory> typedef Deleter;

    this->decoderFactories = PluginFactory::Instance()
        .QueryInterface<IDecoderFactory, Deleter>("GetDecoderFactory");
}

double Stream::DecoderProgress() {
    if (this->fileStream) {
        long fileSize = this->fileStream->Filesize();
        long filePosition = this->fileStream->Position();

        if (fileSize && filePosition) {
            return ((double) filePosition) / ((double) fileSize);
        }
    }

    return 0;
}
