//////////////////////////////////////////////////////////////////////////////
// Copyright  2007, Daniel nnerby
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

#include "WaveOut.h"

#define MAX_VOLUME 65535.0

WaveOut::WaveOut()
: waveHandle(NULL)
, maxBuffers(32)
, currentVolume(1.0)
{
}

WaveOut::~WaveOut(){
    this->ClearBuffers();

    if (this->waveHandle != NULL) {
        waveOutClose(this->waveHandle);
        this->waveHandle = NULL;
    }
}

void WaveOut::Destroy(){
    delete this;
}

void WaveOut::Pause() {
    waveOutPause(this->waveHandle);
}

void WaveOut::Resume() {
    waveOutRestart(this->waveHandle);
}

void WaveOut::SetVolume(double volume) {
    if (this->waveHandle) {
        DWORD newVolume = (DWORD)(volume * MAX_VOLUME);
        waveOutSetVolume(this->waveHandle,newVolume); 
    }

    this->currentVolume = volume;
}

void WaveOut::ClearBuffers() {
    {
        boost::mutex::scoped_lock lock(this->mutex);

        BufferList::iterator it = this->buffers.begin();
        for (; it != this->buffers.end(); ++it) {
            (*it)->Destroy();
        }
    }

    waveOutReset(this->waveHandle);
}

void WaveOut::OnBufferWrittenToOutput(WaveOutBuffer *buffer) {
    boost::mutex::scoped_lock lock(this->mutex);

    bool found = false;
    BufferList::iterator it = this->buffers.begin();

    for( ; it != this->buffers.end() && !found ; ++it) {
        if (it->get() == buffer) {
            it = this->buffers.erase(it);
            this->bufferRemovedCondition.notify_all();
            return;
        }
    }
}

void WaveOut::ReleaseBuffers() {
    boost::mutex::scoped_lock lock(this->mutex);
}

bool WaveOut::PlayBuffer(IBuffer *buffer, IPlayer *player) {
    boost::mutex::scoped_lock lock(this->mutex);

    size_t bufferCount = this->buffers.size();

    /* if we have a different format, return false and wait for the pending 
    buffers to be written to the output device. */
    if (bufferCount > 0) {
        bool formatChanged =  
            this->currentChannels != buffer->Channels() || 
            this->currentSampleRate != buffer->SampleRate();

        if (formatChanged) {
            return false;
        }
    }

    if (this->maxBuffers > bufferCount) {
        /* ensure the output device itself (the WAVEOUT) is configured correctly 
        for the new buffer */
        this->SetFormat(buffer);

        /* add the raw buffer to a WaveOutBuffer; this will ensure a correct WAVEHDR 
        is configured for the WAVEOUT device */
        WaveOutBufferPtr waveBuffer(new WaveOutBuffer(this, buffer, player));

        if (waveBuffer->WriteToOutput()) {
            this->buffers.push_back(waveBuffer);
            return true;
        }
    }

    return false;
}

void WaveOut::SetFormat(IBuffer *buffer) {
    if (this->currentChannels != buffer->Channels() || 
        this->currentSampleRate != buffer->SampleRate() ||
        this->waveHandle == NULL)
    {
        this->currentChannels = buffer->Channels();
        this->currentSampleRate = buffer->SampleRate();

        /* format changed, kill the old output device */
        if(this->waveHandle!=NULL) {
            waveOutClose(this->waveHandle);
            this->waveHandle = NULL;
        }

        /* reset, and configure speaker output */
	    ZeroMemory(&this->waveFormat, sizeof(this->waveFormat));

        DWORD speakerconfig = 0;

        switch (buffer->Channels()) {
            case 1:
                speakerconfig = KSAUDIO_SPEAKER_MONO;
                break;
            case 2:
                speakerconfig = KSAUDIO_SPEAKER_STEREO;
                break;
            case 4:
                speakerconfig = KSAUDIO_SPEAKER_QUAD;
                break;
            case 5:
                speakerconfig = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT  | SPEAKER_BACK_RIGHT);
                break;
            case 6:
                speakerconfig = KSAUDIO_SPEAKER_5POINT1;
                break;
        }

        this->waveFormat.Format.cbSize = 22;
        this->waveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        this->waveFormat.Format.nChannels = (WORD) buffer->Channels();
        this->waveFormat.Format.nSamplesPerSec = (DWORD) buffer->SampleRate();
        this->waveFormat.Format.wBitsPerSample = 32;

        this->waveFormat.Format.nBlockAlign = 
            (this->waveFormat.Format.wBitsPerSample / 8) * this->waveFormat.Format.nChannels;

        this->waveFormat.Format.nAvgBytesPerSec =
            ((this->waveFormat.Format.wBitsPerSample/8) *
            this->waveFormat.Format.nChannels) * 
            this->waveFormat.Format.nSamplesPerSec; /* Compute using nBlkAlign * nSamp/Sec */

        /* IMPORTANT NOTE: wValidBitsPerSample/wReserved/wSamplesPerBlock are a union,
        so don't set wReserved or wSamplesPerBlock to 0 after assigning wValidBitsPerSample. */
        this->waveFormat.Samples.wValidBitsPerSample = 32;
        this->waveFormat.dwChannelMask = speakerconfig; 
        this->waveFormat.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

        /* create the output device */
        int openResult = waveOutOpen(
            &this->waveHandle, 
            WAVE_MAPPER, 
            (WAVEFORMATEX*) &this->waveFormat, 
            (DWORD_PTR) WaveCallback, 
            (DWORD_PTR) this, 
            CALLBACK_FUNCTION);

        if (openResult != MMSYSERR_NOERROR) {
            throw;
        }

        this->SetVolume(this->currentVolume);
    }
}

void CALLBACK WaveOut::WaveCallback(
    HWAVEOUT waveHandle, UINT msg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD dw2)
{
    if (msg == WOM_DONE) {
		LPWAVEHDR waveoutHeader = (LPWAVEHDR) dw1;
        ((WaveOutBuffer*) waveoutHeader->dwUser)->OnWriteFinished();
    }
}
