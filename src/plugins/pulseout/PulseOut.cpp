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
#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>
#include <pulse/volume.h>
#include <pulse/pulseaudio.h>
#include <pulse/thread-mainloop.h>
#include <math.h>

using namespace musik::core::sdk;

typedef std::unique_lock<std::recursive_mutex> Lock;
typedef musik::core::sdk::IOutput IOutput;

static musik::core::sdk::IPreferences* prefs = nullptr;

#define PREF_FORCE_LINEAR_VOLUME "force_linear_volume"
#define PREF_DEVICE_ID "device_id"

class PulseDevice : public musik::core::sdk::IDevice {
    public:
        PulseDevice(const std::string& id, const std::string& name) {
            this->id = id;
            this->name = name;
        }

        virtual void Release() {
            delete this;
        }

        virtual const char* Name() const {
            return name.c_str();
        }

        virtual const char* Id() const {
            return id.c_str();
        }

    private:
        std::string name, id;
};

class PulseDeviceList : public musik::core::sdk::IDeviceList {
    public:
        virtual void Release() {
            delete this;
        }

        virtual size_t Count() const {
            return devices.size();
        }

        virtual const IDevice* At(size_t index) const {
            return &devices.at(index);
        }

        void Add(const std::string& id, const std::string& name) {
            devices.push_back(PulseDevice(id, name));
        }

    private:
        std::vector<PulseDevice> devices;
};

struct DeviceListContext {
    pa_threaded_mainloop* mainLoop;
    PulseDeviceList* devices;
};

static void deviceEnumerator(pa_context* context, const pa_sink_info* info, int eol, void* userdata) {
    DeviceListContext* deviceListContext = (DeviceListContext*) userdata;
    if (info) {
        deviceListContext->devices->Add(info->name, info->description);
    }

    if (eol) {
        pa_threaded_mainloop_signal(deviceListContext->mainLoop, 0);
    }
}

static std::string getDeviceId() {
    return getPreferenceString<std::string>(prefs, PREF_DEVICE_ID, "");
}

extern "C" void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    ::prefs = prefs;
    prefs->GetBool(PREF_FORCE_LINEAR_VOLUME, false);
    prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    prefs->Save();
}

PulseOut::PulseOut() {
    std::cerr << "PulseOut::PulseOut() called" << std::endl;
    this->audioConnection = nullptr;
    this->state = StateStopped;
    this->volume = 1.0f;
    this->volumeUpdated = false;
    this->channels = 0;
    this->rate = 0;
    this->linearVolume = false;
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

void PulseOut::Drain() {
    Lock lock(this->stateMutex);

    if (this->state != StateStopped && this->audioConnection) {
        std::cerr << "draining...\n";
        pa_blocking_drain(this->audioConnection, 0);
        std::cerr << "drained...\n";
    }
}

musik::core::sdk::IDevice* PulseOut::GetDefaultDevice() {
    return findDeviceById<PulseDevice, IOutput>(this, getDeviceId());
}

bool PulseOut::SetDefaultDevice(const char* deviceId) {
    return setDefaultDevice<IPreferences, PulseDevice, IOutput>(prefs, this, PREF_DEVICE_ID, deviceId);
}

musik::core::sdk::IDeviceList* PulseOut::GetDeviceList() {
    PulseDeviceList* result = new PulseDeviceList();

    auto mainLoop = pa_threaded_mainloop_new();
    if (mainLoop) {
        auto context = pa_context_new(pa_threaded_mainloop_get_api(mainLoop), "musikcube");
        if (context) {
            if (pa_context_connect(context, nullptr, (pa_context_flags_t) 0, nullptr) >= 0) {
                if (pa_threaded_mainloop_start(mainLoop) >= 0) {
                    bool contextOk = false;
                    for (;;) {
                        pa_context_state_t state;
                        state = pa_context_get_state(context);

                        if (state == PA_CONTEXT_READY) {
                            contextOk = true;
                            break;
                        }
                        else if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
                            break;
                        }

                        pa_threaded_mainloop_wait(mainLoop);
                    }

                    pa_threaded_mainloop_lock(mainLoop);

                    if (contextOk) {
                        DeviceListContext dlc;
                        dlc.mainLoop = mainLoop;
                        dlc.devices = result;

                        auto op = pa_context_get_sink_info_list(context, deviceEnumerator, (void*) &dlc);
                        if (op) {
                            while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
                                pa_threaded_mainloop_wait(mainLoop);
                            }

                            pa_operation_unref(op);
                        }
                    }

                    pa_threaded_mainloop_unlock(mainLoop);
                }

                pa_context_disconnect(context);
                pa_context_unref(context);
            }
        }

        pa_threaded_mainloop_stop(mainLoop);
        pa_threaded_mainloop_free(mainLoop);
    }

    return result;
}

std::string PulseOut::GetPreferredDeviceId() {
    std::string deviceId = getDeviceId();
    auto device = findDeviceById<PulseDevice>(this, deviceId);
    if (device) {
        device->Release();
        return deviceId;
    }
    return "";
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

        std::string deviceId = this->GetPreferredDeviceId();

        /* output to preferred device id, as specified in prefs */
        this->audioConnection = pa_blocking_new(
            nullptr,
            "musikcube",
            PA_STREAM_PLAYBACK,
            deviceId.size() ? deviceId.c_str() : nullptr,
            "music",
            &spec,
            nullptr,
            nullptr,
            0);

        if (!this->audioConnection && deviceId.size()) {
            /* fall back to default if preferred is not found */
            this->audioConnection = pa_blocking_new(
                nullptr,
                "musikcube",
                PA_STREAM_PLAYBACK,
                nullptr,
                "music",
                &spec,
                nullptr,
                nullptr,
                0);
        }

        if (this->audioConnection) {
            this->rate = buffer->SampleRate();
            this->channels = buffer->Channels();
            this->state = StatePlaying;
            this->linearVolume = ::prefs->GetBool(PREF_FORCE_LINEAR_VOLUME, false);
            this->SetVolume(this->volume);
        }
    }
}

void PulseOut::Release() {
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
    if (volume > 1.0) { volume = 1.0; }
    if (volume < 0) { volume = 0; }
    this->volume = volume;
    this->volumeUpdated = false;
    if (this->audioConnection) {
        int normalized;
        if (this->linearVolume) {
            normalized = (int) round((double) PA_VOLUME_NORM * volume);
        }
        else {
            normalized = (int) pa_sw_volume_from_linear(this->volume);
        }
        this->volumeUpdated = pa_blocking_set_volume(this->audioConnection, normalized, 0) == 0;
        //std::cerr << "PulseOut: volumeUpdated = " << this->volumeUpdated << ", value = " << normalized << "\n";
    }
}

double PulseOut::GetVolume() {
    return this->volume;
}

int PulseOut::Play(IBuffer *buffer, IBufferProvider* provider) {
    int error = 0;

    {
        Lock lock(this->stateMutex);

        if (this->state == StatePaused) {
            return OutputInvalidState;
        }

        this->OpenDevice(buffer);

        if (!this->audioConnection || this->state != StatePlaying) {
            return OutputInvalidState;
        }

        if (!this->volumeUpdated) {
            this->SetVolume(this->volume);
        }

        pa_blocking_write(
            this->audioConnection,
            buffer->BufferPointer(),
            buffer->Samples() * sizeof(float),
            &error);

        if (error > 0) {
            this->CloseDevice();
            return 1000; /* try again in a second */
        }
    }

    provider->OnBufferProcessed(buffer);
    return OutputBufferWritten;
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
