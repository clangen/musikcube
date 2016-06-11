//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include <core/debug.h>
#include <core/audio/Stream.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/plugin/PluginFactory.h>

using namespace musik::core::audio;
using musik::core::PluginFactory;

static std::string TAG = "Stream";

Stream::Stream(unsigned int options)
: preferedBufferSampleSize(4096)
, options(options)
, decoderSampleRate(0)
, decoderSamplePosition(0)
{
    if ((this->options & NoDSP) == 0) {
        typedef PluginFactory::DestroyDeleter<IDSP> Deleter;
        this->dsps = PluginFactory::Instance().QueryInterface<IDSP, Deleter>("GetDSP");
    }

    this->LoadDecoderPlugins();
}

Stream::~Stream() {
}

StreamPtr Stream::Create(unsigned int options) {
    return StreamPtr(new Stream(options));
}

double Stream::SetPosition(double requestedSeconds) {
    double actualSeconds = this->decoder->SetPosition(requestedSeconds);

    if (actualSeconds != -1) {
        double rate = (double) this->decoderSampleRate;
        this->decoderSamplePosition = (uint64)(actualSeconds * rate);
    }

    return actualSeconds;
}

bool Stream::OpenStream(std::string uri) {
    musik::debug::info(TAG, "opening " + uri);

    /* use our file stream abstraction to open the data at the 
    specified URI */
    this->dataStream = musik::core::io::DataStreamFactory::OpenUri(uri.c_str());

    if (!this->dataStream) {
        musik::debug::err(TAG, "failed to open " + uri);
        return false;
    }

    /* find a DecoderFactory we can use for this type of data*/
    DecoderFactoryList::iterator factories = this->decoderFactories.begin();
    DecoderFactoryList::iterator end = this->decoderFactories.end();
    DecoderFactoryPtr decoderFactory;

    for ( ; factories != end && !decoderFactory; ++factories) {
        if ((*factories)->CanHandle(this->dataStream->Type())) {
            decoderFactory  = (*factories);
        }
    }

    if (!decoderFactory) {
        /* nothing can decode this type of file */
        musik::debug::err(TAG, "nothing could open " + uri);
        return false;
    }

    IDecoder *decoder = decoderFactory->CreateDecoder();
    if (!decoder) {
        /* shouldn't ever happen, the factory said it can handle this file */
        return false;
    }

    /* ask the decoder to open the data stream. if it returns true we're
    good to start pulling data out of it! */
    typedef PluginFactory::DestroyDeleter<IDecoder> Deleter;

    this->decoder.reset(decoder, Deleter());
    if (!this->decoder->Open(this->dataStream.get())) {
        musik::debug::err(TAG, "open ok, but decode failed " + uri);
        return false;
    }

    musik::debug::info(TAG, "about ready to play: " + uri);

    return true;
}

void Stream::OnBufferProcessedByPlayer(BufferPtr buffer) {
    this->RecycleBuffer(buffer);
}

BufferPtr Stream::GetNextBufferFromDecoder() {
    /* get a spare buffer, then ask the decoder for some data */
    BufferPtr buffer = this->GetEmptyBuffer();
    if (!this->decoder->GetBuffer(buffer.get())) {
        return BufferPtr(); /* return NULL */
    }

    /* remember the sample rate so we can calculate the current time-position */
    if (!this->decoderSampleRate) {
        this->decoderSampleRate = buffer->SampleRate();
    }

    /* offset, in samples */
    this->decoderSamplePosition += buffer->Samples();

    /* calculate the position (seconds) in the buffer */
    buffer->SetPosition(
        ((double) this->decoderSamplePosition) / 
        ((double) this->decoderSampleRate));

    return buffer;
}

BufferPtr Stream::GetNextProcessedOutputBuffer() {
    /* ask the decoder for the next buffer */
    BufferPtr currentBuffer = this->GetNextBufferFromDecoder();

    if(currentBuffer) {
        /* try to fill the buffer to its optimal size; if the decoder didn't return
        a full buffer, ask it for some more data. */
        bool moreBuffers = true;
        while (currentBuffer->Samples() < this->preferedBufferSampleSize && moreBuffers) {
            BufferPtr bufferToAppend = this->GetNextBufferFromDecoder();
            if (bufferToAppend) {
                currentBuffer->Append(bufferToAppend);
                this->RecycleBuffer(bufferToAppend);
            }
            else {
                moreBuffers = false;
            }
        }

        /* let DSP plugins process the buffer */
        if (this->dsps.size() > 0) {
            BufferPtr oldBuffer = this->GetEmptyBuffer();

            for (Dsps::iterator dsp = this->dsps.begin(); dsp != this->dsps.end(); ++dsp) {
                oldBuffer->CopyFormat(currentBuffer);
                currentBuffer->SetPosition(oldBuffer->Position());

                if ((*dsp)->Process(currentBuffer.get(), oldBuffer.get())) {
                    currentBuffer.swap(oldBuffer);
                }
            }

            this->RecycleBuffer(oldBuffer);
        }
    }

    return currentBuffer;
}

/* returns a previously used buffer, if one is available. otherwise, a
new one will be allocated. */
BufferPtr Stream::GetEmptyBuffer() {
    BufferPtr buffer;
    if (!this->recycledBuffers.empty()) {
        buffer = this->recycledBuffers.front();
        this->recycledBuffers.pop_front();
    }
    else {
        buffer = Buffer::Create();
    }

    return buffer;
}

/* marks a used buffer as recycled so it can be re-used later. */
void Stream::RecycleBuffer(BufferPtr oldBuffer) {
    this->recycledBuffers.push_back(oldBuffer);
}

double Stream::DecoderProgress() {
    if (this->dataStream) {
        long fileSize = this->dataStream->Length();
        long filePosition = this->dataStream->Position();

        if (fileSize && filePosition) {
            return ((double) filePosition) / ((double) fileSize);
        }
    }

    return 0;
}

void Stream::LoadDecoderPlugins() {
    PluginFactory::DestroyDeleter<IDecoderFactory> typedef Deleter;

    this->decoderFactories = PluginFactory::Instance()
        .QueryInterface<IDecoderFactory, Deleter>("GetDecoderFactory");
}
