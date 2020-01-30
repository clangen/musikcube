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

#include "AlsaOut.h"

#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>

static musik::core::sdk::IPreferences* prefs;

#define BUFFER_COUNT 16
#define PCM_ACCESS_TYPE SND_PCM_ACCESS_RW_INTERLEAVED
#define PCM_FORMAT SND_PCM_FORMAT_FLOAT_LE
#define PREF_DEVICE_ID "device_id"

#define LOCK(x) \
    /*std::cerr << "locking " << x << "\n";*/ \
    boost::recursive_mutex::scoped_lock lock(this->stateMutex); \
    /*std::cerr << "locked " << x << "\n";*/ \

#define WAIT() this->threadEvent.wait(lock);
#define NOTIFY() this->threadEvent.notify_all();
#define CHECK_QUIT() if (this->quit) { return; }
#define PRINT_ERROR(x) std::cerr << "AlsaOut: error! " << snd_strerror(x) << std::endl;

#define WRITE_BUFFER(handle, context, samples) \
    err = snd_pcm_writei(handle, context->buffer->BufferPointer(), samples); \
    if (err < 0) { PRINT_ERROR(err); }

static inline bool playable(snd_pcm_t* pcm) {
    if (!pcm) {
        return false;
    }

    snd_pcm_state_t state = snd_pcm_state(pcm);

    if (state == SND_PCM_STATE_RUNNING ||
        state == SND_PCM_STATE_PREPARED)
    {
        return true;
    }

    std::cerr << "AlsaOut: invalid device state: " << (int) state << "\n";
    return false;
}

using namespace musik::core::sdk;

class AlsaDevice : public IDevice {
    public:
        AlsaDevice(const std::string& id, const std::string& name) {
            this->id = id;
            this->name = name;
        }

        virtual void Release() override {
            delete this;
        }

        virtual const char* Name() const override {
            return name.c_str();
        }

        virtual const char* Id() const override {
            return id.c_str();
        }

    private:
        std::string name, id;
};

class AlsaDeviceList : public musik::core::sdk::IDeviceList {
    public:
        virtual void Release() override {
            delete this;
        }

        virtual size_t Count() const override {
            return devices.size();
        }

        virtual const IDevice* At(size_t index) const override {
            return &devices.at(index);
        }

        void Add(const std::string& id, const std::string& name) {
            devices.push_back(AlsaDevice(id, name));
        }

    private:
        std::vector<AlsaDevice> devices;
};

extern "C" void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    ::prefs = prefs;
    prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    prefs->Save();
}

static std::string getDeviceId() {
    return getPreferenceString<std::string>(prefs, PREF_DEVICE_ID, "");
}

AlsaOut::AlsaOut()
: pcmHandle(nullptr)
, device("default")
, channels(2)
, rate(44100)
, volume(1.0)
, quit(false)
, paused(false)
, latency(0)
, initialized(false) {
    std::cerr << "AlsaOut::AlsaOut() called" << std::endl;
    this->writeThread.reset(new boost::thread(boost::bind(&AlsaOut::WriteLoop, this)));
}

AlsaOut::~AlsaOut() {
    std::cerr << "AlsaOut: destructor\n";

    {
        LOCK("dtor");
        this->quit = true;
        NOTIFY();
    }

    std::cerr << "AlsaOut: joining...\n";
    this->writeThread->join();

    std::cerr << "AlsaOut: closing device...\n";
    this->CloseDevice();

    std::cerr << "AlsaOut: destroyed.\n";
}

void AlsaOut::CloseDevice() {
    if (this->pcmHandle) {
        std::cerr << "AlsaOut: closing PCM handle\n";
        snd_pcm_close(this->pcmHandle);
        this->pcmHandle = nullptr;
        this->latency = 0.0;
    }
}

musik::core::sdk::IDevice* AlsaOut::GetDefaultDevice() {
    return findDeviceById<AlsaDevice, IOutput>(this, getDeviceId());
}

bool AlsaOut::SetDefaultDevice(const char* deviceId) {
    return setDefaultDevice<IPreferences, AlsaDevice, IOutput>(prefs, this, PREF_DEVICE_ID, deviceId);
}

IDeviceList* AlsaOut::GetDeviceList() {
    AlsaDeviceList* result = new AlsaDeviceList();

    /* https://stackoverflow.com/a/6870226 */
    char** hints;
    if (snd_device_name_hint(-1, "pcm", (void***)&hints) == 0) {
        char** n = hints;
        while (*n != nullptr) {
            char *name = snd_device_name_get_hint(*n, "NAME");
            if (name) {
                std::string stdName = name;
                if (stdName != "default") {
                    result->Add(stdName, stdName);
                }
                free(name);
            }
            ++n;
        }

        snd_device_name_free_hint((void**) hints);
    }

    size_t n = result->Count();
    return result;
}

