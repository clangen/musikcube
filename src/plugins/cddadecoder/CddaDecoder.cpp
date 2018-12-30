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

#include "stdafx.h"
#include "CddaDecoder.h"

#define CDDA_SAMPLE_RATE 44100
#define CDDA_BUFFER_SIZE 4096
#define BYTES_PER_RAW_SAMPLE 2

CddaDecoder::CddaDecoder() {
    this->duration = -1.0f;
    this->data = nullptr;
    this->buffer = new BYTE[CDDA_BUFFER_SIZE];
    this->exhausted = false;
}

CddaDecoder::~CddaDecoder() {
    delete[] this->buffer;
}

void CddaDecoder::Release() {
    delete this;
}

bool CddaDecoder::Open(IDataStream* data) {
    this->data = (CddaDataStream *) data;
    return (data != NULL);
}

double CddaDecoder::SetPosition(double seconds) {
    LONGLONG bytesPerSecond = (CDDA_SAMPLE_RATE * data->GetChannelCount() * sizeof(short));
    LONGLONG offset = (LONGLONG)((float)bytesPerSecond * seconds);
    if (this->data->SetPosition((PositionType) offset)) {
        return seconds;
    }

    return -1;
}

double CddaDecoder::GetDuration() {
    if (this->duration == -1.0f) {
        this->duration = (double)(data->Length() / data->GetChannelCount() / sizeof(short)) / 44100.0f;
    }

    return this->duration;
}

bool CddaDecoder::GetBuffer(IBuffer *buffer) {
    int channels = data->GetChannelCount();
    buffer->SetSamples(CDDA_BUFFER_SIZE / BYTES_PER_RAW_SAMPLE);
    buffer->SetChannels(data->GetChannelCount());
    buffer->SetSampleRate(CDDA_SAMPLE_RATE);

    float* target = buffer->BufferPointer();

    PositionType count = this->data->Read(
        (void *) this->buffer, CDDA_BUFFER_SIZE);

    if (count == (PositionType) CddaDataStream::ReadError::DeviceBusy) {
        this->exhausted = false; /* stream not exhausted, waiting for data */
        return false;
    }

    if (count > 0) {
        short* t = (short*) this->buffer;

        for (int x = 0; x < (count / BYTES_PER_RAW_SAMPLE); x++) {
            target[x] = (float) t[x] / 16384.0f;
        }

        buffer->SetSamples(count / BYTES_PER_RAW_SAMPLE);

        return true;
    }

    this->exhausted = true;
    return false;
}
