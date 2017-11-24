//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "OggEncoder.h"
#include <random>

/* fre:ac/BoCA has an excellent example of vorbis encoder usage, a lot of code
was adapted (stolen) from here: https://github.com/enzo1982/BoCA/blob/master/components/encoder/vorbis/vorbis.cpp */

void OggEncoder::Initialize(size_t rate, size_t channels, size_t bitrate) {
    ogg_stream_state os = { 0 };
    ogg_page og = { 0 };
    ogg_packet op = { 0 };
    vorbis_info vi = { 0 };
    vorbis_comment vc = { 0 };
    vorbis_dsp_state vd = { 0 };
    vorbis_block vb = { 0 };
    this->bitrate = bitrate * 1000;
    this->headerWritten = false;
}

void OggEncoder::Release() {
    ogg_stream_clear(&os);
    vorbis_block_clear(&vb);
    vorbis_dsp_clear(&vd);
    vorbis_comment_clear(&vc);
    vorbis_info_clear(&vi);
    delete this;
}

int OggEncoder::Encode(const IBuffer* pcm, char** data) {
    encodedData.reset();

    int written = 0;

    if (!headerWritten) { /* ehh... */
        /* init the decoder */
        vorbis_info_init(&vi);
        if (vorbis_encode_init(&vi, pcm->Channels(), pcm->SampleRate(), bitrate, bitrate, bitrate) != 0) {
            return -1;
        }

        vorbis_comment_init(&vc);
        vorbis_analysis_init(&vd, &vi);
        vorbis_block_init(&vd, &vb);
        ogg_stream_init(&os, rand());

        ogg_packet header;
        ogg_packet headerComm;
        ogg_packet headerCode;

        vorbis_analysis_headerout(&vd, &vc, &header, &headerComm, &headerCode);
        ogg_stream_packetin(&os, &header);
        ogg_stream_packetin(&os, &headerComm);
        ogg_stream_packetin(&os, &headerCode);

        written += WritePackets(true);

        headerWritten = true;
    }

    /* now... on to the audio data... let's prep the encoder */
    int channels = pcm->Channels();
    int samplesPerChannel = pcm->Samples() / channels;

    float** buffer = vorbis_analysis_buffer(&vd, samplesPerChannel);
    float* input = pcm->BufferPointer();

    /*
        musikcube expects:

            SPEAKER_FRONT_LEFT 0x1
            SPEAKER_FRONT_RIGHT 0x2
            SPEAKER_FRONT_CENTER 0x4
            SPEAKER_LOW_FREQUENCY 0x8
            SPEAKER_BACK_LEFT 0x10
            SPEAKER_BACK_RIGHT 0x20

        OGG expects:

            One channel: the stream is monophonic
            Two channels: the stream is stereo. channel order: left, right.
            Three channels: the stream is a 1d-surround encoding. channel order: left, center, right
            Four channels: the stream is quadraphonic surround. channel order: front left, front right, rear left, rear right
            Five channels: the stream is five-channel surround. channel order: front left, front center, front right, rear left, rear right
            Six channels: (used in Dolby Digital/AC3) the stream is 5,1 surround. channel order: front left, front center, front right, rear left, rear right, LFE (the sixth channel is entirely bass).

        ... so let's re-order when writing to our output buffer ...
    */

    for (long i = 0, s = 0; i < pcm->Samples(); i+= channels, s++) {
        if (channels == 1) {
            buffer[0][s] = input[i];
        }
        else if (channels == 2) {
            buffer[0][s] = input[i + 0]; /* l */
            buffer[1][s] = input[i + 1]; /* r */
        }
        else if (channels == 3) {
            buffer[0][s] = input[i + 0]; /* l */
            buffer[1][s] = input[i + 2]; /* c */
            buffer[2][s] = input[i + 1]; /* r */
        }
        else if (channels == 4) {
            buffer[0][s] = input[i + 0]; /* l */
            buffer[1][s] = input[i + 1]; /* r */
            buffer[2][s] = input[i + 2]; /* rl */
            buffer[3][s] = input[i + 3]; /* rr */
        }
        else if (channels == 5) {
            buffer[0][s] = input[i + 0]; /* fl */
            buffer[1][s] = input[i + 2]; /* c */
            buffer[2][s] = input[i + 1]; /* fr */
            buffer[3][s] = input[i + 3]; /* rl */
            buffer[4][s] = input[i + 4]; /* rr */
        }
        else if (channels == 6) {
            buffer[0][s] = input[i + 0]; /* fl */
            buffer[1][s] = input[i + 2]; /* fc */
            buffer[2][s] = input[i + 1]; /* rf */
            buffer[3][s] = input[i + 5]; /* bl */
            buffer[4][s] = input[i + 3]; /* br */
            buffer[5][s] = input[i + 4]; /* lfe */
        }
    }

    if (vorbis_analysis_wrote(&vd, samplesPerChannel) != 0) {
        return -1;
    }

    /* encode and write to the buffer */
    while (vorbis_analysis_blockout(&vd, &vb) == 1) {
        vorbis_analysis(&vb, nullptr);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op) == 1) {
            ogg_stream_packetin(&os, &op);
            written += WritePackets(false);
        }
    }

    *data = encodedData.data;
    return written;
}

int OggEncoder::Flush(char** data) {
    encodedData.reset();

    int written = 0;
    vorbis_analysis_wrote(&vd, 0);

    while (vorbis_analysis_blockout(&vd, &vb) == 1) {
        vorbis_analysis(&vb, nullptr);
        vorbis_bitrate_addblock(&vb);

        while (vorbis_bitrate_flushpacket(&vd, &op)) {
            ogg_stream_packetin(&os, &op);
            written += WritePackets(false); /* why not true? */
        }
    }

    *data = encodedData.data;
    return written;
}

void OggEncoder::Finalize(const char* uri) {
}

int OggEncoder::WritePackets(bool flush) {
    int written = 0;
    int result = 0;

    do {
        if (flush) {
            result = ogg_stream_flush(&os, &og);
        }
        else {
            result = ogg_stream_pageout(&os, &og);
        }

        if (result == 0) {
            break;
        }

        written += encodedData.append((const char*) og.header, og.header_len);
        written += encodedData.append((const char*) og.body, og.body_len);
    } while (true);

    return written;
}