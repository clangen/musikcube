#include "Output.h"
#include <common/Contract.h>
#include <cmath>
#include <limits>
#include <algorithm>
#include <climits>
#include <chrono>

namespace musik { namespace core { namespace audio {
    namespace {
        std::atomic<size_t> activeInstances { 0 };
        constexpr size_t QueueBufferCount = 8;
        constexpr size_t MinimumQueueFrames = 4096;
        constexpr auto ControlWait = std::chrono::milliseconds(100);

        /* buffers can contain a short final block or a new track's format.
        read their properties once; only build the C view after checking that
        the claimed samples fit in storage and contain complete audio frames. */
        mc_result readPcmView(const sdk::IBuffer& buffer, mc_pcm_view& pcm) {
            const auto rate = buffer.SampleRate();
            const auto channels = buffer.Channels();
            if (rate <= 0 || static_cast<uint64_t>(rate) > UINT32_MAX || channels <= 0) {
                return MC_ERROR_INVALID_ARGUMENT;
            }

            const auto samples = buffer.Samples();
            const auto bytes = buffer.Bytes();
            const auto data = buffer.BufferPointer();
            if (!data || samples <= 0 || bytes <= 0 || samples % channels != 0) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            if (static_cast<uint64_t>(samples) > static_cast<uint64_t>(bytes) / sizeof(float)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }

            pcm = {
                .struct_size = sizeof(mc_pcm_view),
                .data = data,
                .frame_count = static_cast<size_t>(samples / channels),
                .format = {
                    .struct_size = sizeof(mc_audio_format),
                    .sample_rate = static_cast<uint32_t>(rate),
                    .channels = static_cast<uint32_t>(channels),
                    .channel_layout = 0,
                },
            };

            return MC_OK;
        }

        mc_string_view text(const std::string& value) {
            return {
                .data = value.data(),
                .length = value.size(),
            };
        }

        /* copy callback-scoped device text before the factory returns. */
        void MC_CALL collectDevice(void* context, const mc_output_device* device) {
            auto& list = *static_cast<std::vector<Output::Device>*>(context);
            list.push_back({
                .id = std::string(device->id.data ? device->id.data : "", device->id.length),
                .name = std::string(device->name.data ? device->name.data : "", device->name.length),
            });
        }
    }

    Output::Output(
        ffi::PluginRegistry::Factory<mc_output_factory> factory, std::string id, std::string name, std::string device)
    : factory(std::move(factory))
    , id(std::move(id))
    , name(std::move(name))
    , device(std::move(device))
    {
    }

    Output::Writer::Writer(Output& output, CompletionSink& sink)
    : output(output)
    , sink(sink)
    , lastWakeSeen(output.wakeCount.load())
    , lastFailureSeen(output.completionFailures.load())
    {
    }

    Output::Writer::~Writer() {
        musik::contract::Assert(!this->HasPending(), "output writer destroyed before recycling completed buffers");
    }

    void Output::Writer::WaitForBufferChange() noexcept {
        /* remember notifications per writer so a callback between a failed
        attempt and this wait is seen by every track sharing the device. */
        this->output.wakeCount.wait(this->lastWakeSeen);
        this->lastWakeSeen = this->output.wakeCount.load();
    }

    bool Output::Writer::PrioritizeFailedCompletion() noexcept {
        const auto failures = this->output.completionFailures.load();
        if (failures == this->lastFailureSeen) {
            return false;
        }

        /* a failed driver can hold older buffers until the player stops it.
        move only the failed buffer past them, preserving audio order for
        progress and visualization. successful callbacks never need this scan. */
        for (size_t i = 1; i < this->pendingCount; ++i) {
            auto index = (this->firstPending + i) % CompletionCapacity;
            const auto slot = this->pendingSlots[index];
            const auto& completion = this->output.completions[slot];
            if (completion.token.load(std::memory_order_acquire) == Completion::Returned &&
                completion.outcome != MC_OK && completion.outcome != MC_ERROR_CANCELLED)
            {
                while (index != this->firstPending) {
                    const auto previous = (index + CompletionCapacity - 1) % CompletionCapacity;
                    this->pendingSlots[index] = this->pendingSlots[previous];
                    index = previous;
                }
                this->pendingSlots[this->firstPending] = slot;
                return true;
            }
        }
        this->lastFailureSeen = failures;
        return false;
    }

