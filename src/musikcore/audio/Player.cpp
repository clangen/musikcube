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
#include <musikcore/audio/PlaybackMath.h>
#include <musikcore/audio/Visualizer.h>
#include <musikcore/plugin/PluginFactory.h>
#include <musikcore/sdk/constants.h>

#include <algorithm>
#include <array>
#include <math.h>

#include <musikcore/visualizer.h>

#define FFT_N (2 * MC_VISUALIZER_SPECTRUM_BINS)
#define PI 3.14159265358979323846

using namespace musik::core::audio;
using namespace musik::core::sdk;

using std::min;
using std::max;

static std::string TAG = "Player";
namespace musik { namespace core { namespace audio {
    struct FftContext {
        FftContext() : cfg(kiss_fftr_alloc(FFT_N, false, nullptr, nullptr)) {}

        ~FftContext() { kiss_fftr_free(cfg); }

        kiss_fftr_cfg cfg;
        std::array<kiss_fft_scalar, FFT_N> input;
        std::array<kiss_fft_cpx, (FFT_N / 2) + 1> output;
        std::array<float, FFT_N / 2> spectrum;
    };
} } }

Player::Player(const std::string& url, std::shared_ptr<Output> output, Drain drain, EventListener& listener, Gain gain)
: output(std::move(output))
, stream(Stream::Create())
, writer(*this->output, *this)
, listener(listener)
, url(url)
, gain(gain)
, drain(drain)
, fftContext(std::make_unique<FftContext>())
{
}

Player::~Player() = default;

void Player::Play() {
    auto idle = State::Idle;
    this->state.compare_exchange_strong(idle, State::Playing);
    this->state.notify_one();
}

void Player::RequestStop(Drain drain) {
    this->drain.store(drain);
    if (drain == Drain::No) {
        this->writer.Revoke();
    }
    this->state.store(State::Quit);
    this->state.notify_one();
    this->output->NotifyWriters();
    this->stream->Interrupt();
}

double Player::GetPosition() const {
    const auto seek = this->seekToPosition.load();
    const auto position = seek >= 0 ? seek : this->currentPosition.load();
    return std::max(0.0, round(position - this->output->Latency()));
}

bool Player::SetPosition(double seconds) {
    const auto duration = this->GetDuration();
    if (duration > 0.0) {
        seconds = std::min(seconds, duration);
    }
    {
        std::lock_guard lock(this->mixPointMutex);
        if (!this->acceptingSeeks || this->Exited()) {
            return false;
        }
        this->seekToPosition.store(std::max(0.0, seconds));
        this->pendingMixPoints.splice(this->pendingMixPoints.begin(), this->processedMixPoints);
    }
    this->output->NotifyWriters();
    return true;
}

void Player::AddMixPoint(int id, double time) {
    std::lock_guard lock(this->mixPointMutex);
    this->pendingMixPoints.push_back({
        .id = id,
        .time = time,
    });
}

bool Player::HasCapability(Capability capability) const {
    return (this->capabilities.load() & static_cast<int>(capability)) != 0;
}

bool Player::WaitForBuffers(bool interruptForSeek) {
    for (;;) {
        this->writer.ProcessCompletions();
        if (interruptForSeek && !this->playbackFailed && !this->Exited() && this->HasPendingSeek()) {
            return false;
        }
        if (!this->writer.HasPending()) {
            return true;
        }
        this->writer.WaitForReturnedBuffer();
    }
}

void Player::Run() {
#ifdef WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

    if (this->stream->OpenStream(this->url, this->output.get())) {
        /* controls read snapshots while the worker may be blocked in the decoder. */
        this->duration.store(this->stream->GetDuration());
        this->capabilities.store(this->stream->GetCapabilities());
        if (!this->Exited()) {
            this->streamState = StreamState::Buffered;
            this->listener.OnPlayerBuffered(this);
        }
        this->state.wait(State::Idle);
        for (;;) {
            this->WriteTrack();
            if (!this->playbackFailed && !this->Exited() && !this->HasPendingSeek()) {
                this->streamState = StreamState::AlmostDone;
                this->listener.OnPlayerStreamEof(this);
            }

            /* decoded EOF can precede the audible tail. a seek still belongs
            to this track until its final decision closes admission below. */
            if (!this->WaitForBuffers(true)) {
                continue;
            }
            if (this->drain.load() == Drain::Yes) {
                const auto result = this->writer.Drain();
                if (result != MC_OK && result != MC_ERROR_CANCELLED) {
                    this->FailPlayback(result);
                }
            }
            {
                std::lock_guard lock(this->mixPointMutex);
                if (!this->playbackFailed && !this->Exited() && this->HasPendingSeek()) {
                    continue;
                }
                /* SetPosition uses the same lock: either the worker restarts
                for an accepted seek, or the caller observes closed admission. */
                this->acceptingSeeks = false;
            }
            break;
        }
    }
    else {
        {
            std::lock_guard lock(this->mixPointMutex);
            this->acceptingSeeks = false;
        }
        if (!this->Exited()) {
            this->streamState = StreamState::OpenFailed;
            this->listener.OnPlayerOpenFailed(this);
        }
    }

    if (!this->playbackFailed && !this->Exited() && this->streamState != StreamState::OpenFailed) {
        this->streamState = StreamState::Finished;
        this->listener.OnPlayerFinished(this);
    }
    this->state.store(State::Quit);
    this->streamState = StreamState::Stopped;
    this->listener.OnPlayerStopped(this);
}

