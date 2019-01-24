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

#include "SndioOut.h"

#include <core/sdk/constants.h>
#include <core/sdk/IPreferences.h>
#include <math.h>
#include <limits.h>
#include <iostream>
#include <functional>

#define BUFFER_COUNT 16
#define ERROR(str) std::cerr << "SndioOut Error: " << str << "\n";
#define INFO(str) std::cerr << "SndioOut Info: " << str << "\n";
#define LOCK() std::unique_lock<std::mutex> lock(this->mutex);
#define WAIT() this->threadEvent.wait(lock);
#define NOTIFY() this->threadEvent.notify_all();

#define STOP() \
  if(started) { \
    if (!sio_stop(handle)) { \
      INFO("failed to stop sndio") \
      quit = true; \
      continue; \
    } \
    else { \
      INFO("stopped handle") \
      started = false; \
   } \
  } \
  else { \
    INFO("already stopped") \
  }

#define START() \
  if(!started) { \
    if (!sio_start(handle)) { \
      INFO("failed to start sndio") \
      quit = true; \
      continue; \
    } \
    else { \
      INFO("started handle") \
      started = true; \
   } \
  } \
  else { \
    INFO("handle already started") \
  }

#define PREF_DEVICE_ID "device_id"

using namespace musik::core::sdk;

static char* deviceId = nullptr;

extern "C" void SetPreferences(musik::core::sdk::IPreferences* prefs) {
    size_t len = prefs->GetString(PREF_DEVICE_ID, nullptr, 0, "");
    if (len > 1) {
        delete[] deviceId;
        deviceId = new char[len];
        prefs->GetString(PREF_DEVICE_ID, deviceId, len, "");
        prefs->Save();
        INFO("setting deviceId to " + std::string(deviceId));
    }
}

SndioOut::SndioOut() {
    INFO("---------- sndout.ctor ----------")
    this->volume = 1.0f;
    this->state = StateStopped;
    this->latency = 0.0;
    this->writeThread.reset(new std::thread(
        std::bind(&SndioOut::WriteLoop, this)));
}

SndioOut::~SndioOut() {
    this->PushCommand(Command::Quit);
    INFO("joining thread")
    this->writeThread->join();
    INFO("thread finished")
}

void SndioOut::Release() {
    delete this;
}

void SndioOut::PushCommand(Command command) {
    //INFO("PushCommand.start")
    {
        LOCK()
        commands.push_back(command);
    }
    NOTIFY()
    //INFO("PushCommand.end")
}

void SndioOut::Pause() {
    this->PushCommand(Command::Pause);
}

void SndioOut::Resume() {
    this->PushCommand(Command::Resume);
}

void SndioOut::SetVolume(double volume) {
    this->volume = volume;
    this->PushCommand(Command::SetVolume);
}

double SndioOut::GetVolume() {
    return this->volume;
}

void SndioOut::Stop() {
    this->PushCommand(Command::Stop);
    this->DiscardBuffers();
}

void SndioOut::Drain() {
    this->PushCommand(Command::Drain);
}

IDeviceList* SndioOut::GetDeviceList() {
    return nullptr;
}

bool SndioOut::SetDefaultDevice(const char* deviceId) {
    return false;
}

IDevice* SndioOut::GetDefaultDevice() {
    return nullptr;
}

int SndioOut::Play(IBuffer *buffer, IBufferProvider *provider) {
    std::this_thread::yield();

    if (this->state != StatePlaying) {
        return OutputInvalidState;
    }

    {
        LOCK()
        if (this->CountBuffersWithProvider(provider) >= BUFFER_COUNT) {
            return OutputBufferFull;
        }
        this->buffers.push_back(BufferContext{provider, buffer});
    }

    NOTIFY()
    return OutputBufferWritten;
}

