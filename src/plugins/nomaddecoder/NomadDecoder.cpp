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

#include "NomadDecoder.h"
#include <vector>
#include <cstring>
#include <stdio.h>

using namespace musik::core::sdk;

#define DEFAULT_READ_SAMPLE_SIZE 2304 * 4
#define SAMPLES_PER_FRAME_LAYER_1 1152
#define SAMPLES_PER_FRAME_LAYER_2 576

static ssize_t nomadRead(void *datasource, void *buffer, size_t count) {
    IDataStream *stream = static_cast<IDataStream*>(datasource);
    return (ssize_t) stream->Read(buffer, count);
}

static off_t nomadSeek(void *datasource, off_t offset, int whence) {
    IDataStream *stream = static_cast<IDataStream*>(datasource);

    if (whence == SEEK_END && stream->Length() > 0) {
        /* the only time nomad ever seeks to the end is when it's trying to
        determine the length in bytes. this optimization allows us to play
        mp3 files over HTTP without needing to wait until the entire file
        is downloaded. it also prevents unnecessary fseek() calls. */
        if (stream->Length() > 0) {
            return stream->Length();
        }
        return (off_t) -1;
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
    this->exhausted = false;
    this->duration = -1.0f;
    this->nomadContext = nullptr;
    this->callbacks.read = &nomadRead;
    this->callbacks.lseek = &nomadSeek;
    this->callbacks.close = &nomadClose;
}

NomadDecoder::~NomadDecoder() {
}

void NomadDecoder::Release() {
    if (this->nomadContext) {
        nomad_close(this->nomadContext);
        this->nomadContext = nullptr;
    }

    delete this;
}

double NomadDecoder::SetPosition(double seconds) {
    return !nomad_time_seek(this->nomadContext, seconds) ? seconds : -1;
}

double NomadDecoder::GetDuration() {
    return this->duration;
}

bool NomadDecoder::GetBuffer(IBuffer *buffer) {
    buffer->SetSamples(DEFAULT_READ_SAMPLE_SIZE);

    int read = nomad_read(
        this->nomadContext,
        (char *)buffer->BufferPointer(),
        DEFAULT_READ_SAMPLE_SIZE,
        SAMPLE_FORMAT_32_BIT_FLOAT);

    auto info = nomad_info(this->nomadContext);
    buffer->SetChannels(info->channels);
    buffer->SetSamples(read > 0 ? read : 0);
    buffer->SetSampleRate(info->sample_rate);

    if (read > 0) {
        return true;
    }

    this->exhausted = true;
    return false;
}

bool NomadDecoder::Open(IDataStream *stream) {
    int tagLengthBytes = 0;

    if (stream->Seekable()) {
        tagLengthBytes = this->GetId3v2HeaderLength(stream);
        stream->SetPosition(0);
    }

    const int result = nomad_open_callbacks(&this->nomadContext, stream, &this->callbacks);

    if (!result) {
        /* nomad's time estimation when there is no xing header seems wrong in many
        cases -- so we recalculate it here based on taglib's algorithm, which
        (empirically) seems much more accurate */
        if (!nomad_xing(this->nomadContext) && tagLengthBytes > 0) {
            const int bitrate = nomad_info(this->nomadContext)->avg_bitrate / 1000;
            const int streamLength = stream->Length() - tagLengthBytes;
            this->duration = (streamLength * 8.0 / bitrate) / 1000;
        }
        else {
            this->duration = nomad_info(this->nomadContext)->duration;
        }
    }
    else {
        this->nomadContext = nullptr;
    }

    if (!result) {
        return true;
    }

    this->exhausted = false;
    return false;
}

/* adapted from http://stackoverflow.com/a/3520427 */

#pragma pack(1)
struct Mp3Header {
    char tag[3];
    unsigned char maj_ver;
    unsigned char min_ver;
    unsigned char flags;
    unsigned int  size;
};
#pragma pack()

static int HEADER_LENGTH = sizeof(Mp3Header);

size_t NomadDecoder::GetId3v2HeaderLength(musik::core::sdk::IDataStream *stream) {
    Mp3Header hdr = { 0 };
    if (stream->Read(&hdr, HEADER_LENGTH) != HEADER_LENGTH) {
        return 0;
    }

    if (memcmp(hdr.tag, "ID3", 3) != 0) {
        return 0;
    }

    // value has 4 bytes (8-bits each)
    //  - discard most significant bit from each byte
    //  - reverse byte order
    //  - concatenate the 4 * 7-bit nibbles into a 24-bit size.
    unsigned char *parts = reinterpret_cast<unsigned char *>(&hdr.size);

    return
        (parts[3] & 0x7F) |
        ((parts[2] & 0x7F) << 7) |
        ((parts[1] & 0x7F) << 14) |
        ((parts[0] & 0x7F) << 21);
}
