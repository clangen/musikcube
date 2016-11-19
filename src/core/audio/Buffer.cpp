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

#include <core/audio/Buffer.h>
#include <fftw3.h>

#define DEBUG 0

#if DEBUG > 0
#include <iostream>
#endif

#ifdef WIN32
    #define CopyFloat(dst, src, num) CopyMemory(dst, src, (num) * sizeof(float))
#else
    #define CopyFloat(dst, src, num) memmove((void*) dst, (void*)src, (num) * sizeof(float))
#endif

using namespace musik::core::audio;

Buffer::Buffer(void)
: buffer(nullptr)
, sampleSize(0)
, internalBufferSize(0)
, sampleRate(44100)
, channels(2)
, fft(nullptr) {

}

Buffer::~Buffer() {
    delete this->buffer;
}

BufferPtr Buffer::Create() {
    return BufferPtr(new Buffer());
}

long Buffer::SampleRate() const { /* hertz */
    return this->sampleRate;
}

void Buffer::SetSampleRate(long sampleRate) { /* hertz */
    this->sampleRate = sampleRate;
}

int Buffer::Channels() const {
    return this->channels;
}

void Buffer::SetChannels(int channels) {
    this->channels = channels;
    this->ResizeBuffer();
}

float* Buffer::BufferPointer() const {
    return this->buffer;
}

long Buffer::Samples() const {
    return this->sampleSize;
}

void Buffer::SetSamples(long samples) {
    this->sampleSize = samples;
    this->ResizeBuffer();
}

void Buffer::CopyFormat(BufferPtr fromBuffer) {
    this->sampleSize = fromBuffer->Samples();
    this->channels = fromBuffer->Channels();
    this->sampleRate = fromBuffer->SampleRate();
    this->ResizeBuffer();
}

void Buffer::ResizeBuffer() {
    if (this->sampleSize > this->internalBufferSize) {
        if (this->buffer) {
            delete this->buffer;
            this->buffer = NULL;
        }

        this->buffer = new float[this->sampleSize];
        this->internalBufferSize = this->sampleSize;
    }
}

/* logical bytes; backing store may be be larger */
long Buffer::Bytes() const {
    return sizeof(float) * this->sampleSize;
}

double Buffer::Position() const {
    return this->position;
}

void Buffer::SetPosition(double position) {
    this->position = position;
}

bool Buffer::Append(BufferPtr appendBuffer) {
    if (this->SampleRate() == appendBuffer->SampleRate() &&
        this->Channels() == appendBuffer->Channels())
    {
        /* number of floats (not bytes) in buffer */
        long newBufferSize = (this->Samples() + appendBuffer->Samples());

        if (newBufferSize > this->internalBufferSize) { /* resize, then copy, if too small */
            float *newBuffer = new float[newBufferSize];

            CopyFloat(newBuffer, this->buffer, this->sampleSize);

            float *dst = &newBuffer[this->sampleSize];
            float *src = appendBuffer->BufferPointer();
            long count = appendBuffer->Samples();

            CopyFloat(dst, src, count);

            delete this->buffer;

#if DEBUG > 0
            std::cerr << "resized with realloc old: " << this->internalBufferSize << " new: " << newBufferSize << "\n";
#endif

            this->buffer = newBuffer;
            this->internalBufferSize = newBufferSize;
        }
        else { /* append, no resize required */
            float *dst = &this->buffer[this->sampleSize];
            float *src = appendBuffer->BufferPointer();
            long count = appendBuffer->Samples();
            CopyFloat(dst, src, count);

#if DEBUG > 0
            std::cerr << "appended " << count << " floats to existing buffer, logical bytes=" << newBufferSize << "\n";
#endif
        }

        this->sampleSize = newBufferSize;
        return true;
    }

    return false;
}

static inline int closestPowerOfTwo(int x) {
    /* http://stackoverflow.com/a/2681094 */
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x - (x >> 1);
}

static inline bool isPowerOfTwo(unsigned int x) {
    return ((x != 0) && ((x & (~x + 1)) == x));
}

bool Buffer::Fft(float* buffer, int size) {
    if (buffer && size > 0 && this->sampleSize > size && isPowerOfTwo(size)) {
        int n = closestPowerOfTwo(this->sampleSize);

        fftwf_complex *out = (fftwf_complex*) fftw_malloc(sizeof(fftwf_complex) * n);
        fftwf_plan plan = fftwf_plan_dft_r2c_1d(n, this->buffer, out, FFTW_MEASURE);
        fftwf_execute(plan);
        fftwf_destroy_plan(plan);

        int w = 0; /* write offset */
        int c = n / size; /* bins to average */
        for (int i = 0; i < n; i+=c) {
            float sum = 0.0f;
            for (int j = 0; j < c; j++) {
                sum += out[1][i + j];
            }
            buffer[w++] = sum / c;
        }

        fftwf_free(out);
        return true;
    }

    return false;
}