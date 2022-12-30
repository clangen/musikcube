//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include "PortAudioOut.h"

#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/ISchema.h>
#include <musikcore/sdk/IDebug.h>

#include <cmath>

using namespace musik::core::sdk;

#ifdef WIN32
    #include <Windows.h>
    #define usleep(x) Sleep(x / 1000);
    #define DLLEXPORT __declspec(dllexport)
#else
    #include <unistd.h>
    #define DLLEXPORT
#endif

#define PREF_DEFAULT_SAMPLE_RATE "default_sample_rate"
#define PREF_DEFAULT_DEVICE "default_device"
#define TAG "PortAudioOut"

static IPreferences* prefs = nullptr;
static IDebug* debug = nullptr;

extern "C" DLLEXPORT void SetDebug(IDebug* debug) {
    ::debug = debug;
}

extern "C" DLLEXPORT void SetPreferences(IPreferences* prefs) {
    ::prefs = prefs;
}

extern "C" DLLEXPORT musik::core::sdk::ISchema* GetSchema() {
    auto schema = new TSchema<>();
    return schema;
}

static void logPaError(const std::string method, PaError error) {
    std::string err = method + "() return code: " + std::to_string(error);
    if (error != 0) {
        ::debug->Warning(TAG, err.c_str());
    }
    else {
        ::debug->Info(TAG, err.c_str());
    }
}

class PortAudioDevice : public IDevice {
    public:
        PortAudioDevice(PaDeviceIndex index, const std::string name) { this->index = index; this->name = name; }
        virtual void Release() override { delete this; }
        virtual const char* Name() const override { return name.c_str(); }
        virtual const char* Id() const override { return name.c_str(); }
        PaDeviceIndex Index() const { return index; }
    private:
        PaDeviceIndex index;
        std::string name;
};

class PortAudioDeviceList : public musik::core::sdk::IDeviceList {
    public:
        virtual void Release() override { delete this; }
        virtual size_t Count() const override { return devices.size(); }
        virtual const IDevice* At(size_t index) const override { return &devices.at(index); }
        void Add(PaDeviceIndex index, const std::string name) { devices.push_back(PortAudioDevice(index, name)); }
    private:
        std::vector<PortAudioDevice> devices;
};

PortAudioOut::PortAudioOut() {
    this->volume = 1.0f;
    this->state = StateStopped;
    logPaError("Pa_Initialize", Pa_Initialize());
}

PortAudioOut::~PortAudioOut() {
    this->Stop();
    logPaError("Pa_Terminate", Pa_Terminate());
    if (this->deviceList) {
        this->deviceList->Release();
        this->deviceList = nullptr;
    }
}

void PortAudioOut::Release() {
    delete this;
}

void PortAudioOut::Pause() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (this->paStream) {
        logPaError("Pa_StopStream", Pa_StopStream(this->paStream));
    }
    this->state = StatePaused;
}

void PortAudioOut::Resume() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (this->paStream) {
        logPaError("Pa_StopStream", Pa_StartStream(this->paStream));
    }
    this->state = StatePlaying;
}

void PortAudioOut::SetVolume(double volume) {
    this->volume = volume;
}

double PortAudioOut::GetVolume() {
    return this->volume;
}

void PortAudioOut::Stop() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (this->paStream) {
        logPaError("Pa_AbortStream", Pa_AbortStream(this->paStream));
        logPaError("Pa_CloseStream", Pa_CloseStream(this->paStream));
        this->paStream = nullptr;
    }
    this->state = StateStopped;
}

void PortAudioOut::Drain() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (this->paStream) {
        logPaError("Pa_StopStream", Pa_StopStream(this->paStream));
    }
}

