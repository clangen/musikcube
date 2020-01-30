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

#include "pch.hpp"

#include <kiss_fftr.h>
#include <core/debug.h>
#include <core/audio/Stream.h>
#include <core/audio/Player.h>
#include <core/audio/Visualizer.h>
#include <core/plugin/PluginFactory.h>
#include <core/sdk/constants.h>

#include <algorithm>
#include <math.h>
#include <future>

#define MAX_PREBUFFER_QUEUE_COUNT 8
#define FFT_N 512
#define PI 3.14159265358979323846

using namespace musik::core::audio;
using namespace musik::core::sdk;

using std::min;
using std::max;

static std::string TAG = "Player";
static float* hammingWindow = nullptr;

using Listener = Player::EventListener;
using ListenerList = std::list<Listener*>;

namespace musik {
    namespace core {
        namespace audio {
            void playerThreadLoop(Player* player);

            struct FftContext {
                FftContext() {
                    samples = 0;
                    cfg = nullptr;
                    deinterleaved = nullptr;
                    scratch = nullptr;
                }

                ~FftContext() {
                    Reset();
                }

                void Reset() {
                    kiss_fftr_free(cfg);
                    delete[] deinterleaved;
                    delete[] scratch;
                    cfg = nullptr;
                    deinterleaved = nullptr;
                    scratch = nullptr;
                }

                void Init(int samples) {
                    if (!cfg || samples != this->samples) {
                        Reset();
                        cfg = kiss_fftr_alloc(FFT_N, false, 0, 0);
                        deinterleaved = new float[samples];
                        scratch = new kiss_fft_cpx[(FFT_N / 2) + 1];
                        this->samples = samples;
                    }
                }

                int samples;
                kiss_fftr_cfg cfg;
                float* deinterleaved;
                kiss_fft_cpx* scratch;
            };
        }
    }
}

Player* Player::Create(
    const std::string &url,
    std::shared_ptr<IOutput> output,
    DestroyMode destroyMode,
    EventListener *listener,
    Gain gain)
{
    return new Player(url, output, destroyMode, listener, gain);
}

Player::Player(
    const std::string &url,
    std::shared_ptr<IOutput> output,
    DestroyMode destroyMode,
    EventListener *listener,
    Gain gain)
: state(Player::Idle)
, stream(Stream::Create())
, url(url)
, currentPosition(0)
, output(output)
, notifiedStarted(false)
, seekToPosition(-1)
, nextMixPoint(-1.0)
, pendingBufferCount(0)
, destroyMode(destroyMode)
, fftContext(nullptr)
, gain(gain) {
    musik::debug::info(TAG, "new instance created");

    this->spectrum = new float[FFT_N / 2];

    if (!this->output) {
        throw std::runtime_error("output cannot be null!");
    }

    if (listener) {
        listeners.push_back(listener);
    }

    /* each player instance is driven by a background thread. start it. */
    this->thread = new std::thread(std::bind(&musik::core::audio::playerThreadLoop, this));
}

Player::~Player() {
    delete[] this->spectrum;
    delete fftContext;
}

void Player::Play() {
    std::unique_lock<std::mutex> lock(this->queueMutex);

    if (this->state != Player::Quit) {
        this->state = Player::Playing;
        this->writeToOutputCondition.notify_all();
    }
}

void Player::Destroy() {
    {
        if (this->stream) {
            this->stream->Interrupt();
        }

        std::unique_lock<std::mutex> lock(this->queueMutex);

        if (this->state == Player::Quit && !this->thread) {
            return; /* already terminated (or terminating) */
        }

        this->state = Player::Quit;
        this->writeToOutputCondition.notify_all();
        this->thread->detach();
        delete this->thread;
        this->thread = nullptr;
    }
}

void Player::Destroy(DestroyMode mode) {
    this->destroyMode = mode;
    this->Destroy();
}

void Player::Detach(EventListener* listener) {
    if (listener) {
        std::unique_lock<std::mutex> lock(this->listenerMutex);
        this->listeners.remove_if([listener](EventListener *compare) {
            return (listener == compare);
        });
    }
}

