//////////////////////////////////////////////////////////////////////////////
// Copyright � 2007, mC2 team
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
#include "FlacDecoder.h"
#include <complex>

static inline void copy(float* dst, float* src, size_t count) {
#ifdef WIN32
    CopyMemory(dst, src, count * sizeof(float));
#else
    memcpy(dst, src, count * sizeof(float));
#endif
}

FlacDecoder::FlacDecoder()
: decoder(NULL)
, outputBufferSize(0)
, outputBuffer(NULL)
, channels(0)
, sampleRate(0)
, bitsPerSample(0)
, totalSamples(0) {
    this->decoder = FLAC__stream_decoder_new();
}

FlacDecoder::~FlacDecoder() {
    if (this->decoder) {
        FLAC__stream_decoder_delete(this->decoder);
        this->decoder = NULL;
    }

    if (this->outputBuffer) {
        delete this->outputBuffer;
        this->outputBuffer = NULL;
    }
}

FLAC__StreamDecoderReadStatus FlacDecoder::FlacRead(
    const FLAC__StreamDecoder *decoder,
    FLAC__byte buffer[],
    size_t *bytes,
    void *clientData)
{
    size_t readBytes = (size_t)((FlacDecoder*) clientData)->stream->Read(buffer,(long)(*bytes));
    *bytes = readBytes;

    if (readBytes == 0) {
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    else if(readBytes == (size_t) -1) {
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__bool FlacDecoder::FlacEof(const FLAC__StreamDecoder *decoder, void *clientData) {
    return ((FlacDecoder*) clientData)->stream->Eof() ? 1 : 0;
}

FLAC__StreamDecoderSeekStatus FlacDecoder::FlacSeek(
    const FLAC__StreamDecoder *decoder,
    FLAC__uint64 absolute_byte_offset,
    void *clientData)
{
    if(((FlacDecoder*) clientData)->stream->SetPosition((long) absolute_byte_offset)) {
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }

    return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

FLAC__StreamDecoderTellStatus FlacDecoder::FlacTell(
    const FLAC__StreamDecoder *decoder,
    FLAC__uint64 *absolute_byte_offset,
    void *clientData)
{
    *absolute_byte_offset = (FLAC__uint64)((FlacDecoder*) clientData)->stream->Position();

    if(*absolute_byte_offset == (FLAC__uint64) -1) {
        return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    }

    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderLengthStatus FlacDecoder::FlacFileSize(
    const FLAC__StreamDecoder *decoder,
    FLAC__uint64 *stream_length,
    void *clientData)
{
    *stream_length = (FLAC__uint64)((FlacDecoder*) clientData)->stream->Length();

    if(*stream_length <= 0) {
        return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
    }

    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

bool FlacDecoder::Open(musik::core::io::IDataStream *stream){
    this->stream = stream;

	FLAC__StreamDecoderInitStatus init_status =
        FLAC__stream_decoder_init_stream(
            this->decoder,
            FlacDecoder::FlacRead,
            FlacDecoder::FlacSeek,
            FlacDecoder::FlacTell,
            FlacDecoder::FlacFileSize,
            FlacDecoder::FlacEof,
            FlacDecoder::FlacWrite,
            FlacDecoder::FlacMetadata,
            FlacDecoder::FlacError,
            this);

    if (init_status == FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        FLAC__stream_decoder_process_until_end_of_metadata(this->decoder);
        return true;
    }

    return false;
}

void FlacDecoder::FlacError(
    const FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderErrorStatus status,
    void *clientData)
{
    /* nothing for us to do here... */
}

void FlacDecoder::FlacMetadata(
    const FLAC__StreamDecoder *decoder,
    const FLAC__StreamMetadata *metadata,
    void *clientData)
{
    FlacDecoder *fdec = (FlacDecoder*)clientData;

	if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        fdec->totalSamples = metadata->data.stream_info.total_samples;
        fdec->sampleRate = metadata->data.stream_info.sample_rate;
        fdec->channels = metadata->data.stream_info.channels;
        fdec->bitsPerSample = metadata->data.stream_info.bits_per_sample;
    }
}

FLAC__StreamDecoderWriteStatus FlacDecoder::FlacWrite(
    const FLAC__StreamDecoder *decoder,
    const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[],
    void *clientData)
{
    FlacDecoder *fdec = (FlacDecoder*) clientData;
    int sampleCount  = fdec->channels * frame->header.blocksize;

    /* initialize the output buffer if it doesn't exist */
    if (fdec->outputBufferSize == 0) {
        delete fdec->outputBuffer;
        fdec->outputBuffer = NULL;
        fdec->outputBufferSize = 0;
        fdec->outputBuffer = new float[sampleCount];
    }

    /* if we already have a buffer with an offset, let's append to it. realloc
    the buffer so there's enough room */
    if (fdec->outputBuffer && fdec->outputBufferSize > 0) {
        float *oldBuffer = fdec->outputBuffer;
        fdec->outputBuffer = new float[sampleCount + fdec->outputBufferSize];
        copy(fdec->outputBuffer, oldBuffer, fdec->outputBufferSize);
        delete oldBuffer;
    }

    /* we need to convert the fixed point samples to floating point samples. figure
    out the maximum amplitude of the fixed point samples based on the resolution */
    float maxAmplitude  = pow(2.0f, (fdec->bitsPerSample - 1));

    /* run the conversion */
	for (unsigned int i = 0; i < frame->header.blocksize; ++i) {
        for (int j = 0; j < fdec->channels; ++j) {
            fdec->outputBuffer[fdec->outputBufferSize] = (((float) buffer[j][i]) / maxAmplitude);
            fdec->outputBufferSize++;
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::Destroy() {
	delete this;
}

double FlacDecoder::SetPosition(double seconds) {
    FLAC__uint64 seekToSample = (FLAC__uint64)(this->sampleRate * seconds);

    if (FLAC__stream_decoder_seek_absolute(this->decoder, seekToSample)) {
        return seconds;
    }

    return -1;
}

bool FlacDecoder::GetBuffer(IBuffer *buffer) {
    buffer->SetSampleRate(this->sampleRate);
    buffer->SetChannels(this->channels);

    /* read the next chunk */
    if (FLAC__stream_decoder_process_single(this->decoder)) {
        if (this->outputBuffer && this->outputBufferSize > 0) {
            buffer->SetSamples(this->outputBufferSize / this->channels);
            copy(buffer->BufferPointer(), this->outputBuffer, this->outputBufferSize);
            this->outputBufferSize  = 0;
            return true;
        }
    }

    return false;
}