void SndioOut::WriteLoop() {
    bool started = false;
    sio_hdl* handle = nullptr;
    sio_par pars = { 0 };
    short* pcm = nullptr;
    long pcmSamples = 0;
    float ditherState = 0.0;
    bool quit = false;
    BufferContext next {nullptr, nullptr};

    /* open the output device. we only do this once */
    const char* device = (::deviceId && strlen(::deviceId)) ? deviceId : nullptr;
    handle = sio_open(device, SIO_PLAY, 0);
    if (handle == nullptr) {
        INFO("failed to init device")
        quit = true;
    }
    else {
        if (!sio_start(handle)) {
            INFO("device opened, but couldn't start")
            quit = true;
        }
        else {
            started = true;
            INFO("device handle initialized")
        }
    }

    while (!quit) {
        {
            /* we wait until we have commands to process or samples to play */
            LOCK()
            while (!quit && !this->commands.size() &&
                   (this->state != StatePlaying || !this->buffers.size()))
            {
                //INFO("waiting")
                WAIT()
                //INFO("done waiting")
            }

            if (quit) {
                continue;
            }

            /* process commands */
            for (auto command : this->commands) {
                switch (command) {
                    case Command::Pause: {
                        INFO("command.pause")
                        STOP()
                        this->state = StatePaused;
                    } break;
                    case Command::Resume: {
                        INFO("command.resume")
                        START()
                        this->state = StatePlaying;
                    } break;
                    case Command::Stop: {
                        INFO("command.stop")
                        STOP()
                        this->state = StateStopped;
                    } break;
                    case Command::SetVolume: {
                        INFO("command.setvolume")
                        if (handle) {
                            sio_setvol(handle, lround(this->volume * SIO_MAXVOL));
                        }
                    } break;
                    case Command::Drain: {
                        INFO("command.drain")
                        STOP()
                        START()
                    } break;
                    case Command::Quit: {
                        quit = true;
                        continue;
                    } break;
                }
            }

            this->commands.clear();

            /* play the next buffer, if it exists */
            if (started &&
                this->state == StatePlaying &&
                this->buffers.size())
            {
                next = this->buffers.front();
                this->buffers.pop_front();
            }
            else {
                //INFO(std::string("state=") + std::to_string((int) this->state));
                //INFO(std::string("count=") + std::to_string((int) this->buffers.size()));
                continue;
            }
        }

        auto buffer = next.buffer;

        if (buffer) {
            /* ensure our output params are setup properly based on this
            buffer's params */
            if (pars.pchan != buffer->Channels() ||
                pars.rate != buffer->SampleRate())
            {
                INFO("updating output params")

                STOP()

                sio_initpar(&pars);
                pars.pchan = buffer->Channels();
                pars.rate = buffer->SampleRate();
                pars.sig = 1;
                pars.le = SIO_LE_NATIVE;
                pars.bits = 16;
                pars.appbufsz = (pars.rate * 250) / 1000;

                if (!sio_setpar(handle, &pars)) {
                    INFO("sio_setpar() failed");
                    quit = true;
                    continue;
                }

                if (!sio_getpar(handle, &pars)) {
                    INFO("sio_getpar() failed");
                    quit = true;
                    continue;
                }

                START()

                this->latency = (double) pars.bufsz / pars.pchan / pars.rate;
            }

            /* allocate PCM buffer, if needed. most of the time we'll be
            able to re-use the previously allocated one. */
            long samples = buffer->Samples();
            if (!pcm || samples > pcmSamples) {
                delete[] pcm;
                pcm = new short[samples];
                pcmSamples = samples;
            }

            /* convert to 16-bit PCM */
            float* src = buffer->BufferPointer();
            short* dst = pcm;
            for (long i = 0; i < samples; i++) {
                float sample = *src;
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                sample *= SHRT_MAX;

                /* triangle (high pass) dither, based on Audacity's
                implementation */
                float r = (rand() / (float) RAND_MAX - 0.5f);
                sample = sample + r - ditherState;
                ditherState = r;

                *dst = sample;
                ++dst; ++src;
            }

            /* write the entire output buffer. this may require multiple passes;
            that's ok, just loop until we're done */
            char* data = (char*) pcm;
            size_t dataLength = samples * sizeof(short);
            size_t totalWritten = 0;

            {
//              LOCK()
//              INFO("start write")
                while (totalWritten < dataLength) {
                    size_t remaining = dataLength - totalWritten;
                    size_t written = sio_write(handle, data, remaining);
                    if (written == 0) { INFO("failed to write!") break; }
//                  else { std::cerr << "wrote " << written << " of " << remaining << " bytes\n"; }
                    totalWritten += written;
                    data += written;
                }
//              INFO("end write")
            }

            next.provider->OnBufferProcessed(buffer);
        }
    }

    /* done, free remaining buffers and close the handle */
    if (handle) {
        INFO("closing")
        sio_close(handle);
        handle = nullptr;
    }

    this->DiscardBuffers();

    delete[] pcm;
}

double SndioOut::Latency() {
    return this->latency;
}

void SndioOut::DiscardBuffers() {
    std::list<BufferContext> toNotify;
    {
        LOCK()
        std::swap(toNotify, this->buffers);
    }

    for (auto& it : toNotify) {
        it.provider->OnBufferProcessed(it.buffer);
    }
}

size_t SndioOut::CountBuffersWithProvider(IBufferProvider* provider) {
    size_t count = 0;
    for (auto& it : this->buffers) {
        if (it.provider == provider) {
            ++count;
        }
    }
    return count;
}