void Player::WriteTrack() {
    const float gain = playbackGain(this->gain.preamp * this->gain.gain, this->gain.peak, this->gain.peakValid);
    IBuffer* buffer = nullptr;

    while (!this->Exited()) {
        this->writer.ProcessCompletions();
        if (this->playbackFailed || this->Exited()) {
            break;
        }

        double seek = this->seekToPosition.load();
        if (seek >= 0) {
            if (buffer) {
                this->ReturnUnusedBuffer(buffer);
                buffer = nullptr;
            }
            const auto reset = this->writer.ResetForSeek();
            if (reset != MC_OK) {
                this->FailPlayback(reset);
                break;
            }
            this->WaitForBuffers();
            if (this->Exited() || this->playbackFailed) {
                break;
            }
            const auto actual = this->stream->SetPosition(seek);
            if (actual >= 0) {
                this->currentPosition.store(actual);
                if (this->streamState == StreamState::AlmostDone && !this->Exited()) {
                    this->streamState = StreamState::Playing;
                    this->listener.OnPlayerStarted(this);
                }
            }
            /* a second seek can arrive while the first one flushes the device. */
            if (!this->seekToPosition.compare_exchange_strong(seek, -1.0)) {
                continue;
            }
        }

        if (!buffer) {
            buffer = this->stream->GetNextProcessedOutputBuffer();
            if (!buffer) {
                /* stop may have interrupted a blocked decode. leave accepted
                audio on the device when the caller requested a drain. */
                if (this->Exited()) {
                    break;
                }
                const auto status = this->stream->DecodeStatus();
                if (status != MC_OK) {
                    this->FailPlayback(status, "audio processing");
                    break;
                }
                if (this->stream->Eof()) {
                    break;
                }
                this->writer.WaitForReturnedBuffer();
                continue;
            }
            if (gain != 1.0f) {
                float* samples = buffer->BufferPointer();
                for (long i = 0; i < buffer->Samples(); ++i) {
                    samples[i] *= gain;
                }
            }
        }

        /* a stop or seek may have arrived during a network read. the writer
        also checks retirement under the device lock before admitting a block. */
        if (this->Exited() || this->seekToPosition.load() >= 0) {
            continue;
        }
        const auto result = this->writer.TryQueueAudioBuffer(*buffer);
        if (result == MC_OK) {
            buffer = nullptr;
        }
        else if (result == MC_ERROR_QUEUE_FULL) {
            this->writer.WaitForQueueSpace();
        }
        else {
            this->FailPlayback(result);
            break;
        }
    }

    if (buffer) {
        this->ReturnUnusedBuffer(buffer);
    }
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

void Player::ReturnUnusedBuffer(IBuffer* buffer) {
    this->stream->RecycleBuffer(buffer);
}

/* cancelled buffers return storage without advancing playback or showing
visualizer frames that the output discarded. this runs on the player worker. */
void Player::CompleteBuffer(IBuffer* buffer, mc_result outcome) {
    if (outcome == MC_OK) {
        this->OnBufferProcessed(buffer);
    }
    else {
        this->ReturnUnusedBuffer(buffer);
        if (outcome != MC_ERROR_CANCELLED) {
            this->FailPlayback(outcome);
        }
    }
}

/* a decoder or output failure ends playback once. stop returns any
remaining tokens before the worker waits, and prevents false EOF/auto-advance. */
void Player::FailPlayback(mc_result status, const char* operation) {
    if (this->playbackFailed) {
        return;
    }
    this->playbackFailed = true;
    const auto stopped = this->writer.StopAfterFailure();
    if (stopped != MC_ERROR_CANCELLED && !this->Exited()) {
        musik::debug::error(TAG, std::string(operation) + " failed (" + std::to_string(status) + ")");
        this->streamState = StreamState::OpenFailed;
        this->listener.OnPlayerOpenFailed(this);
    }
}

void Player::OnBufferProcessed(IBuffer *buffer) {
    std::list<MixPoint> mixPointsHit;

    /* process visualizer data, and write to the selected plugin, if applicable */

    auto* visualizer = vis::SelectedVisualizer();
    auto* specVis = dynamic_cast<ISpectrumVisualizer*>(visualizer);
    auto* pcmVis = dynamic_cast<IPcmVisualizer*>(visualizer);

    const bool renderSpectrum = specVis && specVis->Visible();
    const bool captureSpectrum = vis::SpectrumEnabled();
    if ((renderSpectrum || captureSpectrum) && performFft(buffer, this->fftContext.get(), this->fftContext->spectrum.data(), FFT_N / 2)) {
        if (renderSpectrum) {
            specVis->Write(this->fftContext->spectrum.data(), FFT_N / 2);
        }
        if (captureSpectrum) {
            vis::PublishSpectrum(this->fftContext->spectrum.data(), static_cast<uint32_t>(buffer->SampleRate()));
        }
    }
    if (pcmVis && pcmVis->Visible()) {
        pcmVis->Write(buffer);
    }

    const auto position = static_cast<Buffer*>(buffer)->Position();
    this->ReturnUnusedBuffer(buffer);
    const auto latency = this->output->Latency();
    {
        std::lock_guard lock(this->mixPointMutex);
        if (this->seekToPosition.load() < 0) {
            this->currentPosition.store(position);
            const auto audible = std::max(0.0, round(position - latency));
            auto point = this->pendingMixPoints.begin();
            while (point != this->pendingMixPoints.end()) {
                if (audible >= point->time) {
                    mixPointsHit.push_back(*point);
                    this->processedMixPoints.splice(this->processedMixPoints.end(), this->pendingMixPoints, point++);
                }
                else {
                    ++point;
                }
            }
        }
    }

    if (!this->Exited()) {
        if (!this->notifiedStarted) {
            this->streamState = StreamState::Playing;
            this->notifiedStarted = true;
            this->listener.OnPlayerStarted(this);
        }
        for (const auto& point : mixPointsHit) {
            this->listener.OnPlayerMixPoint(this, point.id, point.time);
        }
    }
}
