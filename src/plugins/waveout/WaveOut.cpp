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

#include "pch.h"
#include "WaveOut.h"
#include <string>
#include <vector>
#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>

#define MAX_VOLUME 0xFFFF
#define MAX_BUFFERS_PER_OUTPUT 16
#define PREF_DEVICE_ID "device_id"

musik::core::sdk::IPreferences* prefs = nullptr;

using LockT = std::unique_lock<std::recursive_mutex>;

class WaveOutDevice : public musik::core::sdk::IDevice {
    public:
        WaveOutDevice(const std::string& id, const std::string& name) {
            this->id = id;
            this->name = name;
        }

        virtual void Release() override { delete this; }
        virtual const char* Name() const override { return name.c_str(); }
        virtual const char* Id() const override { return id.c_str(); }

    private:
        std::string name, id;
};

class WaveOutDeviceList : public musik::core::sdk::IDeviceList {
    public:
        virtual void Release() override { delete this; }
        virtual size_t Count() const override { return devices.size(); }
        virtual const IDevice* At(size_t index) const override { return &devices.at(index); }

        void Add(const std::string& id, const std::string& name) {
            devices.push_back(WaveOutDevice(id, name));
        }

    private:
        std::vector<WaveOutDevice> devices;
};

extern "C" __declspec(dllexport) void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    ::prefs = prefs;
    prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    prefs->Save();
}

static inline std::string utf16to8(const wchar_t* utf16) {
    if (!utf16) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, 0, 0, 0, 0);
    if (size <= 0) return "";
    char* buffer = new char[size];
    WideCharToMultiByte(CP_UTF8, 0, utf16, -1, buffer, size, 0, 0);
    std::string utf8str(buffer);
    delete[] buffer;
    return utf8str;
}

static std::string deviceCapsToId(WAVEOUTCAPS& caps, UINT index) {
    std::string name = utf16to8(caps.szPname);
    return std::to_string(index) + ":" + name;
}

static std::string getDeviceId() {
    char buffer[4096] = { 0 };
    std::string storedDeviceId;
    if (prefs && prefs->GetString(PREF_DEVICE_ID, buffer, 4096, "") > 0) {
        storedDeviceId.assign(buffer);
    }
    return storedDeviceId;
}

WaveOut::WaveOut()
: waveHandle(NULL)
, currentVolume(1.0)
, threadId(0)
, threadHandle(NULL)
, playing(false)
{
}

WaveOut::~WaveOut() {
}

void WaveOut::Release() {
    {
        LockT lock(this->outputDeviceMutex);

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
    }

    this->ClearBufferQueue();

    delete this;
}


void WaveOut::NotifyBufferProcessed(WaveOut::WaveOutBufferPtr buffer) {
    /* let the provider know the output device is done with the buffer; the
    Player ensures buffers are locked down and not freed/reused until it
    gets confirmation it's been played (or the user stops playback) */
    IBufferProvider* provider = buffer->GetBufferProvider();
    provider->OnBufferProcessed(buffer->GetWrappedBuffer());

    LockT lock(this->bufferQueueMutex);
    this->freeBuffers.push_back(buffer);
}

void WaveOut::Pause() {
    LockT lock(this->outputDeviceMutex);

    waveOutPause(this->waveHandle);
    this->playing = false;
}

void WaveOut::Resume() {
    LockT lock(this->outputDeviceMutex);

    if (!this->playing) {
        waveOutRestart(this->waveHandle);
    }

    this->playing = true;
}

void WaveOut::SetVolume(double volume) {
    if (this->waveHandle) {
        DWORD newVolume = (DWORD) (volume * MAX_VOLUME);
        DWORD leftAndRight = (newVolume << 16) | newVolume;
        waveOutSetVolume(this->waveHandle, leftAndRight);
    }

    this->currentVolume = volume;
}

double WaveOut::GetVolume() {
    return this->currentVolume;
}

void WaveOut::Stop() {
    LockT lock(this->outputDeviceMutex);

    if (this->waveHandle != NULL) {
        waveOutReset(this->waveHandle);
    }
}

void WaveOut::ClearBufferQueue() {
    BufferList remove;

    {
        LockT lock(this->bufferQueueMutex);
        std::swap(this->queuedBuffers, remove);
    }

    /* notify and free any pending buffers, the Player in the core
    will be waiting for all pending buffers to be processed. */
    if (remove.size() > 0) {
        BufferList::iterator it = remove.begin();
        for (; it != remove.end(); it++) {
            this->NotifyBufferProcessed((*it));
        }
    }
}

