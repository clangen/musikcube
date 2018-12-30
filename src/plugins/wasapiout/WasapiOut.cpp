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

#include "WasapiOut.h"
#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>
#include <core/sdk/ISchema.h>
#include <AudioSessionTypes.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <vector>

#define PREF_DEVICE_ID "device_id"
#define PREF_ENDPOINT_ROUTING "enable_audio_endpoint_routing"
#define PREF_BUFFER_LENGTH_SECONDS "buffer_length_seconds"

/* NOTE! device init and deinit logic was stolen and modified from
QMMP's WASAPI output plugin! http://qmmp.ylsoftware.com/ */

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

using Lock = std::unique_lock<std::recursive_mutex>;
musik::core::sdk::IPreferences* prefs = nullptr;

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

static std::string getDeviceId() {
    return getPreferenceString<std::string>(prefs, PREF_DEVICE_ID, "");;
}

class WasapiDevice : public musik::core::sdk::IDevice {
    public:
        WasapiDevice(const std::string& id, const std::string& name) {
            this->id = id;
            this->name = name;
        }

        virtual void Release() override { delete this; }
        virtual const char* Name() const override { return name.c_str(); }
        virtual const char* Id() const override { return id.c_str(); }

    private:
        std::string name, id;
};

class WasapiDeviceList : public musik::core::sdk::IDeviceList {
    public:
        virtual void Release() override { delete this; }
        virtual size_t Count() const override { return devices.size(); }
        virtual const IDevice* At(size_t index) const override { return &devices.at(index); }

        void Add(const std::string& id, const std::string& name) {
            devices.push_back(WasapiDevice(id, name));
        }

    private:
        std::vector<WasapiDevice> devices;
};

extern "C" __declspec(dllexport) void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    ::prefs = prefs;
    prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    prefs->GetBool(PREF_ENDPOINT_ROUTING, false);
    prefs->Save();
}

extern "C" __declspec(dllexport) musik::core::sdk::ISchema* GetSchema() {
    auto schema = new TSchema<>();
    schema->AddBool(PREF_ENDPOINT_ROUTING, false);
    schema->AddDouble(PREF_BUFFER_LENGTH_SECONDS, 1.0, 2, 0.25, 5.0);
    return schema;
}

static bool audioRoutingEnabled() {
    return ::prefs && prefs->GetBool(PREF_ENDPOINT_ROUTING, false);
}

class NotificationClient : public IMMNotificationClient {
    public:
        NotificationClient(WasapiOut* owner)
        : count(1)
        , owner(owner)
        , enumerator(nullptr) {
        }

        ~NotificationClient() {
            if (this->enumerator) {
                this->enumerator->Release();
                this->enumerator = nullptr;
            }
        }

        /* IUnknown methods -- AddRef, Release, and QueryInterface */

        ULONG STDMETHODCALLTYPE AddRef() {
            return InterlockedIncrement(&this->count);
        }

        ULONG STDMETHODCALLTYPE Release() {
            ULONG newCount = InterlockedDecrement(&this->count);
            if (0 == newCount) {
                delete this;
            }
            return newCount;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface) {
            if (IID_IUnknown == riid) {
                this->AddRef();
                *ppvInterface = (IUnknown*)this;
            }
            else if (__uuidof(IMMNotificationClient) == riid) {
                this->AddRef();
                *ppvInterface = (IMMNotificationClient*)this;
            }
            else {
                *ppvInterface = nullptr;
                return E_NOINTERFACE;
            }
            return S_OK;
        }

        /* Callback methods for device-event notifications. */

        HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(
            EDataFlow flow, ERole role,
            LPCWSTR pwstrDeviceId)
        {
            if (audioRoutingEnabled()) {
                if (flow == eRender && role == eMultimedia) {
                    if (this->lastDeviceId != std::wstring(pwstrDeviceId)) {
                        owner->OnDeviceChanged();
                        this->lastDeviceId = std::wstring(pwstrDeviceId);
                    }
                }
            }

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR pwstrDeviceId) {
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR pwstrDeviceId) {
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState) {
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR pwstrDeviceId, const PROPERTYKEY key){
            return S_OK;
        }

