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
#define MAX_BUFFERS 8

static void notifyBufferProcessed(WaveOutBuffer *buffer) {
    /* let the provider know the output device is done with the buffer; the
    Player ensures buffers are locked down and not freed/reused until it
    gets confirmation it's been played (or the user stops playback) */
    IBufferProvider* provider = buffer->GetBufferProvider();
    provider->OnBufferProcessed(buffer->GetWrappedBuffer());
}

WaveOut::WaveOut()
: waveHandle(NULL)
, currentVolume(1.0)
, threadId(0)
, threadHandle(NULL)
{
}

WaveOut::~WaveOut() {
}

void WaveOut::Destroy() {
    boost::recursive_mutex::scoped_lock lock1(this->bufferQueueMutex);
    boost::recursive_mutex::scoped_lock lock2(this->outputDeviceMutex);

    /* reset playback immediately. this will invalidate all pending
    buffers */
    if (this->waveHandle != NULL) {
        waveOutReset(this->waveHandle);
    }

    /* stop the thread so nothing else is processed */
    this->StopWaveOutThread();

    /* close it down after the threadproc has finished */
    if (this->waveHandle != NULL) {
        waveOutClose(this->waveHandle);
        this->waveHandle = NULL;
    }

    this->ClearBufferQueue();

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
    boost::recursive_mutex::scoped_lock lock1(this->bufferQueueMutex);
    boost::recursive_mutex::scoped_lock lock2(this->outputDeviceMutex);

    if (this->waveHandle != NULL) {
        waveOutReset(this->waveHandle);
    }

    this->ClearBufferQueue();
}

void WaveOut::ClearBufferQueue() {
    boost::recursive_mutex::scoped_lock lock(this->bufferQueueMutex);

    /* notify and free any pending buffers, the Player in the core
    will be waiting for all pending buffers to be processed. */
    if (this->queuedBuffers.size() > 0) {
        BufferList::iterator it = this->queuedBuffers.begin();
        for (; it != this->queuedBuffers.end(); it++) {
            notifyBufferProcessed((*it).get());
        }

        this->queuedBuffers.clear();
    }
}

void WaveOut::OnBufferWrittenToOutput(WaveOutBuffer *buffer) {
    boost::recursive_mutex::scoped_lock lock(this->bufferQueueMutex);
   
    /* removed the buffer. it should be at the front of the queue. */
    BufferList::iterator it = this->queuedBuffers.begin();
    for( ; it != this->queuedBuffers.end(); it++) {
        if (it->get() == buffer) {
            notifyBufferProcessed(buffer);
            this->queuedBuffers.erase(it);
            return;
        }
    }
}

void WaveOut::StartWaveOutThread() {
    this->threadHandle = CreateThread(
        NULL,
        0,
        &WaveOut::WaveCallbackThreadProc,
        this,
        NULL,
        &this->threadId);
}

void WaveOut::StopWaveOutThread() {
    if (this->threadHandle != NULL) {
        PostThreadMessage(this->threadId, WM_QUIT, 0, 0);
        WaitForSingleObject(this->threadHandle, INFINITE);
        this->threadHandle = NULL;
        this->threadId = 0;
    }
}

bool WaveOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    boost::recursive_mutex::scoped_lock lock(this->bufferQueueMutex);

    size_t bufferCount = bufferCount = this->queuedBuffers.size();

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

    if (MAX_BUFFERS > bufferCount) {
        /* ensure the output device itself (the WAVEOUT) is configured correctly 
        for the new buffer */
        this->SetFormat(buffer);

        /* add the raw buffer to a WaveOutBuffer; this will ensure a correct WAVEHDR 
        is configured for the WAVEOUT device */
        WaveOutBufferPtr waveBuffer(new WaveOutBuffer(this, buffer, provider));

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
        boost::recursive_mutex::scoped_lock lock(this->outputDeviceMutex);

        this->currentChannels = buffer->Channels();
        this->currentSampleRate = buffer->SampleRate();

        this->Stop();
        this->StartWaveOutThread();

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

        /* create the output device. note that we use a thread instead of a simple callback
        here. that's because processing a buffer after calling waveOutReset() can lead to
        crashes; so we can use a thread and ensure it's shut down before resetting the
        output device, making it impossible to reach this condition. */
        int openResult = waveOutOpen(
            &this->waveHandle, 
            WAVE_MAPPER, 
            (WAVEFORMATEX*) &this->waveFormat, 
            this->threadId, 
            (DWORD_PTR) this, 
            CALLBACK_THREAD);

        if (openResult != MMSYSERR_NOERROR) {
            throw;
        }

        this->SetVolume(this->currentVolume);
    }
}

DWORD WINAPI WaveOut::WaveCallbackThreadProc(LPVOID params) {
    WaveOut* waveOut = (WaveOut*) params;

    MSG msg;

    /* create message queue implicitly. */
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    bool stop = false;
    while (!stop && GetMessage(&msg, NULL, 0, 0)) {
        switch (msg.message) {
            case WOM_DONE: {
                LPWAVEHDR waveoutHeader = (LPWAVEHDR) msg.lParam;
                WaveOutBuffer* buffer = (WaveOutBuffer*) waveoutHeader->dwUser;
                waveOut->OnBufferWrittenToOutput(buffer);
                break;
            }

            case WM_QUIT: {
                stop = true;
                break;
            }
        }
    }

    return 0;
}
