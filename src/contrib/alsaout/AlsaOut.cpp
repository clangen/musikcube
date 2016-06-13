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

#define BUFFER_COUNT 8
#define PCM_ACCESS_TYPE SND_PCM_ACCESS_RW_INTERLEAVED
#define PCM_FORMAT SND_PCM_FORMAT_FLOAT_LE

#define LOCK(x) \
    /*std::f << "locking " << x << "\n";*/ \
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
    snd_pcm_state_t state = snd_pcm_state(pcm);

    return
        state == SND_PCM_STATE_RUNNING ||
        state == SND_PCM_STATE_PREPARED;
}

using namespace musik::core::audio;

AlsaOut::AlsaOut()
: pcmHandle(NULL)
, device("default")
, channels(2)
, rate(44100)
, volume(1.0)
, quit(false)
, initialized(false) {
    std::cerr << "AlsaOut::AlsaOut() called" << std::endl;
    this->writeThread.reset(new boost::thread(boost::bind(&AlsaOut::WriteLoop, this)));
}

AlsaOut::~AlsaOut() {
    std::cerr << "AlsaOut: destructor\n";

    std::cerr << "AlsaOut: waiting for thread...\n";
    {
        LOCK("dtor");
        this->quit = true;
        NOTIFY();
    }

    this->writeThread->join();
    this->CloseDevice();
}

void AlsaOut::CloseDevice() {
    if (this->pcmHandle) {
        std::cerr << "AlsaOut: closing PCM handle\n";
        snd_pcm_close(this->pcmHandle);
        this->pcmHandle = NULL;
    }
}

void AlsaOut::InitDevice() {
    int err, dir;
    unsigned int rate = (unsigned int) this->rate;

    if ((err = snd_pcm_open(&this->pcmHandle, this->device.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
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

    snd_pcm_nonblock(pcmHandle, 0);

    std::cerr << "AlsaOut: device seems to be prepared for use!\n";
    this->initialized = true;

    return;

error:
    this->CloseDevice();
}

void AlsaOut::Destroy() {
    delete this;
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
    }
}

void AlsaOut::Resume() {
    LOCK("resume");

    if (this->pcmHandle) {
        snd_pcm_pause(this->pcmHandle, 0);
    }
}

void AlsaOut::SetVolume(double volume) {
    LOCK("set volume");
    this->volume = volume;
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
                while (!quit && !this->buffers.size()) {
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


                float volume = (float) this->volume;

                /* software volume; alsa doesn't support this internally. this is about
                as terrible as an algorithm can be -- it's just a linear ramp. */
                float *buffer = next->buffer->BufferPointer();
                for (size_t i = 0; i < samples * channels; i++) {
                    (*buffer) *= volume;
                    ++buffer;
                }

                WRITE_BUFFER(this->pcmHandle, next, samples); /* sets 'err' */

                if (err == -EINTR || err == -EPIPE || err == -ESTRPIPE) {
                    if (!snd_pcm_recover(this->pcmHandle, err, 1)) {
                        /* try one more time... */
                        WRITE_BUFFER(this->pcmHandle, next, samples);
                    }
                }

                if (err > 0 && err < (int) samples) {
                    std::cerr << "AlsaOut: short write. expected=" << samples << ", actual=" << err << std::endl;
                }

                next->provider->OnBufferProcessed(next->buffer);
            }
        }
    }
}

bool AlsaOut::Play(IBuffer *buffer, IBufferProvider* provider) {
    this->SetFormat(buffer);

    {
        LOCK("play");

        if (!playable(this->pcmHandle) ||
            this->CountBuffersWithProvider(provider) >= BUFFER_COUNT)
        {
            return false;
        }

        std::shared_ptr<BufferContext> context(new BufferContext());
        context->buffer = buffer;
        context->provider = provider;

        this->buffers.push_back(context);

        NOTIFY();
    }

    return true;
}

void AlsaOut::SetFormat(IBuffer *buffer) {
    LOCK("set format");

    if (this->channels != buffer->Channels() ||
        this->rate != buffer->SampleRate() ||
        this->pcmHandle == NULL)
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
