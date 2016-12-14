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

#include "NomadDecoder.h"

using namespace musik::core::sdk;

#define DEFAULT_READ_SAMPLE_SIZE 2304

static ssize_t nomadRead(void *datasource, void *buffer, size_t count) {
    IDataStream *stream = static_cast<IDataStream*>(datasource);
    return (ssize_t) stream->Read(buffer, count);
}

static off_t nomadSeek(void *datasource, off_t offset, int whence) {
    IDataStream *stream = static_cast<IDataStream*>(datasource);

    if (whence == SEEK_END && stream->Length() > 0) {
        stream->SetPosition(stream->Length() - offset);
    }
    else {
        PositionType pos = (whence == SEEK_SET)
            ? offset : stream->Position() + offset;

        stream->SetPosition(pos);
    }

    return (off_t) stream->Position();
}

static int nomadClose(void *datasource) {
    /* no-op, IDataStream will be closed by the app */
    return 1;
}

NomadDecoder::NomadDecoder() {
    this->nomad = nullptr;
    this->callbacks.read = &nomadRead;
    this->callbacks.lseek = &nomadSeek;
    this->callbacks.close = &nomadClose;
}

NomadDecoder::~NomadDecoder() {
}

void NomadDecoder::Destroy() {
    nomad_close(this->nomad);
    this->nomad = nullptr;
    delete this;
}

double NomadDecoder::SetPosition(double seconds) {
    return !nomad_time_seek(this->nomad, seconds) ? seconds : -1;
}

bool NomadDecoder::GetBuffer(IBuffer *buffer) {
    buffer->SetSamples(DEFAULT_READ_SAMPLE_SIZE);

    int read = nomad_read(
        this->nomad,
        (char *)buffer->BufferPointer(),
        DEFAULT_READ_SAMPLE_SIZE,
        SAMPLE_FORMAT_32_BIT_FLOAT);

    buffer->SetSamples(read > 0 ? read : 0);

    return (read > 0) ? true : false;
}

bool NomadDecoder::Open(IDataStream *stream) {
    int result = nomad_open_callbacks(&this->nomad, stream, &this->callbacks);
    return result ? false : true;
}
