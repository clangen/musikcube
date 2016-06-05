#include "stdafx.h"
#include "M4aDecoder.h"
#include <string.h>

using musik::core::io::IDataStream;
using musik::core::audio::IBuffer;

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
    this->decoder = NULL;
    this->decoderFile = NULL;
    memset(&decoderCallbacks, 0, sizeof(this->decoderCallbacks));
}

M4aDecoder::~M4aDecoder() {
}

bool M4aDecoder::Open(musik::core::io::IDataStream *stream)
{
    decoder = NeAACDecOpen();
    if (!decoder) {
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

void M4aDecoder::Destroy(void)
{
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

bool M4aDecoder::GetBuffer(IBuffer* target) {
    if (this->decoderSampleId < 0) {
        return false;
    }

    void* sampleBuffer = NULL;
    unsigned char* encodedData = NULL;
    unsigned int encodedDataLength = 0;
    NeAACDecFrameInfo frameInfo;

    long duration = mp4ff_get_sample_duration(
        decoderFile, audioTrackId, decoderSampleId);

    if (duration <= 0) {
        return false;
    }

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
        return false;
    }

    sampleBuffer =
        NeAACDecDecode(
            decoder,
            &frameInfo,
            encodedData,
            encodedDataLength);

    free(encodedData);

    if (frameInfo.error > 0) {
        return false;
    }

    if (decoderSampleId > this->totalSamples) {
        return false;
    }

    target->SetSampleRate(frameInfo.samplerate);
    target->SetChannels(frameInfo.channels);
    target->SetSamples(frameInfo.samples / frameInfo.channels);

    memcpy(
        static_cast<void*>(target->BufferPointer()),
        static_cast<void*>(sampleBuffer),
        sizeof(float) * frameInfo.samples);

    return true;
}