IDeviceList* PortAudioOut::GetDeviceList() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    PortAudioDeviceList* result = new PortAudioDeviceList();
    PaDeviceIndex count = Pa_GetDeviceCount();
    for (PaDeviceIndex i = 0; i < count; i++) {
        const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
        if (deviceInfo && deviceInfo->maxOutputChannels > 0) {
            if (deviceInfo->name) {
                const std::string name(deviceInfo->name);
                if (name != "default") {
                    result->Add(i, deviceInfo->name);
                }
            }
        }
    }
    return result;
}

bool PortAudioOut::SetDefaultDevice(const char* deviceId) {
    ::prefs->SetString(PREF_DEFAULT_DEVICE, deviceId ? deviceId : "");
    return true;
}

IDevice* PortAudioOut::GetDefaultDevice() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (!this->deviceList) {
        this->deviceList = this->GetDeviceList();
    }
    const std::string defaultDeviceName =
        getPreferenceString<std::string>(::prefs, PREF_DEFAULT_DEVICE, "default");
    for (int i = 0; i < this->deviceList->Count(); i++) {
        auto device = static_cast<const PortAudioDevice*>(this->deviceList->At(i));
        if (device->Name() == defaultDeviceName) {
            return new PortAudioDevice(device->Index(), defaultDeviceName);
        }
    }
    return new PortAudioDevice(Pa_GetDefaultOutputDevice(), "default");
}

OutputState PortAudioOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    bool bufferWritten = false;

    {
        std::unique_lock<decltype(this->mutex)> lock(this->mutex);

        if (this->state == StatePaused) {
            return OutputState::InvalidState;
        }

        if (!this->paStream) {
            auto device = static_cast<PortAudioDevice*>(this->GetDefaultDevice());
            if (device) {
                PaStreamParameters params = { 0 };
                params.device = device->Index();
                params.channelCount = buffer->Channels();
                params.sampleFormat = paFloat32;
                params.suggestedLatency = Pa_GetDeviceInfo(device->Index())->defaultHighOutputLatency;
                params.hostApiSpecificStreamInfo = nullptr;
                PaError result = Pa_OpenStream(
                    &this->paStream,
                    nullptr,
                    &params,
                    buffer->SampleRate(),
                    buffer->Samples() / buffer->Channels(),
                    0 /* stream flags */,
                    nullptr /* buffer callback; nullptr = blocking */,
                    nullptr /* callback context */);
                logPaError("Pa_OpenStream", result);
                if (result != paNoError) {
                    return OutputState::InvalidState;
                }
                result = Pa_StartStream(this->paStream);
                logPaError("Pa_StartStream", result);
                device->Release();
            }
        }

        if (this->paStream) {
            auto audio = buffer->BufferPointer();
            auto const samples = buffer->Samples();
            auto const frameCount = samples / buffer->Channels();
            auto const writeAvailable = Pa_GetStreamWriteAvailable(this->paStream);

            if (writeAvailable < frameCount) {
                int retryMs = buffer->SampleRate() / samples;
                return static_cast<OutputState>(retryMs);
            }

            if (volume != 1.0f) {
                float gain = 0.0;
                if (volume > 0) {
                    float dB = 20.0 * std::log(volume/1.0);
                    gain = std::pow(10.0, dB / 20.0);
                }
                for (size_t i = 0; i < samples; i++) {
                    (*audio) *= gain;
                    ++audio;
                }
            }

            PaError result = Pa_WriteStream(
                this->paStream, buffer->BufferPointer(), frameCount);

            if (result == paNoError) {
                bufferWritten = true;
            }
        }
    }

    if (bufferWritten) {
        provider->OnBufferProcessed(buffer);
        return OutputState::BufferWritten;
    }

    return OutputState::InvalidState;
}

double PortAudioOut::Latency() {
    std::unique_lock<decltype(this->mutex)> lock(this->mutex);
    if (this->paStream) {
        const PaStreamInfo* info = Pa_GetStreamInfo(this->paStream);
        if (info) {
            return info->outputLatency;
        }
    }
    return 0.0;
}

int PortAudioOut::GetDefaultSampleRate() {
    return -1;
}
