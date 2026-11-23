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
#include "shared.h"
#include <string>
#include <limits>

using namespace musik::core::sdk;

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
    if (rate > std::numeric_limits<int>::max() || bitrate > std::numeric_limits<int>::max()) {
        return false;
    }
    lame = lame_init();
    lame_set_in_samplerate(lame, static_cast<int>(rate));
    lame_set_VBR(lame, vbr_off);
    lame_set_VBR_mean_bitrate_kbps(lame, static_cast<int>(bitrate));
    lame_set_brate(lame, static_cast<int>(bitrate));
    lame_set_quality(lame, 5);
    lame_set_out_samplerate(lame, static_cast<int>(rate));
    lame_set_bWriteVbrTag(lame, 1);
    lame_init_params(lame);
    return true;
}

void LameEncoder::Release() {
    lame_close(lame);
    lame = nullptr;
    delete this;
}

int LameEncoder::Encode(const IBuffer* pcm, char** data) {
    /* calculated according to lame.h */
    size_t channels = pcm->Channels();
    if (channels == 0) {
        return 0;
    }
    size_t numSamples = pcm->Samples() / channels;
    if (numSamples > (std::numeric_limits<int>::max() - 7200) / 2) {
        return 0;
    }
    size_t requiredBytes = numSamples + numSamples / 4 + 7201;

    encodedBytes.reset(requiredBytes);

    int encodeCount = -1;

    /* stereo */
    if (pcm->Channels() == 2) {
        encodeCount =
            lame_encode_buffer_interleaved_ieee_float(
                lame,
                pcm->BufferPointer(),
                static_cast<int>(numSamples),
                encodedBytes.data,
                static_cast<int>(encodedBytes.length));
    }
    /* non-stereo. convert to stereo! */
    else {
        downmix.reset(numSamples * 2);

        float* from = pcm->BufferPointer();
        float* to = downmix.data;

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
                downmix.data,
                static_cast<int>(numSamples),
                encodedBytes.data,
                static_cast<int>(encodedBytes.length));
    }

    if (encodeCount < 0) {
        return 0;
    }

    encodedBytes.length = (size_t)encodeCount;

    *data = reinterpret_cast<char*>(encodedBytes.data);
    return encodeCount;
}

int LameEncoder::Flush(char** data) {
    /* 7200 bytes minimum is required for the flush op; see lame.h */
    if (encodedBytes.length < 7200) {
        encodedBytes.reset(7200);
    }

    int count = lame_encode_flush(
        lame,
        encodedBytes.data,
        static_cast<int>(std::min(encodedBytes.length, static_cast<size_t>(std::numeric_limits<int>::max()))));

    *data = reinterpret_cast<char*>(encodedBytes.data);
    return count;
}

void LameEncoder::Finalize(const char* uri) {
    /* need to make sure we write the LAME header, otherwise
    gapless playback won't work! */
    unsigned char header[2800]; /* max frame size */
    size_t headerSize = lame_get_lametag_frame(lame, header, sizeof(header));
    if (headerSize) {
#ifdef WIN32
        FILE* fp = nullptr;
        _wfopen_s(&fp, utf8to16(uri).c_str(), L"r+b");
#else
        FILE* fp = fopen(uri, "r+b");
#endif
        if (fp) {
            if (!fseek(fp, 0, SEEK_SET)) {
                size_t written = fwrite(header, 1, headerSize, fp);
                ++written;
            }
            fclose(fp);
        }
    }
}
