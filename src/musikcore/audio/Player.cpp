//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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
#include <musikcore/debug.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/audio/Player.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/sdk/constants.h>

#include <algorithm>
#include <array>
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
static std::mutex playerWorkersMutex;
static std::condition_variable playerWorkersChanged;
static size_t playerWorkers = 0;

void Player::WaitForWorkers() {
    std::unique_lock<std::mutex> lock(playerWorkersMutex);
    playerWorkersChanged.wait(lock, [] { return playerWorkers == 0; });
}

using Listener = Player::EventListener;
using ListenerList = std::list<Listener*>;

namespace musik {
    namespace core {
        namespace audio {
            void playerThreadLoop(Player* player);

            struct FftContext {
                FftContext()
                : cfg(kiss_fftr_alloc(FFT_N, false, nullptr, nullptr)) {
                }

                ~FftContext() {
                    kiss_fftr_free(cfg);
                }

                kiss_fftr_cfg cfg;
                std::array<kiss_fft_scalar, FFT_N> input;
                std::array<kiss_fft_cpx, (FFT_N / 2) + 1> output;
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
: internalState(Player::Idle)
, streamState(StreamState::Buffering)
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
    std::lock_guard<std::mutex> lock(playerWorkersMutex);
    this->thread = new std::thread([this] {
        /* Wait until construction has published the worker and counted it. */
        {
            std::lock_guard<std::mutex> lock(playerWorkersMutex);
        }
        musik::core::audio::playerThreadLoop(this);
    });
    ++playerWorkers;
}

Player::~Player() {
    delete[] this->spectrum;
    delete fftContext;
}

void Player::Play() {
    std::unique_lock<std::mutex> lock(this->queueMutex);

    if (this->internalState != Player::Quit) {
        this->internalState = Player::Playing;
        this->writeToOutputCondition.notify_all();
    }
}

void Player::Destroy() {
    {
        if (this->stream) {
            this->stream->Interrupt();
        }

        std::unique_lock<std::mutex> lock(this->queueMutex);

        if (this->internalState == Player::Quit && !this->thread) {
            return; /* already terminated (or terminating) */
        }

        this->internalState = Player::Quit;
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
    return this->internalState;
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

    if (player->stream->OpenStream(player->url, player->output.get())) {
        for (Listener* l : player->Listeners()) {
            player->streamState = StreamState::Buffered;
            l->OnPlayerBuffered(player);
        }

        /* wait until we enter the Playing or Quit state */
        {
            std::unique_lock<std::mutex> lock(player->queueMutex);
            while (player->internalState == Player::Idle) {
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
                OutputState playResult = player->output->Play(buffer, player);

                if (playResult == OutputState::BufferWritten) {
                    buffer = nullptr; /* reset so we pick up a new one next iteration */
                }
                else {
                    /* if the buffer was unable to be processed, we'll try again after
                    sleepMs milliseconds */
                    int sleepMs = 1000; /* default */

                    /* if the playResult value >= 0, that means the output requested a
                    specific callback time because its internal buffer is full. */
                    if ((int) playResult >= 0) {
                        /* if there is no visualizer active, we can introduce an
                        artificial delay of 25% of the buffer size to ease CPU load */
                        auto visualizer = vis::SelectedVisualizer();
                        if (!visualizer || !visualizer->Visible()) {
                            sleepMs = std::max(
                                (int)(player->output->Latency() * 250.0),
                                (int) playResult);
                        }
                        else {
                            sleepMs = (int) playResult;
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
                player->streamState = StreamState::AlmostDone;
                l->OnPlayerStreamEof(player);
            }
        }
    }

    /* if the stream failed to open... */
    else {
        if (!player->Exited()) {
            for (Listener* l : player->Listeners()) {
                player->streamState = StreamState::OpenFailed;
                l->OnPlayerOpenFailed(player);
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
    if (player->destroyMode == Player::DestroyMode::Drain) {
        player->output->Drain();
    }

    if (!player->Exited()) {
        for (Listener* l : player->Listeners()) {
            player->streamState = StreamState::Finished;
            l->OnPlayerFinished(player);
        }
    }

    player->internalState = Player::Quit;

    for (Listener* l : player->Listeners()) {
        player->streamState = StreamState::Stopped;
        l->OnPlayerDestroying(player);
    }

    player->Destroy();

    delete player;

    /* Stream and output plugin references are released as part of player destruction. */
    std::lock_guard<std::mutex> lock(playerWorkersMutex);
    --playerWorkers;
    playerWorkersChanged.notify_all();
}

bool Player::Exited() {
    std::unique_lock<std::mutex> lock(this->queueMutex);
    return (this->internalState == Player::Quit);
}

struct HammingWindow {
    std::array<float, FFT_N> values;
    double sum;
};

static const HammingWindow& getHammingWindow() {
    static const HammingWindow window = []() {
        HammingWindow result = {};
        for (int i = 0; i < FFT_N; i++) {
            result.values[i] =
                0.54f - 0.46f * (float) cos((2 * PI * i) / (FFT_N - 1));
            result.sum += result.values[i];
        }
        return result;
    }();

    return window;
}

static inline bool performFft(IBuffer* buffer, FftContext* fft, float* output, int outputSize) {
    if (!buffer || !fft || !fft->cfg || !output || outputSize != FFT_N / 2) {
        return false;
    }

    long samples = buffer->Samples();
    int channels = buffer->Channels();
    if (channels <= 0 || samples <= 0 || !buffer->BufferPointer()) {
        return false;
    }

    const long samplesPerChannel = samples / channels;
    const long windowsPerChannel = samplesPerChannel / FFT_N;
    if (windowsPerChannel == 0) {
        return false;
    }

    memset(output, 0, outputSize * sizeof(float));

    float* input = buffer->BufferPointer();
    const HammingWindow& window = getHammingWindow();
    std::array<double, FFT_N / 2> accumulatedPower = {};

    for (int channel = 0; channel < channels; channel++) {
        for (long block = 0; block < windowsPerChannel; block++) {
            const long firstFrame = block * FFT_N;
            for (int i = 0; i < FFT_N; i++) {
                const long source = ((firstFrame + i) * channels) + channel;
                fft->input[i] = input[source] * window.values[i];
            }

            kiss_fftr(fft->cfg, fft->input.data(), fft->output.data());

            for (int bin = 0; bin < outputSize; bin++) {
                const double real = fft->output[bin].r;
                const double imaginary = fft->output[bin].i;
                accumulatedPower[bin] += (real * real) + (imaginary * imaginary);
            }
        }
    }

    /*
     * Convert average power to a single-sided, window-normalized amplitude.
     * The visualizer API historically expects positive bar heights, so expose
     * decibels above an -80 dBFS display floor: 0 is at/below the floor and 80
     * represents 0 dBFS. DC is not doubled; the omitted Nyquist bin would use
     * the same treatment.
     */
    constexpr double floorDb = -80.0;
    const double iterations = (double)(channels * windowsPerChannel);
    for (int bin = 0; bin < outputSize; bin++) {
        const double averagePower = accumulatedPower[bin] / iterations;
        const double singleSidedScale = (bin == 0) ? 1.0 : 2.0;
        const double amplitude = singleSidedScale * sqrt(averagePower) / window.sum;

        if (amplitude > 0.0) {
            const double dbfs = 20.0 * log10(amplitude);
            output[bin] = (float) std::clamp(dbfs - floorDb, 0.0, -floorDb);
        }
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
            this->streamState = StreamState::Playing;
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
