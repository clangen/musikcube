//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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

#include "PulseOut.h"
#include <pulse/volume.h>
#include <math.h>

using namespace musik::core::sdk;

typedef std::unique_lock<std::recursive_mutex> Lock;

PulseOut::PulseOut() {
    std::cerr << "PulseOut::PulseOut() called" << std::endl;
    this->audioConnection = nullptr;
    this->state = StateStopped;
    this->volume = 1.0f;
    this->channels = 0;
    this->rate = 0;
}

PulseOut::~PulseOut() {
    std::cerr << "PulseOut: destructor\n";
    this->CloseDevice();
}

void PulseOut::CloseDevice() {
    Lock lock(this->stateMutex);
    if (this->audioConnection) {
        std::cerr << "PulseOut: closing device\n";

        int error = 0;
        pa_blocking_flush(this->audioConnection, &error);
        pa_blocking_free(this->audioConnection);
        this->audioConnection = nullptr;
        this->rate = 0;
        this->channels = 0;
    }
}

void PulseOut::OpenDevice(musik::core::sdk::IBuffer* buffer) {
    if (!this->audioConnection ||
        this->rate != buffer->SampleRate() ||
        this->channels != buffer->Channels())
    {
        this->CloseDevice();

        pa_sample_spec spec;
        spec.format = PA_SAMPLE_FLOAT32LE;
        spec.channels = buffer->Channels();
        spec.rate = buffer->SampleRate();

        std::cerr << "PulseOut: opening device\n";
        this->audioConnection = pa_blocking_new(
            nullptr,
            "musikbox",
            PA_STREAM_PLAYBACK,
            nullptr,
            "music",
            &spec,
            nullptr,
            nullptr,
            0);

        if (this->audioConnection) {
            this->SetVolume(this->volume);
            this->rate = buffer->SampleRate();
            this->channels = buffer->Channels();
            this->state = StatePlaying;
        }
    }
}

void PulseOut::Destroy() {
    std::cerr << "PulseOut: destroy\n";
    delete this;
}

void PulseOut::Stop() {
    Lock lock(this->stateMutex);
    if (this->audioConnection) {
        pa_blocking_flush(this->audioConnection, 0);
        this->state = StateStopped;
    }
}

void PulseOut::Pause() {
    Lock lock(this->stateMutex);
    if (this->audioConnection) {
        pa_blocking_flush(this->audioConnection, 0);
        this->state = StatePaused;
    }
}

void PulseOut::Resume() {
    Lock lock(this->stateMutex);
    if (this->audioConnection) {
        this->state = StatePlaying;
    }
}

void PulseOut::SetVolume(double volume) {
    Lock lock(this->stateMutex);
    this->volume = volume;
    if (this->audioConnection) {
        int normalized = (int) round((double) PA_VOLUME_NORM * volume);
        pa_blocking_set_volume(this->audioConnection, normalized, 0);
    }
}

bool PulseOut::Play(IBuffer *buffer, IBufferProvider* provider) {
    int error = 0;

    {
        Lock lock(this->stateMutex);

        this->OpenDevice(buffer);

        if (!this->audioConnection || this->state != StatePlaying) {
            return false;
        }

        pa_blocking_write(
            this->audioConnection,
            buffer->BufferPointer(),
            buffer->Samples() * sizeof(float),
            &error);
    }

    provider->OnBufferProcessed(buffer);
    return true;
}

double PulseOut::Latency() {
    Lock lock(this->stateMutex);
    
    if (!this->audioConnection) {
        return 0.0;
    }

    int error = 0;
    unsigned long long latency = pa_blocking_get_latency(this->audioConnection, &error);
    return (double) latency / 1000000.0; /* microseconds to seconds */
}
