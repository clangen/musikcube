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

#include "Stream.h"
#include "Streams.h"
#include <core/debug.h>

using namespace musik::core::audio;
using namespace musik::core::sdk;

static std::string TAG = "Stream";

#define SET_OFFSET(target, offset) \
    target->SetPosition( \
        ((double) this->decoderSamplePosition + offset) / \
        ((double) target->Channels()) / \
        ((double) this->decoderSampleRate));

#define COPY_BUFFER(target, current, count, offset) \
    target->Copy(current->BufferPointer() + offset, count); \
    SET_OFFSET(target, offset) \

Stream::Stream(int samplesPerChannel, int bufferCount, unsigned int options)
: options(options)
, samplesPerChannel(samplesPerChannel)
, bufferCount(bufferCount)
, decoderSampleRate(0)
, decoderChannels(0)
, decoderSamplePosition(0) {
    if ((this->options & NoDSP) == 0) {
        streams::GetDspPlugins();
    }

    for (int i = 0; i < bufferCount; i++) {
        this->recycledBuffers.push_back(Buffer::Create());
    }

    this->dspBuffer = Buffer::Create();

    /* note that the decoder buffer needs to have a pre-allocated, non-resizable buffer
    in Windows due to the way heap allocations work cross-DLL. in theory this is a
    surmountable problem, in practice we get heap corruption. the buffer is enough for
    8 channels worth of 2048 samples, which should be more than reasonable. */
#ifdef WIN32
    this->decoderBuffer = Buffer::Create(Buffer::ImmutableSize);
    this->decoderBuffer->SetSamples(2048 * 8);
#else
    this->decoderBuffer = Buffer::Create();
#endif
}

Stream::~Stream() {
}

StreamPtr Stream::Create(int samplesPerChannel, int bufferCount, unsigned int options) {
    return StreamPtr(new Stream(samplesPerChannel, bufferCount, options));
}

double Stream::SetPosition(double requestedSeconds) {
    double actualSeconds = this->decoder->SetPosition(requestedSeconds);

    if (actualSeconds != -1) {
        double rate = (double) this->decoderSampleRate;

        this->decoderSamplePosition =
            (uint64)(actualSeconds * rate) * this->decoderChannels;

        this->recycledBuffers.splice(
            this->recycledBuffers.begin(),
            this->filledBuffers);
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

    this->decoder = streams::GetDecoderForDataStream(this->dataStream);
    return !!this->decoder;
}

void Stream::OnBufferProcessedByPlayer(BufferPtr buffer) {
    this->RecycleBuffer(buffer);
}

bool Stream::GetNextBufferFromDecoder() {
    BufferPtr buffer = this->decoderBuffer;

    /* get a spare buffer, then ask the decoder for some data */
    if (!this->decoder->GetBuffer(buffer.get())) {
        return false;
    }

    this->decoderSampleRate = buffer->SampleRate();
    this->decoderChannels = buffer->Channels();
    this->decoderSamplePosition += buffer->Samples();

    /* calculate the position (seconds) in the buffer */
    buffer->SetPosition(
        ((double) this->decoderSamplePosition) /
        ((double) buffer->Channels()) /
        ((double) this->decoderSampleRate));

    return true;
}

inline BufferPtr Stream::GetEmptyBuffer() {
    BufferPtr target;

    if (recycledBuffers.size()) {
        target = recycledBuffers.back();
        recycledBuffers.pop_back();
    }
    else {
        target = Buffer::Create();
        target->CopyFormat(this->decoderBuffer);
    }

    return target;
}

BufferPtr Stream::GetNextProcessedOutputBuffer() {
    BufferPtr currentBuffer;

    /* ensure we have at least BUFFER_COUNT buffers, and that at least half of them
    are filled with data! */
    while (this->filledBuffers.size() < (this->bufferCount / 2)) {
        /* ask the decoder for the next buffer */
        if (!GetNextBufferFromDecoder()) {
            break;
        }

        currentBuffer = this->decoderBuffer;

        int floatsPerBuffer = this->samplesPerChannel * currentBuffer->Channels();

        BufferPtr target;
        int offset = 0;

        /* if we have a partial / remainder buffer hanging out from the last time
        through, let's fill it up with the head of the new buffer. */
        if (remainder) {
            long desired = floatsPerBuffer - remainder->Samples();
            long actual = std::min(currentBuffer->Samples(), desired);

            remainder->Append(currentBuffer->BufferPointer(), actual);
            SET_OFFSET(remainder, 0);

            if (remainder->Samples() == floatsPerBuffer) {
                /* normal case: we were able to fill it; add it to the list of
                filled buffers and continue to fill some more... */
                this->filledBuffers.push_back(remainder);
                offset += actual;
                remainder.reset();
            }
            else {
                continue; /* already consumed all of the decoder buffer. go back
                to the top of the loop to get some more data. */
            }
        }

        /* now that the remainder is taken care of, break the rest of the data
        into uniform chunks */

        int buffersToFill = (currentBuffer->Samples() - offset) / floatsPerBuffer;

        for (int i = 0; i < buffersToFill; i++) {
            target = this->GetEmptyBuffer();
            COPY_BUFFER(target, currentBuffer, floatsPerBuffer, offset);
            this->filledBuffers.push_back(target);
            offset += floatsPerBuffer;
        }

        /* any remainder will be sent to the output next time through the loop*/
        if (offset < currentBuffer->Samples()) {
            remainder = this->GetEmptyBuffer();
            COPY_BUFFER(remainder, currentBuffer, currentBuffer->Samples() - offset, offset);
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

    /* final remainder */
    if (remainder) {
        BufferPtr result = remainder;
        remainder.reset();
        return result;
    }

    return BufferPtr();
}

/* marks a used buffer as recycled so it can be re-used later. */
void Stream::RecycleBuffer(BufferPtr oldBuffer) {
    this->recycledBuffers.push_back(oldBuffer);
}
