#include "PcmCapture.h"

#include <musikcore/sdk/IOutput.h>
#include <musikcore/sdk/constants.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <mutex>
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
            return this->frames.size();
        }

        std::mutex mutex;
        std::condition_variable changed;
        std::vector<Channel*> channels;
        bool interrupted {};

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
                while (!stop.stop_requested() && this->Tick(lock)) {
                    next = std::max(next + period, Clock::now());
                    this->changed.wait_until(lock, next, [&] { return stop.stop_requested(); });
                }
            }
        }

        bool Tick(std::unique_lock<std::mutex>& lock) {
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
                this->frames.insert(this->frames.end(), mixed.begin(), mixed.end());
            }

            /* A completion callback can synchronously start or stop another output. */
            this->changed.notify_all();
            lock.unlock();
            for (const auto& pending : complete) {
                pending.provider->OnBufferProcessed(pending.buffer);
            }
            lock.lock();
            return active || waiting;
        }

        std::vector<CapturedFrame> frames;
        std::jthread clock;
    };

    Capture& capture() {
        static Capture capture;
        return capture;
    }

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
                sink.interrupted |= this->channel.started;
                this->channel.playing = false;
                this->channel.started = false;
                discarded.swap(this->channel.buffers);
                sink.changed.notify_all();
            }

            for (const auto& pending : discarded) {
                pending.provider->OnBufferProcessed(pending.buffer);
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

extern "C" std::size_t ReadPcmCapture(CapturedFrame* destination, std::size_t capacity) {
    return capture().Read(destination, capacity);
}
