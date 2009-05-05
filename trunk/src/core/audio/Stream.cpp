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
#include "../pch.hpp"
#include <core/audio/Stream.h>
#include <core/audio/IDecoderFactory.h>
#include <core/PluginFactory.h>

using namespace musik::core::audio;

Stream::Stream(unsigned int options)
 :preferedBufferSampleSize(4096)
 ,options(options)
 ,decoderSampleRate(0)
 ,decoderSamplePosition(0)
{
    // Get all DSPs
    // TODO: fixing PluginFactory
    if( (this->options&NoDSP)==0){
        this->dsps = musik::core::PluginFactory::Instance().QueryInterface<
            IDSP,
            musik::core::PluginFactory::DestroyDeleter<IDSP> >("GetDSP");
    }
}

Stream::~Stream(void)
{
}

StreamPtr Stream::Create(unsigned int options){
    return StreamPtr(new Stream(options));
}

BufferPtr Stream::NextBuffer(){

    // Decode a new buffer
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
/*
void Stream::SetMaxCacheLength(double seconds){
}

void Stream::SetPreferedBufferSampleSize(long samples){
}
*/
bool Stream::OpenStream(utfstring uri){

    // Open the filestream
    this->fileStream    = musik::core::filestreams::Factory::OpenFile(uri.c_str());
    if(!this->fileStream){
        return false;
    }


    // Look up what DecoderFactory to use
    StreamHelper::DecoderFactoryPtr decoderFactory;
    for(StreamHelper::DecoderFactories::iterator decoderFactoryIt=Stream::Helper()->decoderFactories.begin();decoderFactoryIt!=Stream::Helper()->decoderFactories.end() && !decoderFactory;++decoderFactoryIt){
        if( (*decoderFactoryIt)->CanHandle(this->fileStream->Type())){
            decoderFactory  = (*decoderFactoryIt);
        }
    }

    if(!decoderFactory){
        // We have failed to get a working decoderFactory
        return false;
    }

    // Create the decoder
    IDecoder *decoderPtr    =   decoderFactory->CreateDecoder();
    if(!decoderPtr){
        return false;
    }

    // Open the decoder
    typedef musik::core::PluginFactory::DestroyDeleter<IDecoder> IDecoderDeleter;
    this->decoder.reset(decoderPtr,IDecoderDeleter());
    if( !this->decoder->Open(this->fileStream.get()) ){
        return false;
    }

    return true;

}

BufferPtr Stream::GetNextDecoderBuffer(){
    // First get a buffer
    BufferPtr buffer = this->NewBuffer();

    // Get the buffer from the decoder
    if(!this->decoder->GetBuffer(buffer.get())){
        // Nothing to decode left, return a empty buffer
        return BufferPtr();
    }

    // We need to save the decoders samplerate to be able to calculate the current time-position
    if(!this->decoderSampleRate){
        this->decoderSampleRate = buffer->SampleRate();
    }

    // Calculate the current sample position
    this->decoderSamplePosition += buffer->Samples();

    // Save the position (seconds) in the buffer
    buffer->position    = ((double)this->decoderSamplePosition)/((double)this->decoderSampleRate);

    return buffer;

}

BufferPtr Stream::GetNextBuffer(){
    // First get the next decoded buffer
    BufferPtr currentBuffer = this->GetNextDecoderBuffer();

    if(currentBuffer){
        /////////////////////////////////////////////
        // Lets check if the buffer is too small
        bool moreBuffers(true);
        while(currentBuffer->Samples()<this->preferedBufferSampleSize && moreBuffers){
            BufferPtr appendBuffer = this->GetNextDecoderBuffer();
            if(appendBuffer){
                currentBuffer->Append(appendBuffer);
                this->DeleteBuffer(appendBuffer);
            }else{
                moreBuffers = false;
            }
        }
        /////////////////////////////////////////////

        BufferPtr oldBuffer     = this->NewBuffer();

        // Now lets loop through all DSP plugins
        for(Dsps::iterator dsp=this->dsps.begin();dsp!=this->dsps.end();++dsp){
            oldBuffer->CopyFormat(currentBuffer);
            oldBuffer->position = currentBuffer->position;

            if( (*dsp)->ProcessBuffers(currentBuffer.get(),oldBuffer.get()) ){
                // Success in processing DSP, swap the buffers
                currentBuffer.swap(oldBuffer);
            }
        }

        this->DeleteBuffer(oldBuffer);

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

void Stream::DeleteBuffer(BufferPtr oldBuffer){
    this->availableBuffers.push_back(oldBuffer);
}

Stream::StreamHelperPtr Stream::Helper(){
    static StreamHelperPtr helper;
    if(!helper){
        helper.reset(new Stream::StreamHelper());
    }
    return helper;
}

Stream::StreamHelper::StreamHelper(){
    // Look up all DecoderFactories
    this->decoderFactories = musik::core::PluginFactory::Instance().QueryInterface<
        IDecoderFactory,
        musik::core::PluginFactory::DestroyDeleter<IDecoderFactory> >("GetDecoderFactory");
}

double Stream::DecoderProgress(){
    if(this->fileStream){
        long fileSize       = this->fileStream->Filesize();
        long filePosition   = this->fileStream->Position();
        if(fileSize && filePosition){
            return ((double)filePosition)/((double)fileSize);
        }
    }
    return 0;
}