    private:
        LONG count;
        IMMDeviceEnumerator *enumerator;
        WasapiOut* owner;
        std::wstring lastDeviceId;
};

WasapiOut::WasapiOut()
: enumerator(nullptr)
, device(nullptr)
, audioClient(nullptr)
, renderClient(nullptr)
, simpleAudioVolume(nullptr)
, audioStreamVolume(nullptr)
, audioClock(nullptr)
, notificationClient(nullptr)
, outputBufferFrames(0)
, state(StateStopped)
, latency(0)
, deviceChanged(false)
, volume(1.0f) {
    ZeroMemory(&waveFormat, sizeof(WAVEFORMATEXTENSIBLE));
}

WasapiOut::~WasapiOut() {
}

void WasapiOut::Release() {
    this->Reset();

    if (this->enumerator) {
        if (this->notificationClient) {
            this->enumerator->UnregisterEndpointNotificationCallback(this->notificationClient);
            this->notificationClient->Release();
            this->notificationClient = nullptr;
        }

        this->enumerator->Release();
        this->enumerator = nullptr;
    }

    delete this;
}

void WasapiOut::Pause() {
    this->state = StatePaused;

    Lock lock(this->stateMutex);

    if (this->audioClient) {
        this->audioClient->Stop();
    }
}

void WasapiOut::Resume() {
    this->state = StatePlaying;

    Lock lock(this->stateMutex);

    if (this->audioClient) {
        this->audioClient->Start();
    }
}

void WasapiOut::SetVolume(double volume) {
    Lock lock(this->stateMutex);

    this->volume = volume;
    if (this->simpleAudioVolume) {
        /* master volume is always 1.0 for mixing purposes */
        simpleAudioVolume->SetMasterVolume(1.0f, 0);
        simpleAudioVolume->SetMute(false, 0);

        /* volume for this particular output's stream */
        if (this->audioStreamVolume) {
            UINT32 count = 0;
            this->audioStreamVolume->GetChannelCount(&count);

            for (UINT32 i = 0; i < count; i++) {
                this->audioStreamVolume
                    ->SetChannelVolume(i, (float) this->volume);
            }
        }
    }
}

double WasapiOut::GetVolume() {
    return this->volume;
}

void WasapiOut::Stop() {
    this->state = StateStopped;

    Lock lock(this->stateMutex);
    this->Reset();
}

void WasapiOut::Drain() {
    int sleepMs = (int) (round(this->Latency()) * 1000.0f);

    /* not sure of a better way to ensure the final buffer is
    flushed other than to use this heuristic: given the latency
    size in seconds, sleep for 50 milliseconds at a time while
    it's still playing. */
    while (this->state != StateStopped && sleepMs > 0) {
        Sleep(50);
        if (this->state == StatePlaying) {
            sleepMs -= 50;
        }
    }
}

int WasapiOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    Lock lock(this->stateMutex);

    if (this->state == StatePaused) {
        return OutputInvalidState;
    }

    if (this->deviceChanged) {
        this->Reset();
        this->deviceChanged = false;
        return OutputFormatError;
    }

    if (!this->Configure(buffer)) {
        this->Reset();
        return OutputFormatError;
    }

    HRESULT result;
    UINT32 availableFrames = 0;
    UINT32 frameOffset = 0;
    UINT32 samples = (UINT32)buffer->Samples();
    UINT32 framesToWrite = samples / (UINT32)buffer->Channels();
    int channels = buffer->Channels();

    this->audioClient->GetCurrentPadding(&frameOffset);
    availableFrames = (this->outputBufferFrames - frameOffset);

    if (availableFrames < framesToWrite) {
        UINT32 delta = framesToWrite - availableFrames;
        return (delta * 1000) / buffer->SampleRate();
    }

    if (availableFrames >= framesToWrite) {
        BYTE *data = nullptr;
        result = this->renderClient->GetBuffer(framesToWrite, &data);
        if (result == S_OK) {
            memcpy(data, buffer->BufferPointer(), sizeof(float) * samples);
            this->renderClient->ReleaseBuffer(framesToWrite, 0);
            provider->OnBufferProcessed(buffer);
            return OutputBufferWritten;
        }
    }

    return OutputBufferFull;
}

