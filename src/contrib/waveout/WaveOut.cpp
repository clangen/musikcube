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

#define MAX_VOLUME 0xFFFF

WaveOut::WaveOut()
: waveHandle(NULL)
, maxBuffers(32)
, currentVolume(1.0)
{
}

WaveOut::~WaveOut(){
    this->Stop();

    if (this->waveHandle != NULL) {
        waveOutClose(this->waveHandle);
        this->waveHandle = NULL;
    }
}

void WaveOut::Destroy() {
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
        DWORD newVolume = (DWORD) (volume * MAX_VOLUME);
        DWORD leftAndRight = (newVolume << 16) | newVolume;
        waveOutSetVolume(this->waveHandle, leftAndRight); 
    }

    this->currentVolume = volume;
}

void WaveOut::Stop() {
    {
        boost::mutex::scoped_lock lock(this->mutex);

        BufferList::iterator it = this->queuedBuffers.begin();
        for ( ; it != this->queuedBuffers.end(); ++it) {
            (*it)->Destroy();
        }
    }

    waveOutReset(this->waveHandle);
}

void WaveOut::OnBufferWrittenToOutput(WaveOutBuffer *buffer) {
    boost::mutex::scoped_lock lock(this->mutex);

    /* let the player know the output device is done with the buffer; the
    Player ensures buffers are locked down and not freed/reused until it
    gets confirmation it's been played (or the user stops playback) */
    IPlayer* player = buffer->GetPlayer();
    player->OnBufferProcessedByOutput(buffer->GetWrappedBuffer());
    
    /* remove it from our internal list. seems we should be using an 
    std::set here instead of a vector. */
    BufferList::iterator it = this->queuedBuffers.begin();

    for( ; it != this->queuedBuffers.end(); ++it) {
        if (it->get() == buffer) {
            it = this->queuedBuffers.erase(it);
            return;
        }
    }
}

bool WaveOut::Play(IBuffer *buffer, IPlayer *player) {
    boost::mutex::scoped_lock lock(this->mutex);

    size_t bufferCount = this->queuedBuffers.size();

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
            this->queuedBuffers.push_back(waveBuffer);
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
        if (this->waveHandle != NULL) {
            waveOutClose(this->waveHandle);
            this->waveHandle = NULL;
        }

        /* reset, and configure speaker output */
	    ZeroMemory(&this->waveFormat, sizeof(this->waveFormat));

        DWORD speakerConfig = 0;

        switch (buffer->Channels()) {
            case 1:
                speakerConfig = KSAUDIO_SPEAKER_MONO;
                break;
            case 2:
                speakerConfig = KSAUDIO_SPEAKER_STEREO;
                break;
            case 4:
                speakerConfig = KSAUDIO_SPEAKER_QUAD;
                break;
            case 5:
                speakerConfig = (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT);
                break;
            case 6:
                speakerConfig = KSAUDIO_SPEAKER_5POINT1;
                break;
        }

        this->waveFormat.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        this->waveFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        this->waveFormat.Format.nChannels = (WORD) buffer->Channels();
        this->waveFormat.Format.wBitsPerSample = sizeof(float) * 8;
        this->waveFormat.Format.nSamplesPerSec = (DWORD) buffer->SampleRate();

        int bytesPerSample = this->waveFormat.Format.wBitsPerSample / 8;
        this->waveFormat.Format.nBlockAlign = bytesPerSample * this->waveFormat.Format.nChannels;

        this->waveFormat.Format.nAvgBytesPerSec =
            this->waveFormat.Format.nBlockAlign * this->waveFormat.Format.nSamplesPerSec;

        /* NOTE: wValidBitsPerSample/wReserved/wSamplesPerBlock are a union */
        this->waveFormat.Samples.wValidBitsPerSample = this->waveFormat.Format.wBitsPerSample;
        this->waveFormat.dwChannelMask = speakerConfig;
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
        WaveOutBuffer* buffer = (WaveOutBuffer*) waveoutHeader->dwUser;
        ((WaveOut*) dwUser)->OnBufferWrittenToOutput(buffer);
    }
}
