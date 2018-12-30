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
#include "M4aDecoder.h"
#include <cstring>
#include <string>
#include <stdlib.h>

using musik::core::sdk::IDataStream;
using musik::core::sdk::IBuffer;

static uint32_t streamReadCallback(void *userData, void *buffer, uint32_t length) {
    IDataStream *stream = static_cast<IDataStream*>(userData);
    return (uint32_t) stream->Read(buffer, length);
}

static uint32_t streamSeekCallback(void *userData, uint64_t position) {
    IDataStream *stream = static_cast<IDataStream*>(userData);
    return (uint32_t) stream->SetPosition((long) position);
}

static int FindAacTrack(mp4ff_t *infile) {
    int i, rc;
    int numTracks = mp4ff_total_tracks(infile);

    for (i = 0; i < numTracks; i++) {
        unsigned char *buff = NULL;
        int size = 0;
        mp4AudioSpecificConfig mp4ASC;

        mp4ff_get_decoder_config(infile, i, &buff, (unsigned int *) &size);

        if (buff) {
            rc = NeAACDecAudioSpecificConfig(buff, size, &mp4ASC);
            free(buff);

            if (rc < 0) {
                continue;
            }

            return i;
        }
    }

    /* can't decode this */
    return -1;
}

M4aDecoder::M4aDecoder() {
    this->decoder = nullptr;
    this->decoderFile = nullptr;
    memset(&decoderCallbacks, 0, sizeof(this->decoderCallbacks));
    this->duration = -1.0f;
    this->exhausted = false;
}

M4aDecoder::~M4aDecoder() {
}

bool M4aDecoder::Open(musik::core::sdk::IDataStream *stream) {
    decoder = NeAACDecOpen();

    if (!decoder) {
        this->exhausted = true;
        return false;
    }

    NeAACDecConfigurationPtr config;
    config = NeAACDecGetCurrentConfiguration(decoder);

    config->outputFormat = FAAD_FMT_FLOAT;
    config->downMatrix = 0;

    int ret = NeAACDecSetConfiguration(decoder, config);

    decoderCallbacks.read = streamReadCallback;
    decoderCallbacks.seek = streamSeekCallback;
    decoderCallbacks.user_data = stream;

    decoderFile = mp4ff_open_read(&decoderCallbacks);

    if (decoderFile) {
        if ((audioTrackId = FindAacTrack(decoderFile)) >= 0) {
            unsigned char* buffer = NULL;
            unsigned int bufferSize = 0;

            mp4ff_get_decoder_config(
                decoderFile, audioTrackId, &buffer, &bufferSize);

            const float scale =
                (float) mp4ff_time_scale(decoderFile, audioTrackId);

            const float duration =
                (float) mp4ff_get_track_duration(decoderFile, audioTrackId);

            if (scale > 0 && duration > 0) {
                this->duration = duration / scale;
            }

            if (buffer) {
                if (NeAACDecInit2(
                    decoder,
                    buffer,
                    bufferSize,
                    &this->sampleRate,
                    &this->channelCount) >= 0)
                {
                    this->totalSamples = mp4ff_num_samples(decoderFile, audioTrackId);
                    this->decoderSampleId = 0;
                    free(buffer);
                    return true;
                }

                free(buffer);
            }
        }
    }

    return false;
}

void M4aDecoder::Release() {
    mp4ff_close(decoderFile);

    if (decoder) {
        NeAACDecClose(decoder);
        decoder = NULL;
    }

    delete this;
}

double M4aDecoder::SetPosition(double seconds) {
    int64_t duration;
    float fms = (float) seconds * 1000;
    int32_t skip_samples = 0;

    duration = (int64_t)(fms / 1000.0 * sampleRate + 0.5);

    decoderSampleId = mp4ff_find_sample_use_offsets(
        decoderFile, audioTrackId, duration, &skip_samples);

    return seconds;
}

double M4aDecoder::GetDuration() {
    return this->duration;
}

bool M4aDecoder::GetBuffer(IBuffer* target) {
    if (this->decoderSampleId >= 0) {
        void* sampleBuffer = NULL;
        unsigned char* encodedData = NULL;
        unsigned int encodedDataLength = 0;
        NeAACDecFrameInfo frameInfo;

        long duration = mp4ff_get_sample_duration(
            decoderFile, audioTrackId, decoderSampleId);

        if (duration > 0) {
            /* read the raw data required */

            int rc =
                mp4ff_read_sample(
                    decoderFile,
                    audioTrackId,
                    decoderSampleId,
                    &encodedData,
                    &encodedDataLength);

            decoderSampleId++;

            if (rc == 0 || encodedData == NULL) {
                this->exhausted = true;
                return false;
            }

            sampleBuffer =
                NeAACDecDecode(
                    decoder,
                    &frameInfo,
                    encodedData,
                    encodedDataLength);

            free(encodedData);

            if (sampleBuffer && frameInfo.error <= 0 && decoderSampleId <= this->totalSamples) {
                target->SetSampleRate(frameInfo.samplerate);
                target->SetChannels(frameInfo.channels);
                target->SetSamples(frameInfo.samples);

                memcpy(
                    static_cast<void*>(target->BufferPointer()),
                    static_cast<void*>(sampleBuffer),
                    sizeof(float) * frameInfo.samples);

                return true;
            }
        }
    }

    this->exhausted = true;
    return false;
}
