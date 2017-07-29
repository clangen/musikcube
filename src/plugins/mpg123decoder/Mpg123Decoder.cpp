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

#include "Mpg123Decoder.h"
#include <stdio.h>

#define STREAM_FEED_SIZE 2048 * 2
#define MPG123_DECODER_DEBUG 0

#if MPG123_DECODER_DEBUG > 0
#include <iostream>
#endif

using namespace musik::core::sdk;

Mpg123Decoder::Mpg123Decoder()
: cachedLength(0)
, decoder(NULL)
, sampleRate(44100)
, channels(2)
, fileStream(NULL)
, lastMpg123Status(MPG123_NEED_MORE) {
    this->decoder = mpg123_new(NULL, NULL);
    this->sampleSizeBytes = sizeof(float);
}

Mpg123Decoder::~Mpg123Decoder() {
    if (this->decoder) {
        mpg123_close(this->decoder);
        mpg123_delete(this->decoder);
        this->decoder = NULL;
    }
}

void Mpg123Decoder::Destroy() {
    delete this;
}

double Mpg123Decoder::GetDuration() {
    if (this->decoder) {
        return (double) mpg123_length(this->decoder) / (double) this->sampleRate;
    }

    return 0.0;
}

double Mpg123Decoder::SetPosition(double second) {
    off_t seekToFileOffset = 0;
    off_t seekToSampleOffset = (off_t) (second * (double)this->sampleRate);
    off_t *indexOffset = 0;
    off_t indexSet = 0;
    size_t indexFill = 0;
    int err = mpg123_index(this->decoder, &indexOffset, &indexSet, &indexFill);

    off_t seekedTo = 0;
    int feedMore = 20;
    while((seekedTo = mpg123_feedseek(
        this->decoder,
        seekToSampleOffset,
        SEEK_SET,
        &seekToFileOffset)) == MPG123_NEED_MORE && feedMore > 0)
    {
        if (!this->Feed()) {
            feedMore = 0;
        }

        feedMore--;
    }

    if (seekedTo >= 0) {
        if (this->fileStream->SetPosition(seekToFileOffset)) {
            return second;
        }
    }

    return -1;
}

bool Mpg123Decoder::GetBuffer(IBuffer *buffer) {
    buffer->SetChannels(this->channels);
    buffer->SetSampleRate(this->sampleRate);
    buffer->SetSamples(STREAM_FEED_SIZE);

    unsigned char* targetBuffer = (unsigned char*) (buffer->BufferPointer());

    bool done = false;

    size_t bytesWritten = 0;

    do {
        int readResult = mpg123_read(
          this->decoder,
          targetBuffer,
          buffer->Bytes(),
          &bytesWritten);

        switch (readResult) {
            case MPG123_OK:
               break;

            case MPG123_DONE:
                done = true;
                break;

            case MPG123_NEED_MORE:
                if (!this->Feed()) {
                    done = true;
                }
                break;

            case MPG123_ERR:
                return false;
                break;

            case MPG123_NEW_FORMAT: {
#if MPG123_DECODER_DEBUG > 0
                    int encoding = 0;

                    mpg123_getformat(
                        this->decoder,
                        &this->sampleRate,
                        &this->channels,
                        &encoding);

                    std::cerr << "output format:"
                        "\n  type: " << encoding <<
                        "\n  rate " << this->sampleRate <<
                        "\n  channels " << this->channels << "\n";
#endif
                }
                break;
        }

    } while (bytesWritten == 0 && !done);

    buffer->SetSamples(bytesWritten / this->sampleSizeBytes);

    return (bytesWritten > 0);
}

bool Mpg123Decoder::Feed() {
    if (this->fileStream) {
        unsigned char buffer[STREAM_FEED_SIZE];

        long bytesRead = this->fileStream->Read(&buffer, STREAM_FEED_SIZE);

        if (bytesRead) {
            if (mpg123_feed(this->decoder, buffer, bytesRead) == MPG123_OK) {
                return true;
            }
        }
    }

    return false;
}

bool Mpg123Decoder::Open(IDataStream *fileStream){
    if (this->decoder && fileStream) {
        this->fileStream = fileStream;

        if (mpg123_open_feed(this->decoder) == MPG123_OK) {
            int result = mpg123_param(
                this->decoder,
                MPG123_ADD_FLAGS,
                MPG123_FUZZY | MPG123_SEEKBUFFER | MPG123_GAPLESS | MPG123_QUIET,
                0);

            mpg123_set_filesize(this->decoder, this->fileStream->Length());

            /* reset output table */
            mpg123_format_none(this->decoder);

            /* force the output encoding to float32. note that this needs to
            be done before any data is fed to the decoder! */
            mpg123_format(
                this->decoder,
                this->sampleRate,
                MPG123_STEREO,
                MPG123_ENC_FLOAT_32);

            return true;
        }
    }

    return false;
}
