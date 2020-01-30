//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
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
using namespace musik::core::io;

static std::string TAG = "Stream";

#define MIN_BUFFER_COUNT 30

Stream::Stream(int samplesPerChannel, double bufferLengthSeconds, StreamFlags options)
: options(options)
, samplesPerChannel(samplesPerChannel)
, bufferLengthSeconds(bufferLengthSeconds)
, bufferCount(0)
, decoderSampleRate(0)
, decoderChannels(0)
, decoderPosition(0)
, decoderSampleOffset(0)
, decoderSamplesRemain(0)
, done(false)
, capabilities(0)
, rawBuffer(nullptr) {
    if (((int) this->options & (int) StreamFlags::NoDSP) == 0) {
        dsps = streams::GetDspPlugins();
    }

    this->decoderBuffer = new Buffer();
    this->decoderBuffer->SetSamples(0);
}

Stream::~Stream() {
    delete[] rawBuffer;
    delete this->decoderBuffer;

    for (Buffer* buffer : this->recycledBuffers) {
        delete buffer;
    }

    for (Buffer* buffer : this->filledBuffers) {
        delete buffer;
    }
}

IStreamPtr Stream::Create(int samplesPerChannel, double bufferLengthSeconds, StreamFlags options) {
    return IStreamPtr(new Stream(samplesPerChannel, bufferLengthSeconds, options));
}

IStream* Stream::CreateUnmanaged(int samplesPerChannel, double bufferLengthSeconds, StreamFlags options) {
    return new Stream(samplesPerChannel, bufferLengthSeconds, options);
}

double Stream::SetPosition(double requestedSeconds) {
    double actualSeconds = this->decoder->SetPosition(requestedSeconds);

    if (actualSeconds != -1) {
        double rate = (double) this->decoderSampleRate;

        this->decoderPosition =
            (uint64_t)(actualSeconds * rate) * this->decoderChannels;

        /* move all the filled buffers back to the recycled queue */
        auto it = this->filledBuffers.begin();
        while (it != this->filledBuffers.end()) {
            this->recycledBuffers.push_back(*it);
            ++it;
        }

        this->filledBuffers.clear();
    }

    return actualSeconds;
}

double Stream::GetDuration() {
    return this->decoder ? this->decoder->GetDuration() : -1.0f;
}

int Stream::GetCapabilities() {
    return this->capabilities;
}

bool Stream::OpenStream(std::string uri) {
    musik::debug::info(TAG, "opening " + uri);

    /* use our file stream abstraction to open the data at the
    specified URI */
    this->dataStream = DataStreamFactory::OpenSharedDataStream(uri.c_str(), OpenFlags::Read);

    if (!this->dataStream) {
        musik::debug::error(TAG, "failed to open " + uri);
        return false;
    }

    this->decoder = streams::GetDecoderForDataStream(this->dataStream);

    if (this->decoder) {
        if (this->dataStream->CanPrefetch()) {
            this->capabilities |= (int) musik::core::sdk::Capability::Prebuffer;
            this->RefillInternalBuffers();
        }
        return true;
    }

    return false;
}

void Stream::Interrupt() {
    if (this->dataStream) {
        this->dataStream->Interrupt();
    }
}

void Stream::OnBufferProcessedByPlayer(IBuffer* buffer) {
    this->recycledBuffers.push_back((Buffer*) buffer);
}

bool Stream::GetNextBufferFromDecoder() {
    /* ask the decoder for some data */
    if (!this->decoder->GetBuffer(this->decoderBuffer)) {
        return false;
    }

    /* ensure our internal state is initialized */
    if (!this->rawBuffer) {
        this->decoderSampleRate = this->decoderBuffer->SampleRate();
        this->decoderChannels = this->decoderBuffer->Channels();
        this->samplesPerBuffer = samplesPerChannel * decoderChannels;

        this->bufferCount = std::max(MIN_BUFFER_COUNT, (int)(this->bufferLengthSeconds *
            (double)(this->decoderSampleRate / this->samplesPerBuffer)));

        this->rawBuffer = new float[bufferCount * this->samplesPerBuffer];
        int offset = 0;
        for (int i = 0; i < bufferCount; i++) {
            auto buffer = new Buffer(this->rawBuffer + offset, this->samplesPerBuffer);
            buffer->SetSampleRate(this->decoderSampleRate);
            buffer->SetChannels(this->decoderChannels);
            this->recycledBuffers.push_back(buffer);
            offset += this->samplesPerBuffer;
        }
    }

    return true;
}