void Player::Attach(EventListener* listener) {
    this->Detach(listener);

    if (listener) {
        std::unique_lock<std::mutex> lock(this->listenerMutex);
        this->listeners.push_back(listener);
    }
}

ListenerList Player::Listeners() {
    std::unique_lock<std::mutex> lock(this->listenerMutex);
    return ListenerList(this->listeners);
}

double Player::GetPosition() {
    double seek = this->seekToPosition.load();
    double current = this->currentPosition.load();
    const double latency = this->output ? this->output->Latency() : 0.0;
    return std::max(0.0, round((seek >= 0 ? seek : current) - latency));
}

double Player::GetPositionInternal() {
    const double latency = this->output ? this->output->Latency() : 0.0;
    return std::max(0.0, round(this->currentPosition.load() - latency));
}

double Player::GetDuration() {
    return this->stream ? this->stream->GetDuration() : -1.0f;
}

void Player::SetPosition(double seconds) {
    std::unique_lock<std::mutex> queueLock(this->queueMutex);

    if (this->stream) {
        auto duration = this->stream->GetDuration();
        if (duration > 0.0f) {
            seconds = std::min(duration, seconds);
        }
    }

    this->seekToPosition.store(std::max(0.0, seconds));

    /* reset our mix points on seek! that way we'll notify again if necessary */

    this->pendingMixPoints.splice(
        this->pendingMixPoints.begin(),
        this->processedMixPoints);

    this->UpdateNextMixPointTime();
}

void Player::AddMixPoint(int id, double time) {
    std::unique_lock<std::mutex> queueLock(this->queueMutex);
    this->pendingMixPoints.push_back(std::make_shared<MixPoint>(id, time));
    this->UpdateNextMixPointTime();
}

int Player::State() {
    std::unique_lock<std::mutex> lock(this->queueMutex);
    return this->state;
}

bool Player::HasCapability(Capability c) {
    if (this->stream) {
        return (this->stream->GetCapabilities() & (int) c) != 0;
    }
    return false;
}

void Player::UpdateNextMixPointTime() {
    const double position = this->GetPositionInternal();

    double next = -1.0;
    for (MixPointPtr mp : this->pendingMixPoints) {
        if (mp->time >= position) {
            if (mp->time < next || next == -1.0) {
                next = mp->time;
            }
        }
    }

    this->nextMixPoint = next;
}