    void Output::Writer::ProcessCompletions() {
        while (this->HasPending()) {
            auto& completion = this->output.completions[this->pendingSlots[this->firstPending]];
            if (completion.token.load(std::memory_order_acquire) != Completion::Returned) {
                if (this->PrioritizeFailedCompletion()) {
                    continue;
                }
                break;
            }

            auto* buffer = completion.buffer;
            const auto outcome = completion.outcome;
            this->firstPending = (this->firstPending + 1) % CompletionCapacity;
            --this->pendingCount;
            completion.token.store(Completion::Empty, std::memory_order_release);
            this->output.NotifyWriters();
            /* the worker owns error policy. a failed sink can stop its writer
            without a second stop hidden in completion collection. */
            this->sink.CompleteBuffer(buffer, outcome);
        }
    }

    bool Output::Writer::HasPending() const noexcept {
        return this->pendingCount != 0;
    }

    mc_result Output::Writer::ResetForSeek() {
        std::lock_guard ordinary(this->output.ordinary);
        std::lock_guard lifecycle(this->output.lifecycle);
        if (this->revoked.load() || this->output.stopped.load()) {
            return MC_ERROR_CANCELLED;
        }
        /* keep reset and reopening one operation. an obsolete seek must never
        flush a replacement track or reopen admission after transport Stop. */
        this->output.StopBackend();
        this->output.paused = false;
        this->output.NotifyWriters();
        return MC_OK;
    }

    mc_result Output::Writer::Drain() {
        std::lock_guard ordinary(this->output.ordinary);
        if (this->revoked.load() || this->output.stopped.load()) {
            return MC_ERROR_CANCELLED;
        }
        if (!this->output.prepared.load()) {
            return MC_OK;
        }
        const auto result = this->output.factory.Invoke([&](const auto&) {
            return this->output.api->drain(this->output.instance, nullptr);
        });
        return this->revoked.load() ? MC_ERROR_CANCELLED : result;
    }

    mc_result Output::Writer::StopAfterFailure() {
        /* ordinary may be held by a blocked write that needs this stop. the
        lifecycle lock orders revocation checks with reset and replacement. */
        std::lock_guard lifecycle(this->output.lifecycle);
        if (this->revoked.load()) {
            return MC_ERROR_CANCELLED;
        }
        this->output.stopped.store(true);
        this->output.StopBackend();
        this->output.NotifyWriters();
        return MC_OK;
    }

    void Output::Writer::Revoke() noexcept {
        this->revoked.store(true);
        this->output.NotifyWriters();
    }

    Output::~Output() {
        this->Stop();
        /* Stop interrupts a blocking queue/drain before joining ordinary calls. */
        std::lock_guard lock(this->ordinary);
        for (const auto& completion : this->completions) {
            musik::contract::Assert(
                completion.token.load() == Completion::Empty, "output destroyed before players recycled completed buffers");
        }
        if (this->instance) {
            this->factory.Invoke([&](const auto&) {
                this->api->destroy(this->instance);
                activeInstances.fetch_sub(1);
                return MC_OK;
            });
        }
    }

    size_t Output::InstanceCount() noexcept {
        return activeInstances.load();
    }

    std::vector<Output::Device> Output::Devices() const {
        std::vector<Device> devices;
        this->factory.Invoke([&](const auto& factory) {
            return factory.visit_devices(factory.context, collectDevice, &devices);
        });
        return devices;
    }

    std::string Output::PinnedDeviceId() const {
        std::lock_guard lock(this->ordinary);
        return this->device;
    }

