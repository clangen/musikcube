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

#include "FixedSizeStream.h"
#include <core/debug.h>
#include <core/sdk/IDecoderFactory.h>
#include <core/plugin/PluginFactory.h>

using namespace musik::core::audio;
using namespace musik::core::sdk;
using musik::core::PluginFactory;

static std::string TAG = "FixedSizeStream";

FixedSizeStream::FixedSizeStream(int samplesPerChannel, int bufferCount, unsigned int options)
: options(options)
, samplesPerChannel(samplesPerChannel)
, bufferCount(bufferCount)
, decoderSampleRate(0)
, decoderChannels(0)
, decoderSamplePosition(0)
, currentBuffer(Buffer::Create())
, dspBuffer(Buffer::Create())
{
    if ((this->options & NoDSP) == 0) {
        typedef PluginFactory::DestroyDeleter<IDSP> Deleter;
        this->dsps = PluginFactory::Instance().QueryInterface<IDSP, Deleter>("GetDSP");
    }

    this->LoadDecoderPlugins();
}

FixedSizeStream::~FixedSizeStream() {
}

StreamPtr FixedSizeStream::Create(int samplesPerChannel, int bufferCount, unsigned int options) {
    return StreamPtr(new FixedSizeStream(samplesPerChannel, bufferCount, options));
}

double FixedSizeStream::SetPosition(double requestedSeconds) {
    double actualSeconds = this->decoder->SetPosition(requestedSeconds);

    if (actualSeconds != -1) {
        double rate = (double) this->decoderSampleRate;

        this->decoderSamplePosition =
            (uint64)(actualSeconds * rate) * this->decoderChannels;
    }

    return actualSeconds;
}

bool FixedSizeStream::OpenStream(std::string uri) {
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

void FixedSizeStream::OnBufferProcessedByPlayer(BufferPtr buffer) {
    this->RecycleBuffer(buffer);
}

BufferPtr FixedSizeStream::GetNextBufferFromDecoder() {
    BufferPtr buffer = this->currentBuffer;

    /* get a spare buffer, then ask the decoder for some data */
    if (!this->decoder->GetBuffer(buffer.get())) {
        return BufferPtr();
    }

    /* remember the sample rate so we can calculate the current time-position */
    if (!this->decoderSampleRate || !this->decoderChannels) {
        this->decoderSampleRate = buffer->SampleRate();
        this->decoderChannels = buffer->Channels();
    }

    /* offset, in samples */
    this->decoderSamplePosition += buffer->Samples();

    /* calculate the position (seconds) in the buffer */
    buffer->SetPosition(
        ((double) this->decoderSamplePosition) /
        ((double) buffer->Channels()) /
        ((double) this->decoderSampleRate));

    return buffer;
}

inline BufferPtr FixedSizeStream::GetEmptyBuffer() {
    BufferPtr target;
    
    if (recycledBuffers.size()) {
        target = recycledBuffers.back();
        recycledBuffers.pop_back();
    }
    else {
        target = Buffer::Create();
        target->CopyFormat(this->currentBuffer);
    }

    return target;
}

#define COPY_BUFFER(target, current, count, offset) \
    target->Copy(&current->BufferPointer()[offset], count); \
    target->SetPosition( \
        ((double) this->decoderSamplePosition + offset) / \
        ((double) current->Channels()) / \
        ((double) this->decoderSampleRate)); \
    this->filledBuffers.push_back(target);

BufferPtr FixedSizeStream::GetNextProcessedOutputBuffer() {
    BufferPtr currentBuffer;
    
    /* ensure we have at least BUFFER_COUNT buffers, and that at least half of them
    are filled with data! */
    while (this->filledBuffers.size() < (this->bufferCount / 2) || 
        this->filledBuffers.size() + this->recycledBuffers.size() < this->bufferCount)
    {
        /* ask the decoder for the next buffer */
        currentBuffer = this->GetNextBufferFromDecoder();

        if (!currentBuffer) {
            break; /* important... bust out of the loop when we're done! */
        }

        /* break the buffer into 512 sample per channel buffers. this will
        help us ensure visualizer data is uniform. note that the last buffer
        may not be exactly 512 -- that should be fine, generally. */
        BufferPtr target;

        int floatsPerBuffer = this->samplesPerChannel * currentBuffer->Channels();
        int buffers = currentBuffer->Samples() / floatsPerBuffer;
        int offset = 0;

        for (int i = 0; i < buffers; i++) {
            target = this->GetEmptyBuffer();
            COPY_BUFFER(target, currentBuffer, floatsPerBuffer, offset);
            offset += floatsPerBuffer;
        }

        if (offset < this->currentBuffer->Samples()) {
            target = this->GetEmptyBuffer();
            COPY_BUFFER(target, currentBuffer, this->currentBuffer->Samples() - offset, offset);
        }
    }

    if (this->filledBuffers.size()) {
        currentBuffer = this->filledBuffers.front();
        this->filledBuffers.pop_front();

        /* let DSP plugins process the buffer */
        if (this->dsps.size() > 0) {
            for (Dsps::iterator dsp = this->dsps.begin(); dsp != this->dsps.end(); ++dsp) {
                dspBuffer->CopyFormat(currentBuffer);
                dspBuffer->SetPosition(currentBuffer->Position());

                if ((*dsp)->Process(currentBuffer.get(), dspBuffer.get())) {
                    currentBuffer.swap(dspBuffer);
                }
            }
        }

        return currentBuffer;
    }

    return BufferPtr();
}

/* marks a used buffer as recycled so it can be re-used later. */
void FixedSizeStream::RecycleBuffer(BufferPtr oldBuffer) {
    this->recycledBuffers.push_back(oldBuffer);
}

void FixedSizeStream::LoadDecoderPlugins() {
    PluginFactory::DestroyDeleter<IDecoderFactory> typedef Deleter;

    this->decoderFactories = PluginFactory::Instance()
        .QueryInterface<IDecoderFactory, Deleter>("GetDecoderFactory");
}
