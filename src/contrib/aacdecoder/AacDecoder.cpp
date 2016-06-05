#include "stdafx.h"
#include "AacDecoder.h"

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

AacDecoder::AacDecoder() {
    this->decoder = NULL;
    this->decoderFile = NULL;
    memset(&decoderCallbacks, 0, sizeof(this->decoderCallbacks));
}

AacDecoder::~AacDecoder() {
}

bool AacDecoder::Open(musik::core::io::IDataStream *stream)
{
    unsigned char* buffer;
    unsigned int buffer_size;
    NeAACDecConfigurationPtr config;

    decoder = NeAACDecOpen();
    if (!decoder) {
        return false;
    }

    config = NeAACDecGetCurrentConfiguration(decoder);

    config->outputFormat = FAAD_FMT_FLOAT;
    config->downMatrix = 0;

    int ret = NeAACDecSetConfiguration(decoder, config);

    decoderCallbacks.read = streamReadCallback;
    decoderCallbacks.seek = streamSeekCallback;
    decoderCallbacks.user_data = stream;

    decoderFile = mp4ff_open_read(&decoderCallbacks);
    if (!decoderFile) {
        return false;
    }

    if ((audioTrackId = FindAacTrack(decoderFile)) < 0) {
        return false;
    }

    buffer = NULL;
    buffer_size = 0;
    mp4ff_get_decoder_config(decoderFile, audioTrackId, &buffer, &buffer_size);

    if (!buffer) {
        return false;
    }

    if (NeAACDecInit2(
        decoder,
        buffer,
        buffer_size,
        &this->sampleRate,
        &this->channelCount) < 0)
    {
        if (buffer) {
            free(buffer);
        }

        return false;
    }

    free(buffer);

    this->totalSamples = mp4ff_num_samples(decoderFile, audioTrackId);
    decoderSampleId = 0;

    return true;
}

void AacDecoder::Destroy(void)
{
    mp4ff_close(decoderFile);

    if (decoder) {
        NeAACDecClose(decoder);
        decoder = NULL;
    }

    delete this;
}

double AacDecoder::SetPosition(double seconds) {
    int64_t duration;
    float fms = (float) seconds * 1000;
    int32_t skip_samples = 0;

    duration = (int64_t)(fms / 1000.0 * sampleRate + 0.5);

    decoderSampleId = mp4ff_find_sample_use_offsets(
        decoderFile, audioTrackId, duration, &skip_samples);

    return seconds;
}

bool AacDecoder::GetBuffer(IBuffer* target) {
    if (this->decoderSampleId < 0) {
        return false;
    }

    void* sampleBuffer = NULL;
    unsigned char* encodedData = NULL;
    unsigned int encodedDataLength = 0;
    NeAACDecFrameInfo frameInfo;

    /* get acces unit from MP4 file */

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