void WasapiOut::Reset() {
    if (this->simpleAudioVolume) {
        this->simpleAudioVolume->Release();
    }

    if (this->audioStreamVolume) {
        this->audioStreamVolume->Release();
    }

    if (this->audioClock) {
        this->audioClock->Release();
    }

    if (this->renderClient) {
        this->renderClient->Release();
    }

    if (this->audioClient) {
        this->audioClient->Reset();
        this->audioClient->Stop();
        this->audioClient->Release();
    }

    if (this->device) {
        this->device->Release();
    }

    this->simpleAudioVolume = nullptr;
    this->audioStreamVolume = nullptr;
    this->audioClock = nullptr;
    this->renderClient = nullptr;
    this->audioClient = nullptr;
    this->device = nullptr;

    ZeroMemory(&waveFormat, sizeof(WAVEFORMATEXTENSIBLE));
}

double WasapiOut::Latency() {
    return this->latency;
}

bool WasapiOut::SetDefaultDevice(const char* deviceId) {
    return setDefaultDevice<IPreferences, WasapiDevice, IOutput>(prefs, this, PREF_DEVICE_ID, deviceId);
}

IDevice* WasapiOut::GetDefaultDevice() {
    return findDeviceById<WasapiDevice, IOutput>(this, getDeviceId());
}

IDeviceList* WasapiOut::GetDeviceList() {
    WasapiDeviceList* result = new WasapiDeviceList();
    IMMDeviceEnumerator *deviceEnumerator = nullptr;
    IMMDeviceCollection *deviceCollection = nullptr;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        NULL,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        (void**) &deviceEnumerator);

    if (hr == S_OK) {
        hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
        if (hr == S_OK) {
            UINT deviceCount = 0;
            if (deviceCollection->GetCount(&deviceCount) == S_OK) {
                for (UINT i = 0; i < deviceCount; i++) {
                    IMMDevice* device = nullptr;
                    LPWSTR deviceIdPtr;
                    std::string deviceId, deviceName;

                    hr = deviceCollection->Item(i, &device);
                    if (hr == S_OK) {
                        if (device->GetId(&deviceIdPtr) == S_OK) {
                            deviceId = utf16to8(deviceIdPtr);
                            CoTaskMemFree(deviceIdPtr);
                        }

                        IPropertyStore *propertyStore;
                        if (device->OpenPropertyStore(STGM_READ, &propertyStore) == S_OK) {
                            PROPVARIANT friendlyName;
                            PropVariantInit(&friendlyName);

                            if (propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName) == S_OK) {
                                deviceName = utf16to8(friendlyName.pwszVal);
                                PropVariantClear(&friendlyName);
                            }

                            propertyStore->Release();
                        }

                        if (deviceId.size() || deviceName.size()) {
                            result->Add(deviceId, deviceName);
                        }

                        device->Release();
                    }
                }
            }

            deviceCollection->Release();
        }

        deviceEnumerator->Release();
    }

    return result;
}

IMMDevice* WasapiOut::GetPreferredDevice() {
    IMMDevice* result = nullptr;

    std::string storedDeviceId = getDeviceId();
    if (storedDeviceId.size() > 0) {
        IMMDeviceCollection *deviceCollection = nullptr;

        if (this->enumerator) {
            HRESULT hr = this->enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
            if (hr == S_OK) {
                UINT deviceCount = 0;
                if (deviceCollection->GetCount(&deviceCount) == S_OK) {
                    for (UINT i = 0; i < deviceCount; i++) {
                        IMMDevice* device = nullptr;
                        LPWSTR deviceIdPtr;
                        std::string deviceId, deviceName;

                        hr = deviceCollection->Item(i, &device);
                        if (hr == S_OK) {
                            if (device->GetId(&deviceIdPtr) == S_OK) {
                                if (storedDeviceId == utf16to8(deviceIdPtr)) {
                                    result = device;
                                }

                                CoTaskMemFree(deviceIdPtr);

                                if (result == device) { /* found it! */
                                    goto found_or_done;
                                }
                            }

                            device->Release();
                        }
                    }
                }
found_or_done:
                deviceCollection->Release();
            }
        }

    }

    return result;
}

