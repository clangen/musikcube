//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <musikcore/audio/Buffer.h>

#ifdef WIN32
    #define CopyFloat(dst, src, num) CopyMemory(dst, src, (num) * sizeof(float))
#else
    #define CopyFloat(dst, src, num) memcpy((float*) dst, (float*)src, (num) * sizeof(float))
#endif

using namespace musik::core::audio;

Buffer::Buffer(Flags flags) noexcept
: buffer(nullptr)
, samples(0)
, internalBufferSize(0)
, sampleRate(44100)
, channels(2)
, flags(flags)
, position(0) {
}

Buffer::Buffer(float* buffer, int samples) noexcept
: buffer(buffer)
, samples(samples)
, internalBufferSize(samples)
, sampleRate(44100)
, channels(2)
, flags(ImmutableSize | NoDelete)
, position(0) {
}

Buffer::~Buffer() {
    if ((flags & NoDelete) == 0) {
        delete[] this->buffer;
    }
}

long Buffer::SampleRate() const noexcept { /* hertz */
    return this->sampleRate;
}

void Buffer::SetSampleRate(long sampleRate) noexcept { /* hertz */
    this->sampleRate = sampleRate;
}

int Buffer::Channels() const noexcept {
    return this->channels;
}

void Buffer::SetChannels(int channels) noexcept {
    this->channels = channels;
}

float* Buffer::BufferPointer() const noexcept {
    return this->buffer;
}

long Buffer::Samples() const noexcept {
    return this->samples;
}

void Buffer::SetSamples(long samples) {
    this->samples = samples;
    this->ResizeBuffer();
}

void Buffer::CopyFormat(Buffer* fromBuffer) noexcept {
    if (fromBuffer) {
        this->channels = fromBuffer->Channels();
        this->sampleRate = fromBuffer->SampleRate();
    }
}

void Buffer::ResizeBuffer() {
    if (this->samples > this->internalBufferSize) {
        if (flags & ImmutableSize && this->internalBufferSize > 0) {
            throw std::runtime_error("buffer cannot be resized");
        }

        delete[] this->buffer;
        this->buffer = new float[this->samples];
        this->internalBufferSize = this->samples;
    }
}

/* logical bytes; backing store may be be larger */
long Buffer::Bytes() const noexcept {
    return sizeof(float) * this->samples;
}

double Buffer::Position() const noexcept {
    return this->position;
}

void Buffer::SetPosition(double position) noexcept {
    this->position = position;
}

void Buffer::Copy(float const* buffer, long samples, long offset) {
    const long length = offset + samples;
    if (length > this->internalBufferSize) {
        float *newBuffer = new float[length];
        CopyFloat(newBuffer, this->buffer, this->internalBufferSize);
        CopyFloat(newBuffer + offset, buffer, samples);
        delete[] this->buffer;
        this->buffer = newBuffer;
        this->internalBufferSize = length;
    }
    else {
        CopyFloat(this->buffer + offset, buffer, samples);
    }

    this->samples = std::max(this->samples, length);
}