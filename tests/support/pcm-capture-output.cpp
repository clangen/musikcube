#include "PcmCapture.h"

#include <musikcore/sdk/IDataStreamFactory.h>
#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/constants.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
    using namespace musik::core::sdk;
    using namespace playback_fixture;
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    constexpr auto period = 5ms;
    constexpr int framesPerPeriod = sampleRate * period.count() / 1000;
    constexpr std::size_t queueCapacity = 4;

    struct PendingBuffer {
        IBuffer* buffer;
        IBufferProvider* provider;
        long offset {};
    };

    struct Channel {
        std::deque<PendingBuffer> buffers;
        Clock::time_point availableAt {};
        double gain { 1.0 };
        bool playing {};
        bool started {};
    };

    class Capture {
    public:
        Capture() : clock([this](std::stop_token stop) { this->Consume(stop); }) {}
        ~Capture() {
            {
                std::lock_guard lock(this->mutex);
                this->clock.request_stop();
            }
            this->changed.notify_all();
        }

        std::size_t Read(CapturedFrame* destination, std::size_t capacity) {
            std::lock_guard lock(this->mutex);
            if (destination) {
                std::copy_n(this->frames.begin(), std::min(capacity, this->frames.size()), destination);
            }
            return this->frameCount;
        }

        void StoreFrames(bool enabled) {
            std::lock_guard lock(this->mutex);
            this->storeFrames = enabled;
        }

        std::size_t AudibleFrames() {
            std::lock_guard lock(this->mutex);
            return this->audibleFrames;
        }

        void HoldCompletion(bool hold) {
            std::lock_guard lock(this->mutex);
            this->holdCompletion = hold;
            this->changed.notify_all();
        }

        bool Wait(CaptureEvent event, unsigned timeoutMs) {
            std::unique_lock lock(this->mutex);
            if (event == CaptureEvent::Overlap) {
                this->watchOverlap = true;
            }
            return this->changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
                switch (event) {
                    case CaptureEvent::HeldCompletion: return this->heldCompletion;
                    case CaptureEvent::Stop: return this->interrupted;
                    case CaptureEvent::Resumed: return this->frameCount > this->framesAtStop;
                    case CaptureEvent::Balanced: return this->submitted == this->returned;
                    case CaptureEvent::Overlap: return this->overlap;
                    case CaptureEvent::SourceOpening: return this->sourceOpening;
                    case CaptureEvent::SourceInterrupted: return this->sourceInterrupted;
                }
                return false;
            });
        }

        void GateSourceOpen() {
            std::unique_lock lock(this->mutex);
            this->sourceOpening = true;
            this->changed.notify_all();
            this->changed.wait(lock, [&] { return this->releaseSource || this->sourceInterrupted; });
        }

        void ReleaseSourceOpen() {
            std::lock_guard lock(this->mutex);
            this->releaseSource = true;
            this->changed.notify_all();
        }

        void InterruptSource() {
            std::lock_guard lock(this->mutex);
            this->sourceInterrupted = true;
            this->changed.notify_all();
        }

        bool WaitFrames(std::size_t frames, unsigned timeoutMs) {
            std::unique_lock lock(this->mutex);
            return this->changed.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                [&] { return this->frameCount >= frames; });
        }

        void MarkStopped(bool started) {
            this->interrupted |= started;
            this->framesAtStop = this->frameCount;
        }

        void Return(const PendingBuffer& pending) {
            pending.provider->OnBufferProcessed(pending.buffer);
            std::lock_guard lock(this->mutex);
            ++this->returned;
            this->changed.notify_all();
        }

        std::size_t Outstanding() {
            std::lock_guard lock(this->mutex);
            return this->submitted - this->returned;
        }

        std::mutex mutex;
        std::condition_variable changed;
        std::vector<Channel*> channels;
        bool interrupted {};
        std::size_t submitted {};

    private:
        void Consume(std::stop_token stop) {
            std::unique_lock lock(this->mutex);
            while (!stop.stop_requested()) {
                this->changed.wait(lock, [&] {
                    return stop.stop_requested() || std::ranges::any_of(this->channels,
                        [](const auto* channel) { return channel->playing && !channel->buffers.empty(); });
                });
                if (stop.stop_requested()) {
                    break;
                }

                auto next = Clock::now();
                while (!stop.stop_requested() && this->Tick(lock, stop)) {
                    next = std::max(next + period, Clock::now());
                    this->changed.wait_until(lock, next, [&] { return stop.stop_requested(); });
                }
            }
        }

        bool Tick(std::unique_lock<std::mutex>& lock, std::stop_token stop) {
            std::array<CapturedFrame, framesPerPeriod> mixed {};
            std::vector<PendingBuffer> complete;
            bool active = false;
            bool waiting = false;
            std::size_t framesWritten = 0;
            const auto now = Clock::now();

            /* Consume all outputs on one clock so overlapping fades share a timeline. */
            for (auto* channel : this->channels) {
                if (!channel->playing) {
                    continue;
                }
                if (!channel->started) {
                    if (channel->buffers.empty()) {
                        continue;
                    }
                    if (now < channel->availableAt) {
                        waiting = true;
                        continue;
                    }
                    channel->started = true;
                }

                active = true;
                std::size_t i = 0;
                while (i != mixed.size() && !channel->buffers.empty()) {
                    auto& pending = channel->buffers.front();
                    const auto count = pending.buffer->Channels();
                    const auto* input = pending.buffer->BufferPointer();
                    auto& frame = mixed[i++];
                    const auto gain = static_cast<float>(channel->gain);

                    frame.left += gain * input[pending.offset];
                    frame.right += gain * input[pending.offset + count - 1];
                    pending.offset += count;
                    if (pending.offset == pending.buffer->Samples()) {
                        complete.push_back(pending);
                        channel->buffers.pop_front();
                    }
                }
                framesWritten = std::max(i, framesWritten);
            }

            if (active) {
                /* Preserve missing clock time so concatenation cannot hide an underrun. */
                mixed.front().interrupted = std::exchange(this->interrupted, false);
                for (std::size_t i = framesWritten; i < mixed.size(); ++i) {
                    mixed[i].underrun = true;
                }
                this->frameCount += mixed.size();
                this->audibleFrames += framesWritten;
                if (this->watchOverlap && !this->overlap) {
                    this->overlap = std::ranges::any_of(mixed,
                        [](const auto& frame) { return frame.left > 0.01f && frame.right > 0.01f; });
                }
                if (this->storeFrames) {
                    this->frames.insert(this->frames.end(), mixed.begin(), mixed.end());
                }
            }

            /* A completion callback can synchronously start or stop another output. */
            this->changed.notify_all();
            if (!complete.empty() && this->holdCompletion) {
                this->heldCompletion = true;
                this->changed.notify_all();
                this->changed.wait(lock, [&] { return !this->holdCompletion || stop.stop_requested(); });
                this->heldCompletion = false;
            }
            lock.unlock();
            for (const auto& pending : complete) {
                this->Return(pending);
            }
            lock.lock();
            return active || waiting;
        }

        std::vector<CapturedFrame> frames;
        std::size_t framesAtStop {};
        std::size_t frameCount {};
        std::size_t audibleFrames {};
        std::size_t returned {};
        bool storeFrames { true };
        bool watchOverlap {};
        bool overlap {};
        bool sourceOpening {};
        bool releaseSource {};
        bool sourceInterrupted {};
        bool holdCompletion {};
        bool heldCompletion {};
        std::jthread clock;
    };

    Capture& capture() {
        static Capture capture;
        return capture;
    }

    class GatedSource final : public IDataStream {
    public:
        /* block only after publication, so Interrupt can wake this open gate. */
        bool Open(const char*, OpenFlags) override {
            capture().GateSourceOpen();
            return true;
        }
        bool Close() override { return true; }
        void Interrupt() override { capture().InterruptSource(); }
        void Release() override { delete this; }
        bool Readable() override { return false; }
        bool Writable() override { return false; }
        PositionType Read(void*, PositionType) override { return 0; }
        PositionType Write(void*, PositionType) override { return 0; }
        bool SetPosition(PositionType) override { return false; }
        PositionType Position() override { return 0; }
        bool Seekable() override { return false; }
        bool Eof() override { return true; }
        long Length() override { return 0; }
        const char* Type() override { return ".wav"; }
        const char* Uri() override { return stalledSource; }
        bool CanPrefetch() override { return false; }
    };

    class GatedSourceFactory final : public IDataStreamFactory {
    public:
        bool CanRead(const char* uri) override { return std::string_view(uri) == stalledSource; }
        /* SDK 23 creation must return before any blocking provider work. */
        IDataStream* Create() override { return new GatedSource(); }
        void Release() override { delete this; }
    };

    class CaptureOutput final : public IOutput {
    public:
        CaptureOutput() {
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            sink.channels.push_back(&this->channel);
        }

        ~CaptureOutput() {
            this->Stop();
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            std::erase(sink.channels, &this->channel);
        }

        void Release() override { delete this; }

        void Pause() override {
            std::lock_guard lock(capture().mutex);
            this->channel.playing = false;
        }

        void Resume() override {
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            if (!this->channel.playing && !this->channel.started) {
                const char* delay = std::getenv("MUSIKCORE_TEST_OUTPUT_DELAY_MS");
                this->channel.availableAt = Clock::now() + std::chrono::milliseconds(delay ? std::atol(delay) : 0);
            }
            this->channel.playing = true;
            sink.changed.notify_all();
        }

        void SetVolume(double volume) override {
            std::lock_guard lock(capture().mutex);
            this->channel.gain = volume;
        }

        double GetVolume() override {
            std::lock_guard lock(capture().mutex);
            return this->channel.gain;
        }

        void Stop() override {
            auto& sink = capture();
            std::deque<PendingBuffer> discarded;
            {
                std::lock_guard lock(sink.mutex);
                sink.MarkStopped(this->channel.started);
                this->channel.playing = false;
                this->channel.started = false;
                discarded.swap(this->channel.buffers);
                sink.changed.notify_all();
            }

            for (const auto& pending : discarded) {
                sink.Return(pending);
            }
        }

        OutputState Play(IBuffer* buffer, IBufferProvider* provider) override {
            if (buffer->SampleRate() != sampleRate || buffer->Channels() < 1 || buffer->Channels() > 2 ||
                buffer->Samples() <= 0 || buffer->Samples() % buffer->Channels() != 0)
            {
                return OutputState::FormatError;
            }

            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            if (!this->channel.playing) {
                return OutputState::InvalidState;
            }
            if (this->channel.buffers.size() == queueCapacity) {
                return static_cast<OutputState>(period.count());
            }

            this->channel.buffers.push_back({ buffer, provider });
            ++sink.submitted;
            sink.changed.notify_all();
            return OutputState::BufferWritten;
        }

        void Drain() override {
            auto& sink = capture();
            std::unique_lock lock(sink.mutex);
            sink.changed.wait(lock, [&] { return this->channel.buffers.empty(); });
        }

        double Latency() override { return 0.0; }
        const char* Name() override { return driver; }
        int GetDefaultSampleRate() override { return sampleRate; }
        IDeviceList* GetDeviceList() override { return nullptr; }
        bool SetDefaultDevice(const char*) override { return false; }
        IDevice* GetDefaultDevice() override { return nullptr; }

    private:
        Channel channel;
    };

    class CapturePlugin final : public IPlugin {
    public:
        void Release() noexcept override { delete this; }
        const char* Name() override { return driver; }
        const char* Version() override { return "1"; }
        const char* Author() override { return "musikcube tests"; }
        const char* Guid() override { return "9c27689f-d977-4d5b-b079-5278bf1b8487"; }
        bool Configurable() override { return false; }
        void Configure() override {}
        void Reload() override {}
        int SdkVersion() override { return musik::core::sdk::SdkVersion; }
    };
}

extern "C" IPlugin* GetPlugin() {
    return new CapturePlugin();
}

extern "C" IOutput* GetAudioOutput() {
    return new CaptureOutput();
}

extern "C" IDataStreamFactory* GetDataStreamFactory() {
    return new GatedSourceFactory();
}

extern "C" void ReleasePcmSourceOpen() {
    capture().ReleaseSourceOpen();
}

extern "C" std::size_t ReadPcmCapture(CapturedFrame* destination, std::size_t capacity) {
    return capture().Read(destination, capacity);
}

extern "C" void HoldPcmCompletion(bool hold) {
    capture().HoldCompletion(hold);
}

extern "C" bool WaitPcmCapture(CaptureEvent event, unsigned timeoutMs) {
    return capture().Wait(event, timeoutMs);
}

extern "C" std::size_t OutstandingPcmCapture() {
    return capture().Outstanding();
}

extern "C" bool WaitPcmCaptureFrames(std::size_t frames, unsigned timeoutMs) {
    return capture().WaitFrames(frames, timeoutMs);
}

extern "C" void StorePcmCapture(bool enabled) {
    capture().StoreFrames(enabled);
}

extern "C" std::size_t AudiblePcmCapture() {
    return capture().AudibleFrames();
}
