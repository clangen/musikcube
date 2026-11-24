#include "PcmCapture.h"
#include <musikcore/abi.h>

#include <musikcore/plugin/stream.h>
#include <musikcore/plugin/output.hpp>
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

    struct Channel;

    /* retain C PCM and its token until the deterministic clock consumes it.
    completion publishes ownership; player work runs on its own worker. */
    struct PendingBuffer {
        mc_pcm_view pcm;
        mc_output_callbacks callbacks;
        uint64_t token;
        Channel* owner;
        size_t offset {};
    };

    struct Channel {
        std::deque<PendingBuffer> buffers;
        Clock::time_point availableAt {};
        double gain { 1.0 };
        bool playing {};
        bool started {};
        bool stopped { true };
        size_t callbacksInFlight {};
        size_t admitted {};
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

        void HoldCompletionUnlocked(bool hold) {
            this->holdCompletion = hold;
            this->changed.notify_all();
        }

        void HoldCompletion(bool hold) {
            std::lock_guard lock(this->mutex);
            this->HoldCompletionUnlocked(hold);
        }

        /* inject only the device control selected by the current test. */
        void SetControlFailure(ControlFailure failure) {
            std::lock_guard lock(this->mutex);
            this->controlFailure = failure;
        }

        bool Wait(CaptureEvent event, unsigned timeoutMs) {
            std::unique_lock lock(this->mutex);
            if (event == CaptureEvent::Overlap) {
                this->watchOverlap = true;
            }
            return this->changed.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
                switch (event) {
                    case CaptureEvent::Draining: return this->draining;
                    case CaptureEvent::HeldCompletion: return this->heldCompletion;
                    case CaptureEvent::Stop: return this->interrupted;
                    case CaptureEvent::Resumed: return this->frameCount > this->framesAtStop;
                    case CaptureEvent::Balanced: return this->submitted == this->returned;
                    case CaptureEvent::Overlap: return this->overlap;
                    case CaptureEvent::SourceOpening: return this->sourceOpening;
                    case CaptureEvent::SourceInterrupted: return this->sourceInterrupted;
                    case CaptureEvent::SecondPrepared: return this->secondPrepared;
                    case CaptureEvent::HeldRetiredQueue: return this->heldRetiredQueue;
                    case CaptureEvent::SourceReleased: return this->sourceReleased;
                    case CaptureEvent::OnlyOneOutputPaused:
                        return std::ranges::count_if(this->channels, [](const Channel* channel) {
                            return !channel->stopped;
                        }) == 1 && std::ranges::none_of(this->channels, [](const Channel* channel) {
                            return channel->playing && !channel->stopped;
                        });
                    case CaptureEvent::BothOutputsPlaying:
                        return std::ranges::count_if(this->channels, [](const Channel* channel) {
                            return channel->playing && !channel->stopped;
                        }) == 2;
                }
                return false;
            });
        }

        ControlFailure controlFailure = ControlFailure::None;

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

        void GateRetiredQueue() {
            std::lock_guard lock(this->mutex);
            this->gateRetiredQueue = true;
        }

        void ReleaseSource() {
            std::lock_guard lock(this->mutex);
            this->sourceReleased = true;
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
            mc_result outcome;
            {
                std::lock_guard lock(this->mutex);
                outcome = pending.owner->stopped ? MC_ERROR_CANCELLED : MC_OK;
            }
            if (outcome == MC_OK && std::getenv("MUSIKCORE_TEST_OUTPUT_COMPLETION_ERROR")) {
                outcome = MC_ERROR_IO;
            }
            pending.callbacks.completed(pending.callbacks.context, pending.token, outcome);
            if (pending.callbacks.ready) {
                pending.callbacks.ready(pending.callbacks.context);
            }
            std::lock_guard lock(this->mutex);
            --pending.owner->callbacksInFlight;
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
        bool secondPrepared {};
        bool draining {};
        bool gateRetiredQueue {};
        bool heldRetiredQueue {};
        bool sourceReleased {};

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
                    /* Keep the PCM clock on cadence across short scheduler delays.
                       Rebase after a stall so the fixture cannot issue an unbounded burst. */
                    next += period;
                    const auto now = Clock::now();
                    if (now - next > 4 * period) {
                        next = now;
                    }
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
                    const auto count = pending.pcm.format.channels;
                    const auto* input = pending.pcm.data;
                    auto& frame = mixed[i++];
                    const auto gain = static_cast<float>(channel->gain);

                    frame.left += gain * input[pending.offset];
                    frame.right += gain * input[pending.offset + count - 1];
                    pending.offset += count;
                    if (pending.offset == pending.pcm.frame_count * count) {
                        ++channel->callbacksInFlight;
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

    /* the URI chooses the cancellation boundary. one test holds open, while
    another lets the byte decoder open successfully and holds its first read. */
    const mc_stream_vtable gatedMethods {
        .struct_size = sizeof(mc_stream_vtable),
        .destroy = [](void*) { capture().ReleaseSource(); },
        .open = [](void*, mc_string_view uri, mc_stream_open_flags, mc_error**) -> mc_result {
            if (std::string_view(uri.data, uri.length) == stalledDecoderInput) {
                return MC_OK;
            }
            capture().GateSourceOpen();
            return MC_ERROR_CANCELLED;
        },
        .close = [](void*) -> mc_result { return MC_OK; },
        .interrupt = [](void*) { capture().InterruptSource(); },
        .get_info = [](void*, mc_stream_info* info) -> mc_result {
            *info = MC_STREAM_INFO_DEFAULT;
            info->flags = MC_STREAM_READABLE | MC_STREAM_LENGTH_KNOWN;
            info->length_bytes = 4;
            info->uri = { stalledDecoderInput, sizeof(stalledDecoderInput) - 1 };
            info->extension = { "bytes", 5 };
            return MC_OK;
        },
        .read = [](void*, void*, size_t, size_t* count, mc_read_state*, mc_error**) -> mc_result {
            *count = 0;
            capture().GateSourceOpen();
            return MC_ERROR_CANCELLED;
        },
        .write = [](void*, const void*, size_t, size_t*, mc_error**) -> mc_result { return MC_ERROR_UNSUPPORTED; },
        .seek = [](void*, uint64_t, mc_error**) -> mc_result { return MC_ERROR_UNSUPPORTED; },
        .tell = [](void*, uint64_t*) -> mc_result { return MC_ERROR_UNSUPPORTED; },
    };

    const mc_stream_factory gatedFactory {
        .struct_size = sizeof(mc_stream_factory),
        .context = nullptr,
        .instance_api = &gatedMethods,
        .can_handle = [](void*, const mc_media_hint* hint, uint32_t* supported) -> mc_result {
            const std::string_view uri(hint->uri.data, hint->uri.length);
            *supported = uri == stalledSource || uri == stalledDecoderInput;
            return MC_OK;
        },
        .create = [](void*, const mc_plugin_services*, void** instance, mc_error**) -> mc_result {
            *instance = &capture();
            return std::getenv("MUSIKCORE_TEST_EXPECT_ABORT") ? MC_ERROR_IO : MC_OK;
        },
    };

    class CaptureOutput final {
    public:
        CaptureOutput() {
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            sink.channels.push_back(&this->channel);
        }

        ~CaptureOutput() {
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            std::erase(sink.channels, &this->channel);
        }

        mc_result Prepare(const mc_output_options& options, const mc_output_callbacks& callbacks) {
            this->callbacks = callbacks;
            this->channel.stopped = false;
            this->channel.admitted = 0;
            this->SetVolume(options.initial_volume);
            const auto result = this->Resume();
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            if (sink.channels.size() > 1 && sink.channels.front() != &this->channel) {
                sink.secondPrepared = true;
                sink.changed.notify_all();
            }
            return result;
        }

        mc_result Pause() {
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            if (sink.controlFailure == ControlFailure::Pause ||
                (sink.controlFailure == ControlFailure::PauseOutgoing && sink.channels.front() == &this->channel))
            {
                return MC_ERROR_IO;
            }
            this->channel.playing = false;
            return MC_OK;
        }

        mc_result Resume() {
            auto& sink = capture();
            std::lock_guard lock(sink.mutex);
            if (sink.controlFailure == ControlFailure::Resume) {
                return MC_ERROR_IO;
            }
            if (!this->channel.playing && !this->channel.started) {
                const char* delay = std::getenv("MUSIKCORE_TEST_OUTPUT_DELAY_MS");
                this->channel.availableAt = Clock::now() + std::chrono::milliseconds(delay ? std::atol(delay) : 0);
            }
            this->channel.playing = true;
            sink.changed.notify_all();
            return MC_OK;
        }

        mc_result SetVolume(double volume) {
            std::lock_guard lock(capture().mutex);
            this->channel.gain = volume;
            return MC_OK;
        }

        double GetVolume() const {
            std::lock_guard lock(capture().mutex);
            return this->channel.gain;
        }

        mc_result Stop() noexcept {
            auto& sink = capture();
            std::deque<PendingBuffer> discarded;
            {
                std::lock_guard lock(sink.mutex);
                sink.MarkStopped(this->channel.started);
                this->channel.playing = false;
                this->channel.started = false;
                this->channel.stopped = true;
                discarded.swap(this->channel.buffers);
                this->channel.callbacksInFlight += discarded.size();
                /* Stop must interrupt the fixture's held completion gate too;
                no callback may outlive this instance. */
                if (this->channel.callbacksInFlight) {
                    sink.HoldCompletionUnlocked(false);
                }
                sink.changed.notify_all();
            }
            for (const auto& pending : discarded) {
                sink.Return(pending);
            }
            std::unique_lock lock(sink.mutex);
            sink.changed.wait(lock, [&] { return !this->channel.callbacksInFlight; });
            return MC_OK;
        }

        mc_result QueueAudioBuffer(const mc_output_buffer& block) noexcept {
            if (block.pcm.format.sample_rate != sampleRate || block.pcm.format.channels > 2) {
                return MC_ERROR_UNSUPPORTED;
            }
            auto& sink = capture();
            std::unique_lock lock(sink.mutex);
            if (sink.gateRetiredQueue && sink.channels.front() == &this->channel) {
                /* the old output is no longer active, but its producer still
                owns Output's ordinary-call lock until Stop interrupts it. */
                sink.heldRetiredQueue = true;
                sink.changed.notify_all();
                sink.changed.wait(lock, [&] { return this->channel.stopped; });
            }
            if (this->channel.stopped) {
                return MC_ERROR_CANCELLED;
            }
            if (this->channel.admitted && std::getenv("MUSIKCORE_TEST_OUTPUT_QUEUE_ERROR")) {
                return MC_ERROR_IO;
            }
            if (this->channel.buffers.size() == queueCapacity) {
                return MC_ERROR_QUEUE_FULL;
            }
            this->channel.buffers.push_back({ block.pcm, this->callbacks, block.token, &this->channel });
            ++sink.submitted;
            ++this->channel.admitted;
            sink.changed.notify_all();
            return MC_OK;
        }

        mc_result Drain() {
            auto& sink = capture();
            std::unique_lock lock(sink.mutex);
            sink.draining = true;
            sink.changed.notify_all();
            if (!this->channel.playing && !this->channel.buffers.empty()) {
                return MC_ERROR_IN_FLIGHT;
            }
            sink.changed.wait(lock, [&] {
                return this->channel.stopped || (this->channel.buffers.empty() && !this->channel.callbacksInFlight);
            });
            return this->channel.stopped ? MC_ERROR_CANCELLED : MC_OK;
        }

        static mc_result VisitDevices(mc_output_device_fn, void*) { return MC_OK; }
        static mc_result ReadDefaultDevice(mc_output_device_fn, void*) { return MC_ERROR_NOT_FOUND; }
        static mc_result GetDefaultFormat(mc_string_view, mc_audio_format& format) {
            format = { sizeof(format), sampleRate, 2, 0 };
            return MC_OK;
        }

    private:
        Channel channel;
        mc_output_callbacks callbacks = MC_OUTPUT_CALLBACKS_DEFAULT;
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        const mc_plugin_capability capability {
            .struct_size = sizeof(mc_plugin_capability),
            .id = { MC_PLUGIN_CAPABILITY_STREAM, sizeof(MC_PLUGIN_CAPABILITY_STREAM) - 1 },
            .api = &gatedFactory,
            .api_size = sizeof(gatedFactory),
        };
        const auto status = host->register_plugin_capability(host->host_context, &capability);
        return status == MC_OK ? musik::core::plugin::RegisterOutput<CaptureOutput>(*host) : status;
    }
    void MC_CALL shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "capture.output", 14 };
    plugin->name = { driver, sizeof(driver) - 1 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}

extern "C" MC_PLUGIN_API void ReleasePcmSourceOpen() {
    capture().ReleaseSourceOpen();
}

extern "C" MC_PLUGIN_API void SetPcmControlFailure(ControlFailure failure) {
    capture().SetControlFailure(failure);
}

extern "C" MC_PLUGIN_API void GateRetiredPcmQueue() {
    capture().GateRetiredQueue();
}

extern "C" MC_PLUGIN_API std::size_t ReadPcmCapture(CapturedFrame* destination, std::size_t capacity) {
    return capture().Read(destination, capacity);
}

extern "C" MC_PLUGIN_API void HoldPcmCompletion(bool hold) {
    capture().HoldCompletion(hold);
}

extern "C" MC_PLUGIN_API bool WaitPcmCapture(CaptureEvent event, unsigned timeoutMs) {
    return capture().Wait(event, timeoutMs);
}

extern "C" MC_PLUGIN_API std::size_t OutstandingPcmCapture() {
    return capture().Outstanding();
}

extern "C" MC_PLUGIN_API bool WaitPcmCaptureFrames(std::size_t frames, unsigned timeoutMs) {
    return capture().WaitFrames(frames, timeoutMs);
}

extern "C" MC_PLUGIN_API void StorePcmCapture(bool enabled) {
    capture().StoreFrames(enabled);
}

extern "C" MC_PLUGIN_API std::size_t AudiblePcmCapture() {
    return capture().AudibleFrames();
}
