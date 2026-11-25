//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

#include "LameEncoder.h"
#include <climits>
#include <string>


#ifdef WIN32
#include <windows.h>
static inline std::wstring utf8to16(const char* utf8) {
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, 0, 0);
    if (size <= 0) return L"";
    wchar_t* buffer = new wchar_t[size];
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buffer, size);
    std::wstring utf16fn(buffer);
    delete[] buffer;
    return utf16fn;
}
#endif

LameEncoder::LameEncoder() {
    this->lame = nullptr;
}

bool LameEncoder::Initialize(size_t rate, size_t channels, size_t bitrate) {
    lame = lame_init();
    if (!lame || !rate || !channels || !bitrate || rate > INT_MAX || bitrate > INT_MAX) {
        return false;
    }
    lame_set_in_samplerate(lame, static_cast<int>(rate));
    lame_set_VBR(lame, vbr_off);
    lame_set_VBR_mean_bitrate_kbps(lame, bitrate);
    lame_set_brate(lame, bitrate);
    lame_set_quality(lame, 5);
    lame_set_out_samplerate(lame, rate);
    lame_set_bWriteVbrTag(lame, 1);
    return lame_init_params(lame) >= 0;
}

void LameEncoder::Release() {
    if (lame) {
        lame_close(lame);
    }
    lame = nullptr;
    delete this;
}

int LameEncoder::Encode(const mc_pcm_view& pcm, char** data) {
    /* calculated according to lame.h */
    size_t channels = pcm.format.channels;
    size_t numSamples = pcm.frame_count;
    if (!channels || numSamples > static_cast<size_t>((INT_MAX - 7200) / 5) * 4) {
        return -1;
    }
    size_t requiredBytes = numSamples + (numSamples + 3) / 4 + 7200;

    encodedBytes.resize(requiredBytes);

    int encodeCount = -1;

    /* stereo */
    if (pcm.format.channels == 2) {
        encodeCount =
            lame_encode_buffer_interleaved_ieee_float(
                lame,
                pcm.data,
                numSamples,
                encodedBytes.data(),
                encodedBytes.size());
    }
    /* non-stereo. convert to stereo! */
    else {
        downmix.resize(numSamples * 2);

        const float* from = pcm.data;
        float* to = downmix.data();

        if (channels == 1) {
            /* mono: duplicate the channel to L and R */
            for (size_t i = 0; i < numSamples; i++) {
                *(to + 0) = *from;
                *(to + 1) = *from;
                to += 2;
                ++from;
            }
        }
        else {
            /* 3+ channels: only take the first two channels */
            for (size_t i = 0; i < numSamples; i++) {
                *(to + 0) = *(from + 0);
                *(to + 1) = *(from + 1);
                to += 2;
                from += channels;
            }
        }

        encodeCount =
            lame_encode_buffer_interleaved_ieee_float(
                lame,
                downmix.data(),
                numSamples,
                encodedBytes.data(),
                encodedBytes.size());
    }

    if (encodeCount < 0) {
        return -1;
    }

    encodedBytes.resize(static_cast<size_t>(encodeCount));

    *data = reinterpret_cast<char*>(encodedBytes.data());
    return encodedBytes.size();
}

int LameEncoder::Flush(char** data) {
    /* 7200 bytes minimum is required for the flush op; see lame.h */
    if (encodedBytes.size() < 7200) {
        encodedBytes.resize(7200);
    }

    int count = lame_encode_flush(
        lame,
        encodedBytes.data(),
        encodedBytes.size());

    *data = reinterpret_cast<char*>(encodedBytes.data());
    return count;
}

bool LameEncoder::Finalize(const char* uri) {
    /* a complete cached MP3 can now advertise the encoder delay and padding. */
    unsigned char header[2800];
    const size_t headerSize = lame_get_lametag_frame(lame, header, sizeof(header));
    if (!headerSize) {
        return true;
    }
    if (headerSize > sizeof(header)) {
        return false;
    }
#ifdef WIN32
    FILE* file = _wfopen(utf8to16(uri).c_str(), L"r+b");
#else
    FILE* file = fopen(uri, "r+b");
#endif
    if (!file) {
        return false;
    }
    const bool written = fwrite(header, 1, headerSize, file) == headerSize;
    return fclose(file) == 0 && written;
}
