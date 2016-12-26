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

#include "pch.hpp"

#include <kiss_fftr.h>
#include <core/debug.h>
#include <core/audio/Stream.h>
#include <core/audio/Player.h>
#include <core/audio/Visualizer.h>
#include <core/plugin/PluginFactory.h>

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
    EventListener *listener)
{
    return new Player(url, output, listener);
}

Player::Player(
    const std::string &url,
    std::shared_ptr<IOutput> output,
    EventListener *listener)
: state(Player::Precache)
, url(url)
, currentPosition(0)
, output(output)
, notifiedStarted(false)
, setPosition(-1)
, fftContext(nullptr) {
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
    return std::max(0.0, round(this->currentPosition.load() - this->output->Latency()));
}

double Player::GetDuration() {
    return this->stream ? this->stream->GetDuration() : -1.0f;
}

void Player::SetPosition(double seconds) {
    this->setPosition.store(std::max(0.0, seconds));

    /* reset our mix points on seek! that way we'll notify again if necessary */
    std::unique_lock<std::mutex> queueLock(this->queueMutex);

    this->pendingMixPoints.splice(
        this->pendingMixPoints.begin(),
        this->processedMixPoints);
}

void Player::AddMixPoint(int id, double time) {
    std::unique_lock<std::mutex> queueLock(this->queueMutex);
    this->pendingMixPoints.push_back(std::make_shared<MixPoint>(id, time));
}

int Player::State() {
    std::unique_lock<std::mutex> lock(this->queueMutex);
    return this->state;
}