    mc_string_view Output::PlaybackDeviceId() const {
        if (!this->device.empty()) {
            for (const auto& candidate : this->Devices()) {
                if (candidate.id == this->device) {
                    return text(this->device);
                }
            }
        }
        /* a saved device can be unplugged between tracks. follow the system
        default until it returns, keeping the user's pin for next time. */
        return {};
    }

    bool Output::SetDefaultDevice(const char* device) {
        const std::string requested = device ? device : "";
        if (!requested.empty()) {
            const auto devices = this->Devices();
            if (std::none_of(devices.begin(), devices.end(),
                    [&](const auto& item) {
                        return item.id == requested;
                    }))
            {
                return false;
            }
        }
        std::lock_guard lock(this->ordinary);
        if (this->instance) {
            return false;
        }
        this->device = requested;
        return true;
    }

    int Output::GetDefaultSampleRate() const {
        std::lock_guard lock(this->ordinary);
        const auto deviceId = this->PlaybackDeviceId();
        mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
        const auto result = this->factory.Invoke([&](const auto& factory) {
            return factory.get_default_format(factory.context, deviceId, &format);
        });
        return result == MC_OK && format.sample_rate <= INT_MAX ? static_cast<int>(format.sample_rate) : -1;
    }

    void Output::StopBackend() {
        if (this->instance) {
            const auto result = this->factory.Invoke([&](const auto&) {
                return this->api->stop(this->instance);
            });
            musik::contract::Assert(result == MC_OK, "output Stop failed; callbacks and ownership are not safe");
        }
        this->prepared.store(false);
    }

    void Output::Stop() {
        this->stopped.store(true);
        std::lock_guard lock(this->lifecycle);
        /* a Resume already holding lifecycle may have overwritten the early
        admission closure while Stop waited. the final ordering belongs here. */
        this->stopped.store(true);
        this->StopBackend();
        this->NotifyWriters();
    }

    std::unique_lock<std::timed_mutex> Output::LockForControl() {
        /* give a normally paced block time to finish and give controls the
        next turn. a device can also hold a write indefinitely until Stop. */
        std::unique_lock lock(this->ordinary, std::try_to_lock);
        if (lock.owns_lock()) {
            return lock;
        }
        this->waitingControls.fetch_add(1);
        lock.try_lock_for(ControlWait);
        this->waitingControls.fetch_sub(1);
        this->NotifyWriters();
        return lock;
    }

    mc_result Output::SetPaused(bool pause) {
        auto ordinary = this->LockForControl();
        if (!ordinary.owns_lock()) {
            return MC_ERROR_IN_FLIGHT;
        }
        std::lock_guard lifecycle(this->lifecycle);
        if (this->prepared.load() && this->paused != pause) {
            const auto result = this->factory.Invoke([&](const auto&) {
                return pause ? this->api->pause(this->instance) : this->api->resume(this->instance);
            });
            if (result != MC_OK) {
                return result;
            }
        }
        this->paused = pause;
        if (!pause) {
            this->stopped.store(false);
        }
        this->NotifyWriters();
        return MC_OK;
    }

    mc_result Output::Pause() {
        return this->SetPaused(true);
    }

    mc_result Output::Resume() {
        return this->SetPaused(false);
    }

    mc_result Output::SetVolume(double value) {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        this->volume.store(value);
        std::unique_lock ordinary(this->ordinary, std::try_to_lock);
        if (!ordinary.owns_lock()) {
            return MC_ERROR_IN_FLIGHT;
        }
        std::unique_lock lifecycle(this->lifecycle, std::try_to_lock);
        return lifecycle.owns_lock() ? this->ApplyVolume() : MC_ERROR_IN_FLIGHT;
    }

    mc_result Output::ApplyVolume() {
        const auto value = this->volume.load();
        if (this->deviceVolume == value) {
            return MC_OK;
        }
        if (!this->prepared.load()) {
            return MC_OK;
        }
        const auto result = this->factory.Invoke([&](const auto&) {
            return this->api->set_volume(this->instance, value);
        });
        if (result == MC_OK) {
            this->deviceVolume = value;
        }
        return result;
    }