void musik::core::audio::playerThreadLoop(Player* player) {
#ifdef WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    IBuffer* buffer = nullptr;

    float gain = player->gain.preamp * player->gain.gain;
    if (gain > 1.0f && player->gain.peakValid) {
        gain = player->gain.peak;
    }

    if (player->stream->OpenStream(player->url)) {
        for (Listener* l : player->Listeners()) {
            l->OnPlayerPrepared(player);
        }

        /* wait until we enter the Playing or Quit state */
        {
            std::unique_lock<std::mutex> lock(player->queueMutex);
            while (player->state == Player::Idle) {
                player->writeToOutputCondition.wait(lock);
            }
        }

        /* we're ready to go.... */
        bool finished = false;

        while (!finished && !player->Exited()) {
            /* see if we've been asked to seek since the last sample was
            played. if we have, clear our output buffer and seek the
            stream. */
            double seek = player->seekToPosition.load();

            if (seek != -1.0) {
                player->output->Stop(); /* flush all buffers */
                player->output->Resume(); /* start it back up */

                /* if we've allocated a buffer, but it hasn't been written
                to the output yet, unlock it. this is an important step, and if
                not performed, will result in a deadlock just below while
                waiting for all buffers to complete. */
                if (buffer) {
                    player->OnBufferProcessed(buffer);
                    buffer = nullptr;
                }

                player->currentPosition.store(seek);

                {
                    std::unique_lock<std::mutex> lock(player->queueMutex);
                    while (player->pendingBufferCount > 0) {
                        player->writeToOutputCondition.wait(lock);
                    }
                }

                player->stream->SetPosition(seek);
                player->seekToPosition.exchange(-1.0);
            }

            /* let's see if we can find some samples to play */
            if (!buffer) {
                std::unique_lock<std::mutex> lock(player->queueMutex);

                buffer = player->stream->GetNextProcessedOutputBuffer();

                if (buffer) {
                    /* apply replay gain, if specified */
                    if (gain != 1.0f) {
                        float* samples = buffer->BufferPointer();
                        for (long i = 0; i < buffer->Samples(); i++) {
                            samples[i] *= gain;
                        }
                    }

                    ++player->pendingBufferCount;
                }
            }

            /* if we have a decoded, processed buffer available. let's try to send it to
            the output device. */
            if (buffer) {
                /* if this result is negative it's an error code defined by the sdk's
                OutputPlay enum. if it's a positive number it's the number of milliseconds
                we should wait until automatically trying to play the buffer again. */
                int playResult = player->output->Play(buffer, player);

                if (playResult == OutputBufferWritten) {
                    buffer = nullptr; /* reset so we pick up a new one next iteration */
                }
                else {
                    /* if the buffer was unable to be processed, we'll try again after
                    sleepMs milliseconds */
                    int sleepMs = 1000; /* default */

                    /* if the playResult value >= 0, that means the output requested a
                    specific callback time because its internal buffer is full. */
                    if (playResult >= 0) {
                        /* if there is no visualizer active, we can introduce an
                        artifical delay of 25% of the buffer size to ease CPU load */
                        auto visualizer = vis::SelectedVisualizer();
                        if (!visualizer || !visualizer->Visible()) {
                            sleepMs = std::max(
                                (int)(player->output->Latency() * 250.0),
                                playResult);
                        }
                        else {
                            sleepMs = playResult;
                        }
                    }

                    std::unique_lock<std::mutex> lock(player->queueMutex);

                    player->writeToOutputCondition.wait_for(
                        lock, std::chrono::milliseconds(sleepMs));
                }
            }
            else {
                if (player->stream->Eof()) {
                    finished = true;
                }
                else {
                    /* we should never get here, but if for some reason we can't get the
                    next buffer, but we're not EOF, that means we need to wait for a short
                    while and try again... */
                    std::unique_lock<std::mutex> lock(player->queueMutex);
                    player->writeToOutputCondition.wait_for(
                        lock, std::chrono::milliseconds(10));
                }
            }
        }

        /* if the Quit flag isn't set, that means the stream has ended "naturally", i.e.
        it wasn't stopped by the user. raise the "almost ended" flag. */
        if (!player->Exited()) {
            for (Listener* l : player->Listeners()) {
                l->OnPlayerAlmostEnded(player);
            }
        }
    }

    /* if the stream failed to open... */
    else {
        if (!player->Exited()) {
            for (Listener* l : player->Listeners()) {
                l->OnPlayerError(player);
            }
        }
    }

    /* if non-null, it was never accepted by the output. release it now. */
    if (buffer) {
        player->OnBufferProcessed(buffer);
        buffer = nullptr;
    }

    /* wait until all remaining buffers have been written, set final state... */
    {
        std::unique_lock<std::mutex> lock(player->queueMutex);
        while (player->pendingBufferCount > 0) {
            player->writeToOutputCondition.wait(lock);
        }
    }

    /* buffers have been written, wait for the output to play them all */
    if (player->destroyMode == Player::Drain) {
        player->output->Drain();
    }

    if (!player->Exited()) {
        for (Listener* l : player->Listeners()) {
            l->OnPlayerFinished(player);
        }
    }

    player->state = Player::Quit;

    for (Listener* l : player->Listeners()) {
        l->OnPlayerDestroying(player);
    }

    player->Destroy();

    delete player;
}

bool Player::Exited() {
    std::unique_lock<std::mutex> lock(this->queueMutex);
    return (this->state == Player::Quit);
}

static inline void initHammingWindow() {
    delete hammingWindow;
    hammingWindow = new float[FFT_N];

    for (int i = 0; i < FFT_N; i++) {
        hammingWindow[i] = 0.54f - 0.46f * (float) cos((2 * PI * i) / (FFT_N - 1));
    }
}

