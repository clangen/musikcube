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

#include "Constants.h"
#include "GmeDecoder.h"
#include <core/sdk/IPreferences.h>
#include <cassert>

static const int BUFFER_SAMPLE_COUNT = 2048;
static const int CHANNELS = 2;
static const int SAMPLE_RATE = 44100;
static const int SAMPLES_PER_MS = (SAMPLE_RATE * CHANNELS) / 1000;
static const float F_SHRT_MAX = (float) SHRT_MAX;

extern IPreferences* prefs;

GmeDecoder::GmeDecoder() {
    this->buffer = new short[BUFFER_SAMPLE_COUNT];
}

GmeDecoder::~GmeDecoder() {
    gme_delete(this->gme);
    gme_free_info(this->info);
    this->gme = nullptr;
    this->info = nullptr;
    delete[] buffer;
}

bool GmeDecoder::Open(musik::core::sdk::IDataStream *stream) {
    this->stream = dynamic_cast<GmeDataStream*>(stream);
    assert(this->stream);
    auto length = stream->Length();
    const char* data = new char[length];
    if (stream->Read((void*) data, length) == length) {
        if (!gme_open_data(data, length, &this->gme, SAMPLE_RATE)) {
            int trackNum = this->stream->GetTrackNumber();

            if (prefs->GetBool(KEY_ENABLE_M3U, DEFAULT_ENABLE_M3U)) {
                std::string m3u = getM3uFor(this->stream->GetFilename());
                if (m3u.size()) {
                    gme_load_m3u(this->gme, m3u.c_str());
                }
            }

            bool error =
                !!gme_track_info(this->gme, &this->info, trackNum) ||
                !!gme_start_track(this->gme, trackNum);

            if (error) {
                gme_delete(this->gme);
                gme_free_info(this->info);
                this->gme = nullptr;
                this->info = nullptr;
            }
            else {
                if (this->info->length == -1) {
                    this->length = prefs->GetDouble(
                        KEY_DEFAULT_TRACK_LENGTH, DEFAULT_TRACK_LENGTH);

                    auto fadeLength = prefs->GetDouble(
                        KEY_TRACK_FADE_OUT_LENGTH, DEFAULT_FADE_OUT_LENGTH);

                    gme_set_fade(
                        this->gme,
                        (int)((this->length - fadeLength) * 1000.0),
                        (int)(fadeLength * 1000.0));
                }
                else {
                    this->length = (double) this->info->length / 1000.0;
                }
            }
        }
    }
    delete[] data;
    return this->gme != nullptr;
}

void GmeDecoder::Release() {
    delete this;
}

double GmeDecoder::SetPosition(double seconds) {
    if (this->gme) {
        gme_seek(this->gme, (int)(seconds * 1000.0));
        return (double) gme_tell(this->gme) / 1000.0;
    }
    return 0.0;
}

double GmeDecoder::GetDuration() {
    return this->length;
}

bool GmeDecoder::GetBuffer(IBuffer *target) {
    if (this->gme) {
        if (!gme_track_ended(this->gme) &&
            !gme_play(this->gme, BUFFER_SAMPLE_COUNT, this->buffer))
        {
            target->SetChannels(CHANNELS);
            target->SetSampleRate(SAMPLE_RATE);
            target->SetSamples(BUFFER_SAMPLE_COUNT);
            float* dst = target->BufferPointer();
            for (size_t i = 0; i < BUFFER_SAMPLE_COUNT; i++) {
                dst[i] = (float) this->buffer[i] / F_SHRT_MAX;
            }
            return true;
        }
    }
    return false;
}

bool GmeDecoder::Exhausted() {
    if (this->gme) {
        return gme_track_ended(this->gme);
    }
    return true;
}
