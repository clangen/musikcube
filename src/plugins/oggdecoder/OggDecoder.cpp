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
#include "OggDecoder.h"

#define OGG_MAX_SAMPLES 1024

OggDecoder::OggDecoder() {
    this->duration = -1.0f;
    this->exhausted = false;
    this->oggCallbacks.read_func = &OggRead;
    this->oggCallbacks.seek_func = &OggSeek;
    this->oggCallbacks.tell_func = &OggTell;
    this->oggCallbacks.close_func = &OggClose;
}

size_t OggDecoder::OggRead(void *buffer, size_t nofParts, size_t partSize, void *datasource) {
    return (size_t)((OggDecoder*)datasource)->fileStream->Read(buffer,(long)(nofParts*partSize));
}

int OggDecoder::OggSeek(void *datasource, ogg_int64_t offset, int whence) {
    OggDecoder* decoder = (OggDecoder*) datasource;

    switch(whence) {
        case SEEK_CUR:
            {
                long currentPosition = decoder->fileStream->Position();
                if (decoder->fileStream->SetPosition(currentPosition + (long) offset)) {
                    return 0;
                }
            }
            break;
        case SEEK_END:
            {
                long fileSize = decoder->fileStream->Length();
                if (decoder->fileStream->SetPosition(fileSize)) {
                    return 0;
                }
            }
            break;
        case SEEK_SET:
            {
                if (decoder->fileStream->SetPosition((long) offset)) {
                    return 0;
                }
            }
            break;
    }

    return -1;
}

long OggDecoder::OggTell(void *datasource) {
    return ((OggDecoder*) datasource)->fileStream->Position();
}

int OggDecoder::OggClose(void *datasource) {
    if(((OggDecoder*) datasource)->fileStream->Close()) {
        return 0;
    }
    return -1;
}

OggDecoder::~OggDecoder() {
}

bool OggDecoder::Open(musik::core::sdk::IDataStream *fileStream) {
    this->fileStream = fileStream;

    if (ov_open_callbacks(this, &this->oggFile, NULL, 0, this->oggCallbacks) != 0) {
        this->exhausted = true;
        return false;
    }

    this->duration = ov_time_total(&this->oggFile, -1);

    return true;
}

void OggDecoder::Release() {
    ov_clear(&this->oggFile);
    delete this;
}

double OggDecoder::SetPosition(double second) {
    if (ov_seekable(&this->oggFile)) {
        if (!ov_time_seek(&this->oggFile, second)) {
            return ov_time_tell(&this->oggFile);
        }
    }

    return -1;
}

double OggDecoder::GetDuration() {
    return this->duration;
}

bool OggDecoder::GetBuffer(IBuffer *buffer) {
    int bitstream;
    float **pcm;

    unsigned long samplesRead = ov_read_float(
        &this->oggFile,
        &pcm,
        OGG_MAX_SAMPLES,
        &bitstream);

    if (samplesRead == 0) {
        this->exhausted = true;
        return false;
    }

    vorbis_info *info = ov_info(&this->oggFile, -1);
    buffer->SetChannels(info->channels);
    buffer->SetSampleRate(info->rate);
    buffer->SetSamples(samplesRead * info->channels);

    /*
        The musik audio engine expects:

            SPEAKER_FRONT_LEFT 0x1
            SPEAKER_FRONT_RIGHT 0x2
            SPEAKER_FRONT_CENTER 0x4
            SPEAKER_LOW_FREQUENCY 0x8
            SPEAKER_BACK_LEFT 0x10
            SPEAKER_BACK_RIGHT 0x20

        OGG returns:

            One channel: the stream is monophonic
            Two channels: the stream is stereo. channel order: left, right.
            Three channels: the stream is a 1d-surround encoding. channel order: left, center, right
            Four channels: the stream is quadraphonic surround. channel order: front left, front right, rear left, rear right
            Five channels: the stream is five-channel surround. channel order: front left, front center, front right, rear left, rear right
            Six channels: (used in Dolby Digital/AC3) the stream is 5,1 surround. channel order: front left, front center, front right, rear left, rear right, LFE (the sixth channel is entirely bass).

        ... so let's re-order when writing to our output buffer ...
    */

    float *pDataBuffer = buffer->BufferPointer();

    if (info->channels == 2) {
        for (unsigned long x = 0; x < samplesRead; ++x) {
            pDataBuffer[0] = pcm[0][x];
            pDataBuffer[1] = pcm[1][x];
            pDataBuffer += 2;
        }
    }
    else if (info->channels == 3) {
        for (unsigned long x=0; x < samplesRead; ++x) {
            *pDataBuffer = pcm[0][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[2][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[1][x];
            ++pDataBuffer;
        }
    }
    else if (info->channels == 4) {
        for (unsigned long x = 0; x < samplesRead; ++x) {
            *pDataBuffer = pcm[0][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[1][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[2][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[3][x];
            ++pDataBuffer;
        }
    }
    else if (info->channels == 5) {
        for (unsigned long x = 0; x < samplesRead; ++x) {
            *pDataBuffer = pcm[0][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[2][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[1][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[3][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[4][x];
            ++pDataBuffer;
        }
    }
    else if (info->channels == 6) {
        for (unsigned long x = 0; x < samplesRead; ++x) {
            *pDataBuffer = pcm[0][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[2][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[1][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[5][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[3][x];
            ++pDataBuffer;
            *pDataBuffer = pcm[4][x];
            ++pDataBuffer;
        }
    }
    else {
        for (unsigned long x = 0; x < samplesRead; ++x) {
            for (int channel(0); channel < info->channels; ++channel) {
                *pDataBuffer = pcm[channel][x];
                ++pDataBuffer;
            }
        }
    }

    return true;
}
