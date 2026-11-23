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

#pragma once

#include <musikcore/audio/IStream.h>
#include <musikcore/audio/Output.h>
#include <musikcore/sdk/constants.h>

#include <atomic>
#include <list>
#include <memory>
#include <mutex>

namespace musik { namespace core { namespace audio {

    struct FftContext;

    /* decodes one track and sends its samples to an output. a URI player or
    catalog transport can select another track while this one returns its last
    buffers to the decoder. */
    class Player : private Output::CompletionSink {
    public:
        using Shared = std::shared_ptr<Player>;

        enum class Drain { No, Yes };

        struct Gain {
            float preamp = 1.0f;
            float gain = 1.0f;
            float peak = 1.0f;
            bool peakValid = false;
        };

        /* identify the sending track so the listener can ignore late events
        from a track it has already replaced. */
        struct EventListener {
            virtual ~EventListener() = default;
            virtual void OnPlayerBuffered(Player*) {}
            virtual void OnPlayerStarted(Player*) {}
            virtual void OnPlayerStreamEof(Player*) {}
            virtual void OnPlayerFinished(Player*) {}
            virtual void OnPlayerOpenFailed(Player*) {}
            virtual void OnPlayerStopped(Player*) {}
            virtual void OnPlayerMixPoint(Player*, int, double) {}
        };

        ~Player();

        void Play();
        void RequestStop(Drain drain = Drain::No);

        double GetPosition() const;
        /* reject once the worker has committed its terminal decision. */
        bool SetPosition(double seconds);
        bool HasPendingSeek() const { return this->seekToPosition.load() >= 0; }
        double GetDuration() const { return this->duration.load(); }

        void AddMixPoint(int id, double time);
        bool HasCapability(sdk::Capability capability) const;

        const std::string& GetUrl() const { return this->url; }
        sdk::StreamState GetStreamState() const { return this->streamState.load(); }

    private:
        friend class PlaybackWorkers;

        enum class State { Idle, Playing, Quit };

        struct MixPoint {
            int id;
            double time;
        };

        Player(const std::string& url, std::shared_ptr<Output> output, Drain drain, EventListener& listener, Gain gain);

        void Run();
        void WriteTrack();
        bool WaitForBuffers(bool interruptForSeek = false);
        void ReturnUnusedBuffer(sdk::IBuffer* buffer);
        void CompleteBuffer(sdk::IBuffer* buffer, mc_result outcome) override;
        void OnBufferProcessed(sdk::IBuffer* buffer);
        void FailOutput(mc_result status);
        bool Exited() const { return this->state.load() == State::Quit; }

        const std::shared_ptr<Output> output;
        const IStreamPtr stream;
        Output::Writer writer;
        EventListener& listener;
        const std::string url;
        const Gain gain;

        std::atomic<State> state { State::Idle };
        std::atomic<sdk::StreamState> streamState { sdk::StreamState::Buffering };
        std::atomic<Drain> drain;
        std::atomic<double> duration { -1.0 };
        std::atomic<int> capabilities { 0 };
        std::atomic<double> currentPosition { 0.0 };
        std::atomic<double> seekToPosition { -1.0 };

        std::mutex mixPointMutex;
        bool acceptingSeeks = true;
        std::list<MixPoint> pendingMixPoints;
        std::list<MixPoint> processedMixPoints;

        /* only Run and the completions it dispatches touch these fields. */
        bool notifiedStarted = false;
        bool outputFailed = false;
        std::unique_ptr<FftContext> fftContext;
    };

} } }