    double Output::GetVolume() const {
        return this->volume.load();
    }

    mc_result Output::Prepare(const mc_pcm_view& pcm, const std::atomic_bool& revoked) {
        /* the next track may need a different sample rate or larger buffers.
        keep the current setup when it fits, so gapless playback stays seamless;
        otherwise finish the queued audio before replacing that setup. */
        if (this->prepared.load() && this->format.sample_rate == pcm.format.sample_rate &&
            this->format.channels == pcm.format.channels && pcm.frame_count <= this->capacity)
        {
            return MC_OK;
        }

        /* capacity arithmetic belongs to setup, not every queued block. */
        if (pcm.frame_count > std::numeric_limits<size_t>::max() / QueueBufferCount) {
            return MC_ERROR_INVALID_ARGUMENT;
        }

        if (this->prepared.load()) {
            const auto result = this->factory.Invoke([&](const auto&) {
                return this->api->drain(this->instance, nullptr);
            });
            if (result != MC_OK) {
                return result;
            }
        }

        /* taking this lock before drain would prevent Stop from interrupting
        the wait. shutdown may have happened while we were waiting. */
        std::lock_guard lifecycle(this->lifecycle);
        if (this->stopped.load() || revoked.load()) {
            return MC_ERROR_CANCELLED;
        }

        /* create the plugin object when audio first arrives, so listing outputs
        does not open devices or start workers. keep it for later tracks. */
        if (!this->instance) {
            const auto result = this->factory.Invoke([&](const auto& factory) {
                this->api = factory.instance_api;
                return factory.create(factory.context, nullptr, &this->instance, nullptr);
            });
            if (this->instance) {
                activeInstances.fetch_add(1);
            }
            if (result != MC_OK) {
                /* a factory can start workers before reporting failure. stop
                them before freeing the object they might still be using. */
                this->StopBackend();
                if (this->instance) {
                    this->factory.Invoke([&](const auto&) {
                        this->api->destroy(this->instance);
                        return MC_OK;
                    });
                    activeInstances.fetch_sub(1);
                    this->instance = nullptr;
                }
                return result;
            }
            musik::contract::Assert(this->instance != nullptr, "output factory succeeded without an instance");
        }

        /* reuse the plugin object, but release its old device resources before
        asking it to open a different audio setup. */
        this->StopBackend();

        mc_output_options options = MC_OUTPUT_OPTIONS_DEFAULT;
        options.format = pcm.format;
        options.device_id = this->PlaybackDeviceId();
        options.initial_volume = this->volume.load();
        this->capacity = std::max<size_t>(pcm.frame_count * QueueBufferCount, MinimumQueueFrames);
        options.queue_capacity_frames = this->capacity;
        const mc_output_callbacks callbacks {
            .struct_size = sizeof(mc_output_callbacks),
            .context = this,
            .completed = Completed,
            .ready = Ready,
        };
        const auto result = this->factory.Invoke([&](const auto&) {
            return this->api->prepare(this->instance, &options, &callbacks, nullptr);
        });

        if (result != MC_OK) {
            /* prepare can open the device before a later step fails. close
            anything it opened rather than leaving it running after an error. */
            this->StopBackend();
            return result;
        }

        this->deviceVolume = options.initial_volume;
        this->format = pcm.format;
        this->prepared.store(true);
        return MC_OK;
    }