void musik::core::audio::playerThreadLoop(Player* player) {
    player->stream = Stream::Create();

    BufferPtr buffer;

    if (player->stream->OpenStream(player->url)) {
        for (Listener* l : player->Listeners()) {
            l->OnPlayerPrepared(player);
        }

        /* precache until buffers are full */
        bool keepPrecaching = true;
        while (player->State() == Player::Precache && keepPrecaching) {
            keepPrecaching = player->PreBuffer();
        }

        /* wait until we enter the Playing or Quit state; we may still
        be in the Precache state. */
        {
            std::unique_lock<std::mutex> lock(player->queueMutex);
            while (player->state == Player::Precache) {
                player->writeToOutputCondition.wait(lock);
            }
        }

        /* we're ready to go.... */
        bool finished = false;

        while (!finished && !player->Exited()) {
            /* see if we've been asked to seek since the last sample was
            played. if we have, clear our output buffer and seek the
            stream. */
            double position = player->setPosition.exchange(-1.0);

            if (position != -1) {
                player->output->Stop(); /* flush all buffers */
                player->output->Resume(); /* start it back up */

                /* if we've allocated a buffer, but it hasn't been written
                to the output yet, unlock it. this is an important step, and if
                not performed, will result in a deadlock just below while
                waiting for all buffers to complete. */
                if (buffer) {
                    player->OnBufferProcessed(buffer.get());
                    buffer.reset();
                }

                {
                    std::unique_lock<std::mutex> lock(player->queueMutex);
                    while (player->lockedBuffers.size() > 0) {
                        player->writeToOutputCondition.wait(lock);
                    }
                }

                player->stream->SetPosition(position);

                {
                    std::unique_lock<std::mutex> lock(player->queueMutex);
                    player->prebufferQueue.clear();
                }
            }

            /* let's see if we can find some samples to play */
            if (!buffer) {
                std::unique_lock<std::mutex> lock(player->queueMutex);

                /* the buffer queue may already have some available if it was prefetched. */
                if (!player->prebufferQueue.empty()) {
                    buffer = player->prebufferQueue.front();
                    player->prebufferQueue.pop_front();
                }
                /* otherwise, we need to grab a buffer from the stream and add it to the queue */
                else {
                    buffer = player->stream->GetNextProcessedOutputBuffer();
                }

                /* lock it down until it's processed */
                if (buffer) {
                    player->lockedBuffers.push_back(buffer);
                }
            }

            /* if we have a decoded, processed buffer available. let's try to send it to
            the output device. */
            if (buffer) {
                if (player->output->Play(buffer.get(), player)) {
                    /* success! the buffer was accepted by the output.*/
                    /* lock it down so it's not destroyed until the output device lets us
                    know it's done with it. */
                    std::unique_lock<std::mutex> lock(player->queueMutex);

                    if (player->lockedBuffers.size() == 1) {
                        player->currentPosition.store(buffer->Position());
                    }

                    buffer.reset(); /* important! we're done with this one locally. */
                }
                else {
                    /* the output device queue is probably full. we should block and wait until
                    the output lets us know that it needs more data. if we are starved for
                    more than a second, try to push the buffer into the output again. this
                    may happen if the sound driver has some sort of transient problem and
                    is temporarily unable to process the bufer (ALSA, i'm looking at you) */
                    std::unique_lock<std::mutex> lock(player->queueMutex);
                    player->writeToOutputCondition.wait_for(lock, std::chrono::milliseconds(1000));
                }
            }
            /* if we're unable to obtain a buffer, it means we're out of data and the
            player is finished. terminate the thread. */
            else {
                finished = true;
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

    /* unlock any remaining buffers... see comment above */
    if (buffer) {
        player->OnBufferProcessed(buffer.get());
        buffer.reset();
    }

    /* wait until all remaining buffers have been written, set final state... */
    {
        std::unique_lock<std::mutex> lock(player->queueMutex);
        while (player->lockedBuffers.size() > 0) {
            player->writeToOutputCondition.wait(lock);
        }
    }

    /* buffers have been written, wait for the output to play them all */
    player->output->Drain();

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

void Player::ReleaseAllBuffers() {
    std::unique_lock<std::mutex> lock(this->queueMutex);
    this->lockedBuffers.empty();
}

bool Player::PreBuffer() {
    /* don't prebuffer if the buffer is already full */
    if (this->prebufferQueue.size() < MAX_PREBUFFER_QUEUE_COUNT) {
        BufferPtr newBuffer = this->stream->GetNextProcessedOutputBuffer();

        if (newBuffer) {
            std::unique_lock<std::mutex> lock(this->queueMutex);
            this->prebufferQueue.push_back(newBuffer);
        }

        return true;
    }

    return false;
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

    MixPointList mixPointsHit;

    /* free the buffer */

    {
        std::unique_lock<std::mutex> lock(this->queueMutex);

        /* removes the specified buffer from the list of locked buffers, and also
        lets the stream know it can be recycled. */
        BufferList::iterator it = this->lockedBuffers.begin();
        while (it != this->lockedBuffers.end() && !found) {
            if (it->get() == buffer) {
                found = true;

                if (this->stream) {
                    this->stream->OnBufferProcessedByPlayer(*it);
                }

                bool isFront = this->lockedBuffers.front() == *it;
                it = this->lockedBuffers.erase(it);

                /* this sets the current time in the stream. it does this by grabbing
                the time at the next buffer in the queue */
                if (this->lockedBuffers.empty() || isFront) {
                    this->currentPosition.store(((Buffer*)buffer)->Position());
                }
                else {
                    /* if the queue is drained, use the position from the buffer
                    that was just processed */
                    this->currentPosition.store(this->lockedBuffers.front()->Position());
                }

                /* did we hit any pending mixpoints? if so add them to our set and
                move them to the processed set. we'll notify once out of the
                critical section. */
                if (this->pendingMixPoints.size() > 0) {
                    double adjustedPosition = this->GetPosition();

                    auto it = this->pendingMixPoints.begin();
                    while (it != this->pendingMixPoints.end()) {
                        if (adjustedPosition >= (*it)->time) {
                            mixPointsHit.push_back(*it);
                            this->processedMixPoints.push_back(*it);
                            it = this->pendingMixPoints.erase(it);
                        }
                        else {
                            ++it;
                        }
                    }
                }
            }
            else {
                ++it;
            }
        }

        if (!this->notifiedStarted) {
            this->notifiedStarted = true;
            started = true;
        }
    }

    /* check up front so we don't have to acquire the mutex if
    we don't need to. */
    if (started || mixPointsHit.size()) {
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

                auto it = mixPointsHit.begin();
                while (it != mixPointsHit.end()) {
                    l->OnPlayerMixPoint(this, (*it)->id, (*it)->time);
                    ++it;
                }
            }
        }
    }

    /* if the output device's internal buffers are full, it will stop
    accepting new samples. now that a buffer has been processed, we can
    try to enqueue another sample. the thread loop blocks on this condition */
    this->writeToOutputCondition.notify_all();
}