bool WasapiOut::Configure(IBuffer *buffer) {
    HRESULT result;

    if (!this->audioClient) {
        if (!this->enumerator) {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);

            result = CoCreateInstance(
                __uuidof(MMDeviceEnumerator),
                NULL,
                CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator),
                (void**) &this->enumerator);

            if (result != S_OK) {
                return false;
            }

            if (audioRoutingEnabled()) {
                this->notificationClient = new NotificationClient(this);

                if ((result = this->enumerator->RegisterEndpointNotificationCallback(this->notificationClient)) != S_OK) {
                    return false;
                }
            }
        }
    }

    if (waveFormat.Format.nChannels == buffer->Channels() &&
        waveFormat.Format.nSamplesPerSec == buffer->SampleRate())
    {
        return true;
    }

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    bool preferredDeviceOk = false;

    IMMDevice* preferredDevice = this->GetPreferredDevice();
    if (preferredDevice) {
        if ((result = preferredDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**) &this->audioClient)) == S_OK) {
            preferredDeviceOk = true;
            this->device = preferredDevice;
        }
    }

    if (!preferredDeviceOk) {
        if (preferredDevice) {
            preferredDevice->Release();
        }

        if ((result = this->enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &this->device)) != S_OK) {
            return false;
        }
    }

    if ((result = this->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**) &this->audioClient)) != S_OK) {
        return false;
    }

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

    WAVEFORMATEXTENSIBLE &wf = this->waveFormat;
    wf.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
    wf.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    wf.Format.nChannels = (WORD) buffer->Channels();
    wf.Format.wBitsPerSample = 8 * sizeof(float);
    wf.Format.nSamplesPerSec = (DWORD) buffer->SampleRate();
    wf.Samples.wValidBitsPerSample = 8 * sizeof(float);
    wf.Format.nBlockAlign = (wf.Format.wBitsPerSample / 8) * wf.Format.nChannels;
    wf.Format.nAvgBytesPerSec = wf.Format.nSamplesPerSec * wf.Format.nBlockAlign;
    wf.dwChannelMask = speakerConfig;
    wf.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;

    DWORD streamFlags = 0;
    if (this->audioClient->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *) &wf, 0) != S_OK) {
        streamFlags |= AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
    }

    double bufferLengthSeconds = ::prefs->GetDouble(PREF_BUFFER_LENGTH_SECONDS, 1.0);
    REFERENCE_TIME hundredNanos = REFERENCE_TIME (1000.0 * 1000.0 * 10.0 * bufferLengthSeconds);
    if ((result = this->audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, hundredNanos, 0, (WAVEFORMATEX *) &wf, NULL)) != S_OK) {
        return false;
    }

    if ((result = this->audioClient->GetBufferSize(&this->outputBufferFrames)) != S_OK) {
        return false;
    }

    this->latency = (float) outputBufferFrames / (float) buffer->SampleRate();

    if ((result = this->audioClient->GetService(__uuidof(IAudioRenderClient), (void**) &this->renderClient)) != S_OK) {
        return false;
    }

    if ((result = this->audioClient->GetService(__uuidof(ISimpleAudioVolume), (void**) &this->simpleAudioVolume)) != S_OK) {
        return false;
    }

    if ((result = this->audioClient->GetService(__uuidof(IAudioStreamVolume), (void**) &this->audioStreamVolume)) != S_OK) {
        return false;
    }

    if ((result = this->audioClient->GetService(__uuidof(IAudioClock), (void**) &this->audioClock)) != S_OK) {
        return false;
    }

    if ((result = this->audioClient->Start()) != S_OK) {
        return false;
    }

    this->state = StatePlaying;
    this->SetVolume(this->volume);

    return true;
}