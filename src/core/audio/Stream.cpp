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
#include <core/sdk/constants.h>
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

Stream::Stream(int samplesPerChannel, double bufferLengthSeconds, unsigned int options)
: options(options)
, samplesPerChannel(samplesPerChannel)
, bufferLengthSeconds(bufferLengthSeconds)
, bufferCount(0)
, decoderSampleRate(0)
, decoderChannels(0)
, decoderSamplePosition(0)
, done(false)
, capabilities(0)
, remainder(nullptr)
, rawBuffer(nullptr) {
    if ((this->options & NoDSP) == 0) {
        streams::GetDspPlugins();
    }

    this->decoderBuffer = new Buffer();
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

IStreamPtr Stream::Create(int samplesPerChannel, double bufferLengthSeconds, unsigned int options) {
    return IStreamPtr(new Stream(samplesPerChannel, bufferLengthSeconds, options));
}

double Stream::SetPosition(double requestedSeconds) {
    double actualSeconds = this->decoder->SetPosition(requestedSeconds);

    if (actualSeconds != -1) {
        double rate = (double) this->decoderSampleRate;

        this->decoderSamplePosition =
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
    this->dataStream = musik::core::io::DataStreamFactory::OpenUri(uri.c_str());

    if (!this->dataStream) {
        musik::debug::err(TAG, "failed to open " + uri);
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

void Stream::OnBufferProcessedByPlayer(Buffer* buffer) {
    this->recycledBuffers.push_back(buffer);
}

bool Stream::GetNextBufferFromDecoder() {
    Buffer* buffer = this->decoderBuffer;

    /* ask the decoder for some data */
    if (!this->decoder->GetBuffer(buffer)) {
        return false;
    }

    /* ensure our internal state is initialized. */
    if (!this->rawBuffer) {
        this->decoderSampleRate = buffer->SampleRate();
        this->decoderChannels = buffer->Channels();

        int samplesPerBuffer = samplesPerChannel * decoderChannels;

        this->bufferCount = (int)(this->bufferLengthSeconds *
            (double)(this->decoderSampleRate / samplesPerBuffer));

        this->rawBuffer = new float[bufferCount * samplesPerBuffer];
        int offset = 0;
        for (int i = 0; i < bufferCount; i++) {
            auto buffer = new Buffer(this->rawBuffer + offset, samplesPerBuffer);
            buffer->SetSampleRate(this->decoderSampleRate);
            this->recycledBuffers.push_back(buffer);
            offset += samplesPerBuffer;
        }
    }

    this->decoderSamplePosition += buffer->Samples();

    /* calculate the position (seconds) in the buffer */
    buffer->SetPosition(
        ((double) this->decoderSamplePosition) /
        ((double) buffer->Channels()) /
        ((double) this->decoderSampleRate));

    return true;
}

inline Buffer* Stream::GetEmptyBuffer() {
    Buffer* target;

    if (recycledBuffers.size()) {
        target = recycledBuffers.front();
        recycledBuffers.pop_front();
    }
    else {
        /* if we've calculated our buffer sizes correctly based on what
        the output device expects, we should never hit this case. but
        if something goes awry and we need more space to store samples */
        target = new Buffer();
        target->CopyFormat(this->decoderBuffer);
    }

    return target;
}

Buffer* Stream::GetNextProcessedOutputBuffer() {
    this->RefillInternalBuffers();

    /* in the normal case we have buffers available in the filled queue. */
    if (this->filledBuffers.size()) {
        Buffer* currentBuffer = this->filledBuffers.front();
        this->filledBuffers.pop_front();
        this->ApplyDsp(currentBuffer);
        return currentBuffer;
    }

    /* if there's nothing left in the queue, we're almost done. there may
    be a partial buffer still hanging around from the last decoder read */
    if (remainder) {
        Buffer* finalBuffer = remainder;
        remainder = nullptr;
        this->ApplyDsp(finalBuffer);
        return finalBuffer;
    }

    return nullptr; /* stream is done. */
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
        count = std::min(recycled - 1, this->bufferCount / 4);
    }

    while (!this->done && (count > 0 || count == -1)) {
        --count;

        /* ask the decoder for the next buffer */
        if (!GetNextBufferFromDecoder()) {
            this->done = true;
            break;
        }

        /* if we were just initialized, then make sure our buffer
        is about a quarter filled. this will start playback quickly,
        and ensure we don't have any buffer underruns */
        if (count < 0) {
            count = bufferCount / 4;
        }

        Buffer* currentBuffer = this->decoderBuffer;

        int floatsPerBuffer = this->samplesPerChannel * currentBuffer->Channels();

        Buffer* target;
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
                remainder = nullptr;
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
}

void Stream::ApplyDsp(Buffer* buffer) {
    if (this->dsps.size() > 0) {
        for (std::shared_ptr<IDSP> dsp : this->dsps) {
            dsp->Process(buffer);
        }
    }
}