inline Buffer* Stream::GetEmptyBuffer() {
    if (recycledBuffers.size()) {
        Buffer* target = recycledBuffers.front();
        recycledBuffers.pop_front();
        return target;
    }
    return nullptr;
}

IBuffer* Stream::GetNextProcessedOutputBuffer() {
    this->RefillInternalBuffers();

    /* in the normal case we have buffers available in the filled queue. */
    if (this->filledBuffers.size()) {
        Buffer* buffer = this->filledBuffers.front();
        this->filledBuffers.pop_front();

        for (std::shared_ptr<IDSP> dsp : this->dsps) {
            dsp->Process(buffer);
        }

        return buffer;
    }

    return nullptr;
}

void Stream::RefillInternalBuffers() {
    int recycled = this->recycledBuffers.size();
    int count = 0;

    if (!this->rawBuffer) { /* not initialized */
        count = -1;
    }
    else {
        /* fill another chunk -- most of the time for file-based
        streams this will only be a single buffer. note the - 1
        part is to leave space for any potential remainder. */
        count = std::min(recycled - 1, std::max(1, this->bufferCount / 4));
    }

    Buffer* target = nullptr;
    long targetSampleOffset = 0;
    long targetSamplesRemain = 0;

    while (!this->done && (count > 0 || count == -1)) {
        /* get the next buffer, if the last one has been consumed... */
        if (this->decoderSamplesRemain <= 0) {
            if (!GetNextBufferFromDecoder()) {
                if (target) { /* very last buffer for this stream. */
                    target->SetSamples(targetSampleOffset);
                }
                this->done = true;
                break;
            }

            if (this->decoderBuffer->Samples() == 0) {
                continue;
            }

            this->decoderSamplesRemain = this->decoderBuffer->Samples();
            this->decoderSampleOffset = 0;
        }

        /* count will be < 0 on the very first pass through. let's try to
        fill 1/4 of our buffers */
        if (count < 0) {
            count = bufferCount / 4;
        }

        /* we're going to write to this guy... */
        if (!target) {
            target = this->GetEmptyBuffer();

            if (!target) {
                break; /* no available buffers. break out. */
            }

            target->SetSamples(0);

            target->SetPosition(
                ((double) this->decoderPosition) /
                ((double) this->decoderChannels) /
                ((double) this->decoderSampleRate));

            filledBuffers.push_back(target);
        }

        /* write to the target, from the decoder buffer. note that after the
        write the target may not be full, or the decoder buffer may not be
        empty. we'll go through the loop again... */
        targetSamplesRemain = this->samplesPerBuffer - targetSampleOffset;
        if (targetSamplesRemain > 0) {
            long samplesToCopy = std::min(this->decoderSamplesRemain, targetSamplesRemain);
            if (samplesToCopy > 0) {
                float* src = this->decoderBuffer->BufferPointer() + this->decoderSampleOffset;
                target->Copy(src, samplesToCopy, targetSampleOffset);

                this->decoderPosition += samplesToCopy;
                this->decoderSampleOffset += samplesToCopy;
                this->decoderSamplesRemain -= samplesToCopy;

                targetSampleOffset += samplesToCopy;

                if (targetSampleOffset == this->samplesPerBuffer) {
                    targetSampleOffset = 0;
                    target = nullptr;
                    --count; /* target buffer has been filled. */
                }
            }
        }
    }
}