std::string AlsaOut::GetPreferredDeviceId() {
    std::string result;

    if (prefs) {
        std::string storedDeviceId = getDeviceId();

        auto deviceList = GetDeviceList();
        if (deviceList) {
            for (size_t i = 0; i < deviceList->Count(); i++) {
                if (deviceList->At(i)->Id() == storedDeviceId) {
                    result = storedDeviceId;
                    break;
                }
            }
            deviceList->Release();
        }
    }

    return result;
}

void AlsaOut::InitDevice() {
    int err, dir;
    unsigned int rate = (unsigned int) this->rate;

    std::string preferredDeviceId = this->GetPreferredDeviceId();
    bool preferredOk = false;

    if (preferredDeviceId.size() > 0) {
        if ((err = snd_pcm_open(&this->pcmHandle, preferredDeviceId.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            std::cerr << "AlsaOut: cannot opened preferred device id " << preferredDeviceId << ": " << snd_strerror(err) << std::endl;
        }
        else {
            preferredOk = true;
        }
    }

    if (!preferredOk && (err = snd_pcm_open(&this->pcmHandle, this->device.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        std::cerr << "AlsaOut: cannot open audio device 'default' :" << snd_strerror(err) << std::endl;
        goto error;
    }

    if ((err = snd_pcm_hw_params_malloc(&hardware)) < 0) {
        std::cerr << "AlsaOut: cannot allocate hardware parameter structure " << snd_strerror(err) << std::endl;
        goto error;
    }

    if ((err = snd_pcm_hw_params_any(pcmHandle, hardware)) < 0) {
        std::cerr << "AlsaOut: cannot initialize hardware parameter structure " << snd_strerror(err) << std::endl;
        goto error;
    }

    if ((err = snd_pcm_hw_params_set_access(pcmHandle, hardware, PCM_ACCESS_TYPE)) < 0) {
        std::cerr << "AlsaOut: cannot set access type " << snd_strerror(err) << std::endl;
        goto error;
    }

    if ((err = snd_pcm_hw_params_set_format(pcmHandle, hardware, PCM_FORMAT)) < 0) {
        std::cerr << "AlsaOut: cannot set sample format " << snd_strerror(err) << std::endl;
        goto error;
    }

    if ((err = snd_pcm_hw_params_set_rate_near(pcmHandle, hardware, &rate, 0)) < 0) {
        std::cerr << "AlsaOut: cannot set sample rate " << snd_strerror(err) << std::endl;
        goto error;
    }

    if ((err = snd_pcm_hw_params_set_channels(pcmHandle, hardware, this->channels)) < 0) {
        std::cerr << "AlsaOut: cannot set channel count " << snd_strerror(err) << std::endl;
        goto error;
    }

    if ((err = snd_pcm_hw_params(pcmHandle, hardware)) < 0) {
        std::cerr << "AlsaOut: cannot set parameters " << snd_strerror(err) << std::endl;
        goto error;
    }

    snd_pcm_hw_params_free(hardware);

    if ((err = snd_pcm_prepare (pcmHandle)) < 0) {
        std::cerr << "AlsaOut: cannot prepare audio interface for use " << snd_strerror(err) << std::endl;
        goto error;
    }

    snd_pcm_nonblock(pcmHandle, 0); /* operate in blocking mode for simplicity */

    std::cerr << "AlsaOut: device seems to be prepared for use!\n";
    this->initialized = true;

    return;

error:
    this->CloseDevice();
}

void AlsaOut::Release() {
    delete this;
}

double AlsaOut::Latency() {
    if (latency <= 0.0f) {
        LOCK("latency_calc");

        if (this->pcmHandle && this->rate && this->channels) {
            snd_pcm_uframes_t bufferSize = 0, periodSize = 0;
            snd_pcm_get_params(this->pcmHandle, &bufferSize, &periodSize);

            if (bufferSize) {
                this->latency =
                (double) bufferSize /
                (double) (this->rate * this->channels * sizeof(float));
            }
        }
    }

    return this->latency;
}

void AlsaOut::Stop() {
    std::list<std::shared_ptr<BufferContext> > toNotify;

    {
        LOCK("stop");

        std::swap(this->buffers, toNotify);

        if (this->pcmHandle) {
            snd_pcm_drop(this->pcmHandle);
            this->CloseDevice();
        }
    }

    auto it = toNotify.begin();
    while (it != toNotify.end()) {
        ((*it)->provider)->OnBufferProcessed((*it)->buffer);
        ++it;
    }
}

void AlsaOut::Pause() {
    LOCK("pause");

    if (this->pcmHandle) {
        snd_pcm_pause(this->pcmHandle, 1);
        this->paused = true;
    }
}

void AlsaOut::Resume() {
    LOCK("resume");

    if (this->pcmHandle) {
        snd_pcm_pause(this->pcmHandle, 0);
        this->paused = false;
        NOTIFY();
    }
}

void AlsaOut::SetVolume(double volume) {
    LOCK("set volume");
    this->volume = volume;
}

double AlsaOut::GetVolume() {
    return this->volume;
}

void AlsaOut::WriteLoop() {
    {
        LOCK("thread: init");
        while (!quit && !initialized) {
            WAIT();
        }
    }

    {
        while (!quit) {
            std::shared_ptr<BufferContext> next;

            {
                LOCK("thread: waiting for buffer");
                while (!quit && (!playable(this->pcmHandle) || !this->buffers.size())) {
                    WAIT();
                }

                CHECK_QUIT();

                next = this->buffers.front();
                this->buffers.pop_front();
            }

            int err;

            if (next) {
                size_t samples = next->buffer->Samples();
                size_t channels = next->buffer->Channels();
                size_t samplesPerChannel = samples / channels;
                float volume = (float) this->volume;

                /* software volume; alsa doesn't support this internally. this is about
                as terrible as an algorithm can be -- it's just a linear ramp. */
                //std::cerr << "volume=" << volume << std::endl;
                if (volume != 1.0f) {
                    float *buffer = next->buffer->BufferPointer();
                    for (size_t i = 0; i < samples; i++) {
                        (*buffer) *= volume;
                        ++buffer;
                    }
                }

                WRITE_BUFFER(this->pcmHandle, next, samplesPerChannel); /* sets 'err' */

                if (err == -EINTR || err == -EPIPE || err == -ESTRPIPE) {
                    if (!snd_pcm_recover(this->pcmHandle, err, 1)) {
                        /* try one more time... */
                        WRITE_BUFFER(this->pcmHandle, next, samplesPerChannel);
                    }
                }

                if (err > 0 && err < (int) samplesPerChannel) {
                    std::cerr << "AlsaOut: short write. expected=" << samplesPerChannel << ", actual=" << err << std::endl;
                }

                next->provider->OnBufferProcessed(next->buffer);
            }
        }
    }

    std::cerr << "AlsaOut: thread finished\n";
}

int AlsaOut::Play(IBuffer *buffer, IBufferProvider* provider) {
    this->SetFormat(buffer);

    {
        LOCK("play");

        if (this->paused) {
            return OutputInvalidState;
        }

        if (this->CountBuffersWithProvider(provider) >= BUFFER_COUNT) {
            return OutputBufferFull;
        }

        std::shared_ptr<BufferContext> context(new BufferContext());
        context->buffer = buffer;
        context->provider = provider;

        this->buffers.push_back(context);

        if (!playable(this->pcmHandle)) {
            std::cerr << "AlsaOut: sanity check -- stream not playable. adding buffer to queue anyway\n";
        }
        else {
            NOTIFY();
        }
    }

    return OutputBufferWritten;
}

void AlsaOut::Drain() {
    LOCK("drain");

    if (this->pcmHandle) {
        std::cerr << "draining...\n";
        snd_pcm_drain(this->pcmHandle);
        std::cerr << "drained\n";
    }
}

void AlsaOut::SetFormat(IBuffer *buffer) {
    LOCK("set format");

    if (this->channels != buffer->Channels() ||
        this->rate != buffer->SampleRate() ||
        this->pcmHandle == nullptr)
    {
        this->channels = buffer->Channels();
        this->rate = buffer->SampleRate();

        this->CloseDevice();

        this->InitDevice();

        int err = snd_pcm_set_params(
            this->pcmHandle,
            PCM_FORMAT,
            PCM_ACCESS_TYPE,
            this->channels,
            this->rate,
            1, /* allow resampling */
            500000); /* 0.5s latency */

        if (err > 0) {
            std::cerr << "AlsaOut: set format error: " << snd_strerror(err) << std::endl;
        }
        else {
            this->SetVolume(this->volume);
        }

        std::cerr << "AlsaOut: device format initialized from buffer\n";
    }
}

size_t AlsaOut::CountBuffersWithProvider(IBufferProvider* provider) {
    LOCK("count");

    size_t count = 0;
    auto it = this->buffers.begin();
    while (it != this->buffers.end()) {
        if ((*it)->provider == provider) {
            ++count;
        }
        ++it;
    }

    return count;
}
