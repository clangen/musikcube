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

#include "WaveOutBuffer.h"
#include "WaveOut.h"

#include <iostream>

WaveOutBuffer::WaveOutBuffer(WaveOut *waveOut, IBuffer *buffer, IBufferProvider *provider)
: waveOut(waveOut) 
, buffer(buffer)
, provider(provider)
, destroyed(false)
{
    this->Initialize();
}

void WaveOutBuffer::Initialize() {
    this->header.dwBufferLength = this->buffer->Samples() * this->buffer->Channels() * sizeof(float);
    this->header.lpData = (LPSTR) this->buffer->BufferPointer();
	this->header.dwUser = (DWORD_PTR) this;
	this->header.dwBytesRecorded = 0;
	this->header.dwFlags = 0;
	this->header.dwLoops = 0;
    this->header.lpNext = NULL;
    this->header.reserved = NULL;

    MMRESULT result = waveOutPrepareHeader(this->waveOut->waveHandle, &this->header, sizeof(WAVEHDR));
    if (result != MMSYSERR_NOERROR) {
        throw;
    }
}

void WaveOutBuffer::Destroy() {
    if (!this->destroyed) {
        if (this->waveOut->waveHandle && this->header.dwFlags & WHDR_PREPARED) {
            waveOutUnprepareHeader(this->waveOut->waveHandle, &this->header, sizeof(WAVEHDR));
            this->header.dwFlags = WHDR_DONE;
        }

        this->destroyed = true;
    }
}

IBufferProvider* WaveOutBuffer::GetBufferProvider() const {
    return this->provider;
}

IBuffer* WaveOutBuffer::GetWrappedBuffer() const {
    return this->buffer;
}

WaveOutBuffer::~WaveOutBuffer() {
    this->Destroy();
}

bool WaveOutBuffer::WriteToOutput() {
    MMRESULT result = waveOutWrite(this->waveOut->waveHandle, &this->header, sizeof(WAVEHDR));
    
    if (result == MMSYSERR_NOERROR) {
        return true;
    }

    return false;
}

