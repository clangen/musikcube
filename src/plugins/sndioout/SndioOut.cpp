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

#include "SndioOut.h"

#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>
#include <math.h>
#include <limits.h>
#include <iostream>

#define DUMPSTATE() std::cerr << "handle=" << this->handle << " state=" << this->state << "\n";
#define ERROR(str) std::cerr << "SndioOut Error: " << str << "\n";
#define INFO(str) std::cerr << "SndioOut Info: " << str << "\n";
#define LOCK() std::unique_lock<std::recursive_mutex> lock(this->mutex);
#define PREF_DEVICE_ID "device_id"

using namespace musik::core::sdk;

static char* deviceId = nullptr;

extern "C" void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    size_t len = prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    if (len > 1) {
        delete[] deviceId;
        deviceId = new char[len];
        prefs->GetString(PREF_DEVICE_ID, deviceId, len, "");
        prefs->Save();
        INFO("setting deviceId to " + std::string(deviceId));
    }
}

SndioOut::SndioOut() {
    this->volume = 1.0f;
    this->state = StateStopped;
    this->handle = nullptr;
    this->buffer = nullptr;
    this->bufferSamples = 0;
    this->latency = 0.0;
    this->pars = { 0 };
    this->ditherState = 0.0;
}

SndioOut::~SndioOut() {
    this->Stop();
    delete[] this->buffer;
    this->bufferSamples = 0;
}

void SndioOut::Release() {
    delete this;
}

void SndioOut::Pause() {
    INFO("Pause()")
    LOCK()
    if (this->handle && this->state == StatePlaying) {
        if (!sio_stop(this->handle)) {
            ERROR("pause failed")
            this->Stop();
        }
        else {
            INFO("paused")
            this->state = StatePaused;
        }
    }
}

void SndioOut::Resume() {
    INFO("Resume()")
    LOCK()
    if (this->handle && this->state == StatePaused) {
        if (!sio_start(this->handle)) {
            ERROR("resume failed")
            this->Stop();
        }
        else {
            INFO("playing")
        }
    }
    this->state = StatePlaying;
}

void SndioOut::SetVolume(double volume) {
    this->volume = volume;

    if (this->handle) {
        sio_setvol(this->handle, lround(volume * SIO_MAXVOL));
    }
}

double SndioOut::GetVolume() {
    return this->volume;
}

void SndioOut::Stop() {
    INFO("Stop()")
    LOCK()
    if (this->handle) {
        sio_close(this->handle);
    }
    this->handle = nullptr;
    this->pars = { 0 };
    this->latency = 0;
    this->state = StateStopped;
}

void SndioOut::Drain() {
    INFO("Drain()")
    LOCK()
    if (this->handle) {
        sio_stop(this->handle);
        sio_start(this->handle);
    }
}

IDeviceList* SndioOut::GetDeviceList() {
    return nullptr;
}

bool SndioOut::SetDefaultDevice(const char* deviceId) {
    return false;
}

IDevice* SndioOut::GetDefaultDevice() {
    return nullptr;
}

bool SndioOut::InitDevice(IBuffer *buffer) {
    const char* device = (::deviceId && strlen(::deviceId)) ? deviceId : nullptr;

    this->handle = sio_open(device, SIO_PLAY, 0);

    if (this->handle == nullptr) {
        return false;
    }

    int n = 1; bool littleEndian = *(char *) &n == 1;

    sio_initpar(&this->pars);
    this->pars.pchan = buffer->Channels();
    this->pars.rate = buffer->SampleRate();
    this->pars.sig = 1;
    this->pars.le = !!littleEndian;
    this->pars.bits = 16;

    /* stolen from cmus; presumeably they've already iterated
    this value and it should be a reasonable default */
    this->pars.appbufsz = pars.rate * 300 / 1000;

    if (!sio_setpar(this->handle, &this->pars)) {
        return false;
    }

    if (!sio_getpar(this->handle, &this->pars)) {
        return false;
    }

    if (!sio_start(this->handle)) {
        return false;
    }

    this->latency = (double)
        this->pars.bufsz /
        this->pars.pchan /
        this->pars.rate;

    this->SetVolume(this->volume);

    return true;
}

int SndioOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    //DUMPSTATE()

    if (this->handle == nullptr) {
        INFO("initializing device...");
        if (!this->InitDevice(buffer)) {
            ERROR("failed to initialize device")
            return OutputInvalidState;
        }
        INFO("initialized");
        this->state = StateStopped;
    }

    if (!this->handle || this->state == StatePaused) {
        return OutputInvalidState;
    }

    this->state = StatePlaying;

    /* convert to 16-bit PCM */
    long samples = buffer->Samples();
    if (!this->buffer || samples > this->bufferSamples) {
        delete[] this->buffer;
        this->buffer = new short[samples];
        this->bufferSamples = samples;
    }

    float* src = buffer->BufferPointer();
    short* dst = this->buffer;
    for (long i = 0; i < samples; i++) {
        float sample = *src;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        sample *= SHRT_MAX;

        /* triangle (high pass) dither, based on Audacity's
        implementation */
        float r = (rand() / (float) RAND_MAX - 0.5f);
        sample = sample + r - this->ditherState;
        this->ditherState = r;

        *dst = sample;
        ++dst; ++src;
    }

    /* write the entire output buffer. this may require multiple passes;
    that's ok, just loop until we're done */
    char* data = (char*) this->buffer;
    size_t dataLength = samples * sizeof(short);
    size_t totalWritten = 0;

    while (totalWritten < dataLength && this->state == StatePlaying) {
        size_t remaining = dataLength - totalWritten;
        size_t written = 0;

        {
            LOCK()
            written = sio_write(this->handle, data, remaining);
        }

        if (written == 0) {
            break;
        }

        totalWritten += written;
        data += written;
    }

    provider->OnBufferProcessed(buffer);
    return OutputBufferWritten;
}

double SndioOut::Latency() {
    return this->latency;
}