void WaveOut::OnBufferWrittenToOutput(WaveOutBuffer *buffer) {
    WaveOutBufferPtr erased;

    {
        LockT lock(this->bufferQueueMutex);

        /* removed the buffer. it should be at the front of the queue. */
        BufferList::iterator it = this->queuedBuffers.begin();
        for (; it != this->queuedBuffers.end(); it++) {
            if (it->get() == buffer) {
                erased = *it;
                this->queuedBuffers.erase(it);
                break;
            }
        }
    }

    if (erased) {
        this->NotifyBufferProcessed(erased);
    }
}

void WaveOut::StartWaveOutThread() {
    this->StopWaveOutThread();

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

WaveOut::WaveOutBufferPtr WaveOut::GetEmptyBuffer() {
    LockT lock(this->bufferQueueMutex);

    if (this->freeBuffers.size()) {
        WaveOut::WaveOutBufferPtr buffer = this->freeBuffers.front();
        this->freeBuffers.pop_front();
        return buffer;
    }
    else {
        return WaveOut::WaveOutBufferPtr(new WaveOutBuffer(this));
    }
}

int WaveOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    LockT lock(this->bufferQueueMutex);

    /* if we have a different format, return false and wait for the pending
    buffers to be written to the output device. */
    if (!this->queuedBuffers.empty()) {
        bool formatChanged =
            this->currentChannels != buffer->Channels() ||
            this->currentSampleRate != buffer->SampleRate();

        if (formatChanged) {
            return OutputFormatError;
        }
    }

    size_t buffersForOutput = 0;
    auto it = this->queuedBuffers.begin();
    while (it != this->queuedBuffers.end()) {
        if ((*it)->GetBufferProvider() == provider) {
            ++buffersForOutput;
        }
        ++it;
    }

    if (MAX_BUFFERS_PER_OUTPUT > buffersForOutput) {
        {
            LockT lock2(this->outputDeviceMutex);

            if (!this->playing) {
                return OutputInvalidState;
            }
        }

        /* ensure the output device itself (the WAVEOUT) is configured correctly
        for the new buffer */
        this->SetFormat(buffer);

        /* add the raw buffer to a WaveOutBuffer; this will ensure a correct WAVEHDR
        is configured for the WAVEOUT device */
        WaveOutBufferPtr waveBuffer = this->GetEmptyBuffer();
        waveBuffer->Set(buffer, provider);

        if (waveBuffer->WriteToOutput()) {
            this->queuedBuffers.push_back(waveBuffer);
            return OutputBufferWritten;
        }
    }

    return OutputBufferFull;
}

bool WaveOut::SetDefaultDevice(const char* deviceId) {
    return setDefaultDevice<IPreferences, WaveOutDevice, IOutput>(prefs, this, PREF_DEVICE_ID, deviceId);
}

IDevice* WaveOut::GetDefaultDevice() {
    return findDeviceById<WaveOutDevice, IOutput>(this, getDeviceId());
}

IDeviceList* WaveOut::GetDeviceList() {
    WaveOutDeviceList* result = new WaveOutDeviceList();

    for (UINT i = 0; i < waveOutGetNumDevs(); i++) {
        WAVEOUTCAPS caps = { 0 };
        if (waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS)) == MMSYSERR_NOERROR) {
            std::string name = utf16to8(caps.szPname);
            std::string id = deviceCapsToId(caps, i);
            result->Add(id, name);
        }
    }

    return result;
}

UINT WaveOut::GetPreferredDeviceId() {
    std::string storedDeviceId = getDeviceId();

    for (UINT i = 0; i < waveOutGetNumDevs(); i++) {
        WAVEOUTCAPS caps = { 0 };
        if (waveOutGetDevCaps(i, &caps, sizeof(WAVEOUTCAPS)) == MMSYSERR_NOERROR) {
            if (storedDeviceId == deviceCapsToId(caps, i)) {
                return i;
            }
        }
    }

    return WAVE_MAPPER;
}

void WaveOut::SetFormat(IBuffer *buffer) {
    if (this->currentChannels != buffer->Channels() ||
        this->currentSampleRate != buffer->SampleRate() ||
        this->waveHandle == NULL)
    {
        LockT lock(this->outputDeviceMutex);

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
            this->GetPreferredDeviceId(),
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