static inline bool performFft(IBuffer* buffer, FftContext* fft, float* output, int outputSize) {
    long samples = buffer->Samples();
    int channels = buffer->Channels();
    long samplesPerChannel = samples / channels;

    if (samplesPerChannel < FFT_N || outputSize != FFT_N / 2) {
        return false;
    }

    if (!hammingWindow) {
        initHammingWindow();
    }

    memset(output, 0, outputSize * sizeof(float));

    float* input = buffer->BufferPointer();

    fft->Init(samples);

    for (int i = 0; i < samples; i++) {
        const int to = ((i % channels) * samplesPerChannel) + (i / channels);
        fft->deinterleaved[to] = input[i] * hammingWindow[to % FFT_N];
    }

    int offset = 0;
    int iterations = samples / FFT_N;
    for (int i = 0; i < iterations; i++) {
        kiss_fftr(fft->cfg, fft->deinterleaved + offset, fft->scratch);

        for (int z = 0; z < outputSize; z++) {
            /* convert to decibels */
            double db = (fft->scratch[z].r * fft->scratch[z].r + fft->scratch[z].i + fft->scratch[z].i);
            output[z] += (db < 1 ? 0 : 20 * (float) log10(db)) / iterations; /* frequencies over all channels */
        }

        offset += FFT_N;
    }

    return true;
}

void Player::OnBufferProcessed(IBuffer *buffer) {
    bool started = false;
    bool found = false;

    /* process visualizer data, and write to the selected plugin, if applicable */

    ISpectrumVisualizer* specVis = vis::SpectrumVisualizer();
    IPcmVisualizer* pcmVis = vis::PcmVisualizer();

    if (specVis && specVis->Visible()) {
        if (!fftContext) {
            fftContext = new FftContext();
        }

        if (performFft(buffer, fftContext, spectrum, FFT_N / 2)) {
            vis::SpectrumVisualizer()->Write(spectrum, FFT_N / 2);
        }
    }
    else if (pcmVis && pcmVis->Visible()) {
        vis::PcmVisualizer()->Write(buffer);
    }

    /* release the buffer back to the stream, find mixpoints */

    {
        std::unique_lock<std::mutex> lock(this->queueMutex);

        /* removes the specified buffer from the list of locked buffers, and also
        lets the stream know it can be recycled. */
        --pendingBufferCount;
        this->stream->OnBufferProcessedByPlayer((Buffer*)buffer);

        /* if we're seeking this value will be non-negative, so we shouldn't touch
        the current time. */
        if (this->seekToPosition.load() == -1) {
            this->currentPosition.store(((Buffer*)buffer)->Position());
        }

        /* did we hit any pending mixpoints? if so add them to our set and
        move them to the processed set. we'll notify once out of the
        critical section. */
        double adjustedPosition = this->GetPositionInternal();
        if (adjustedPosition >= this->nextMixPoint) {
            auto it = this->pendingMixPoints.begin();
            while (it != this->pendingMixPoints.end()) {
                if (adjustedPosition >= (*it)->time) {
                    this->mixPointsHitTemp.push_back(*it);
                    this->processedMixPoints.push_back(*it);
                    it = this->pendingMixPoints.erase(it);
                }
                else {
                    ++it;
                }
            }

            /* kind of awkward to recalc the next mixpoint here, but we
            already have the queue mutex acquired. */
            if (this->mixPointsHitTemp.size()) {
                this->UpdateNextMixPointTime();
            }
        }

        if (!this->notifiedStarted) {
            this->notifiedStarted = true;
            started = true;
        }
    }

    /* check up front so we don't have to acquire the mutex if
    we don't need to. */
    if (started || this->mixPointsHitTemp.size()) {
        ListenerList listeners = this->Listeners();
        if (!this->Exited() && listeners.size()) {
            for (Listener* l : ListenerList(listeners)) {
                /* we notify our listeners that we've started playing only after the first
                buffer has been consumed. this is because sometimes we precache buffers
                and send them to the output before they are actually processed by the
                output device */
                if (started) {
                    l->OnPlayerStarted(this);
                }

                for (MixPointPtr mp : this->mixPointsHitTemp) {
                    l->OnPlayerMixPoint(this, mp->id, mp->time);
                }
            }
        }

        this->mixPointsHitTemp.clear();
    }

    /* if the output device's internal buffers are full, it will stop
    accepting new samples. now that a buffer has been processed, we can
    try to enqueue another sample. the thread loop blocks on this condition */
    this->writeToOutputCondition.notify_all();
}