    mc_result Output::Writer::TryQueueAudioBuffer(sdk::IBuffer& buffer) {
        mc_pcm_view pcm = MC_PCM_VIEW_DEFAULT;
        const auto valid = readPcmView(buffer, pcm);
        if (valid != MC_OK) {
            return valid;
        }

        if (output.waitingControls.load()) {
            return MC_ERROR_QUEUE_FULL;
        }
        std::lock_guard ordinary(output.ordinary);
        if (output.stopped.load() || this->revoked.load()) {
            return MC_ERROR_CANCELLED;
        }
        if (output.paused) {
            return MC_ERROR_QUEUE_FULL;
        }
        const auto ready = output.Prepare(pcm, this->revoked);
        if (ready != MC_OK) {
            return ready;
        }
        if (this->revoked.load() || output.stopped.load()) {
            return MC_ERROR_CANCELLED;
        }
        /* a volume request can arrive during a blocked write. apply the most
        recent setting before sending more audio. */
        if (output.volume.load() != output.deviceVolume) {
            std::lock_guard lifecycle(output.lifecycle);
            const auto gain = output.ApplyVolume();
            if (gain != MC_OK) {
                return gain;
            }
        }
        for (size_t i = 0; i < output.completions.size(); ++i) {
            auto& completion = output.completions[i];
            if (completion.token.load(std::memory_order_acquire) != Completion::Empty) {
                continue;
            }
            musik::contract::Assert(
                output.nextToken < UINT64_MAX / CompletionCapacity, "output token identity exhausted");
            const auto token = output.nextToken++ * CompletionCapacity + i;
            completion.buffer = &buffer;
            completion.token.store(token, std::memory_order_release);
            const mc_output_buffer block {
                .struct_size = sizeof(mc_output_buffer),
                .token = token,
                .pcm = pcm,
            };
            const auto result = output.factory.Invoke([&](const auto&) {
                return output.api->queue_audio_buffer(output.instance, &block);
            });
            if (result == MC_OK) {
                const auto tail = (this->firstPending + this->pendingCount) % CompletionCapacity;
                this->pendingSlots[tail] = static_cast<uint8_t>(i);
                ++this->pendingCount;
            }
            else {
                auto pending = token;
                const bool unclaimed = completion.token.compare_exchange_strong(pending, Completion::Empty);
                musik::contract::Assert(unclaimed,
                    "output completed a buffer whose admission failed");
            }
            return result;
        }
        return MC_ERROR_QUEUE_FULL;
    }

    double Output::Latency() const {
        if (!this->prepared.load()) {
            return 0.0;
        }
        mc_output_timing timing = MC_OUTPUT_TIMING_DEFAULT;
        const auto status = this->factory.Invoke([&](const auto&) {
            return this->api->get_timing(this->instance, &timing);
        });
        if (status != MC_OK) {
            return 0.0;
        }
        /* timing is optional. combine only valid bounded measurements;
        unknown values must not invent a device clock or poison progress. */
        double latency = 0.0;
        if ((timing.valid_flags & MC_OUTPUT_TIMING_QUEUED) && std::isfinite(timing.queued_seconds) &&
            timing.queued_seconds >= 0)
        {
            latency += timing.queued_seconds;
        }
        if ((timing.valid_flags & MC_OUTPUT_TIMING_DEVICE_LATENCY) && std::isfinite(timing.device_latency_seconds) &&
            timing.device_latency_seconds >= 0)
        {
            latency += timing.device_latency_seconds;
        }
        return std::isfinite(latency) ? latency : 0.0;
    }

    void MC_CALL Output::Completed(void* state, uint64_t token, mc_result outcome) noexcept {
        auto& output = *static_cast<Output*>(state);
        auto& completion = output.completions[token % CompletionCapacity];
        /* claim this exact submission before touching its result. an old
        callback must never return storage that a later track is using. */
        musik::contract::Assert(token >= CompletionCapacity, "invalid output completion token");
        const bool claimed = completion.token.compare_exchange_strong(token, Completion::Returning);
        musik::contract::Assert(claimed, "output returned an unknown or already completed buffer");
        completion.outcome = outcome;
        completion.token.store(Completion::Returned, std::memory_order_release);
        if (outcome != MC_OK && outcome != MC_ERROR_CANCELLED) {
            output.completionFailures.fetch_add(1);
        }
        output.NotifyWriters();
    }

    void MC_CALL Output::Ready(void* state) noexcept {
        static_cast<Output*>(state)->NotifyWriters();
    }

    void Output::NotifyWriters() noexcept {
        this->wakeCount.fetch_add(1);
        this->wakeCount.notify_all();
    }

} } }
