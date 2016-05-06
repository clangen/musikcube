#include "stdafx.h"
#include "CddaDecoder.h"

#define CDDA_SAMPLE_RATE 44100
#define CDDA_BUFFER_SIZE 4096
#define BYTES_PER_RAW_SAMPLE 2

CddaDecoder::CddaDecoder() {
    this->data = NULL;
    this->buffer = new BYTE[CDDA_BUFFER_SIZE];
}

CddaDecoder::~CddaDecoder() {
    delete[] this->buffer;
}

void CddaDecoder::Destroy() {
    delete this;
}

bool CddaDecoder::Open(IDataStream* data) {
    this->data = (CddaDataStream *) data;
    return (data != NULL);
}

double CddaDecoder::SetPosition(double seconds) {
    LONGLONG bytesPerSecond = (CDDA_SAMPLE_RATE * data->GetChannelCount() * sizeof(short));
    LONGLONG offset = (LONGLONG)((float)bytesPerSecond * seconds);
    if (this->data->SetPosition(offset)) {
        return seconds;
    }

    return -1;
}

bool CddaDecoder::GetBuffer(IBuffer *buffer) {
    int channels = data->GetChannelCount();
    buffer->SetSamples(CDDA_BUFFER_SIZE / BYTES_PER_RAW_SAMPLE / channels);
    buffer->SetChannels(data->GetChannelCount());
    buffer->SetSampleRate(CDDA_SAMPLE_RATE);

    float* target = buffer->BufferPointer();

    PositionType count = this->data->Read(
        (void *) this->buffer, CDDA_BUFFER_SIZE);

    if (count > 0) {
        short* t = (short*) this->buffer;

        for (int x = 0; x < (count / BYTES_PER_RAW_SAMPLE); x++)
        {
            target[x] = (float) t[x] / 16384.0f;
        }

        buffer->SetSamples(count / BYTES_PER_RAW_SAMPLE / channels);

        return true;
    }

    return false;
}
