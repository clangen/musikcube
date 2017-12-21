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

#include "SndioOut.h"

#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>

using namespace musik::core::sdk;

SndioOut::SndioOut() {
    this->volume = 1.0f;
    this->state = StateStopped;
}

SndioOut::~SndioOut() {
}

void SndioOut::Release() {
    delete this;
}

void SndioOut::Pause() {
    this->state = StatePaused;
}

void SndioOut::Resume() {
    this->state = StatePlaying;
}

void SndioOut::SetVolume(double volume) {
    this->volume = volume;
}

double SndioOut::GetVolume() {
    return this->volume;
}

void SndioOut::Stop() {
    this->state = StateStopped;
}

void SndioOut::Drain() {

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

int SndioOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    if (this->state == StatePaused) {
        return OutputInvalidState;
    }

    return OutputBufferWritten;
}

double SndioOut::Latency() {
    return 0.0;
}
