#pragma once

#include <musikcore/plugin/output.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>

namespace musik { namespace plugins {

    /* estimates audible playback from backend-supplied presentation deadlines.
    this is an optional implementation helper for `get_timing()`, not another
    timing interface: readers still receive the standard mc_output_timing.
    backends with a native played-frame counter need not use this helper.

    frames count only accepted PCM, excluding underrun silence. a deadline is
    the host monotonic time when the last scheduled frame becomes audible;
    it already includes device latency. reads interpolate toward that deadline,
    cap progress at scheduled PCM, and mark the result as estimated. a changed
    deadline never rewinds frames already reported as heard. if lead time grows while earlier
    PCM is still in flight, the estimate may hold that tail until it catches up;
    it does not retain a separate timeline for every silence gap.

    example: accept 480 frames at 48 kHz, with their audible end 100 ms away:
        estimator.Reset(48000);
        const auto endFrame = estimator.Submit(480);
        estimator.Publish(endFrame, OutputTimingEstimator::Now() + 100'000'000);
        estimator.Read(timing);

    how an output backend uses this helper:
    - during prepare, call `Reset()` with the new sample rate after the old
      device and its callbacks have stopped.
    - when accepting PCM, call `Submit()` before making the samples visible to
      the device callback. call `Reject()` if admission fails instead.
    - once the backend knows when scheduled PCM will be heard, call `Publish()`
      with the cumulative scheduled frame count and its audible end time. this
      may happen while queueing, or in a callback after copying PCM to the
      device. count only the scheduled portion of a partially consumed buffer.
    - implement `get_timing()` by calling `Read()`. it estimates the current audible
      frame count from that deadline and reports how much accepted PCM remains.

    queue calls must not overlap each other, and scheduling callbacks must not
    overlap each other. queueing and a scheduling callback can run together.
    do not reset the estimator until both have stopped: an old callback could
    otherwise attach its frames to the new sample rate and playback generation.

    if the device freezes its pending PCM on pause, stop scheduling callbacks
    before calling `Pause()`. call `Resume()` before restarting them. this keeps
    a callback from replacing the deadline while it is being shifted for pause.
    a backend that keeps playing its pending PCM and then emits silence does
    not freeze this estimator when paused.

    `Read()` is safe while those operations run. if a deadline changes during
    the read, it returns MC_ERROR_BUSY instead of mixing old and new values.
    the consumer should keep its current picture and try again, not treat the
    busy snapshot as a device without timing information. */
    class OutputTimingEstimator {
    public:
        using TimeSource = uint64_t() noexcept;

        /* the default host clock is used by plugins. a bounded, allocation-free
        replacement lets tests exercise deadlines without sleeping. */
        explicit OutputTimingEstimator(TimeSource& readTime = Now)
        : readTime(readTime) {}

        /* host monotonic nanoseconds, shared by deadlines and snapshot timestamps. */
        static uint64_t Now() noexcept {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count();
        }

        /* start a new generation at zero frames; rate is frames/second. pass
        zero after shutdown to make subsequent snapshots unknown. */
        void Reset(uint32_t rate) noexcept {
            if constexpr (!Supported) {
                return;
            }
            this->version.fetch_add(1);
            this->rate.store(rate);
            this->generation.fetch_add(1);
            this->submitted.store(0);
            this->scheduled.store(0);
            this->audibleFloor.store(0);
            this->deadline.store(0);
            this->frozen.store(0);
            this->version.fetch_add(1);
        }

        /* reserve accepted PCM before making it visible to the scheduling writer.
        returns its exclusive end-frame index. undo failed admission with `Reject()`. */
        uint64_t Submit(size_t frames) noexcept {
            if constexpr (!Supported) {
                return 0;
            }
            return this->submitted.fetch_add(frames) + frames;
        }

        /* undo the producer's latest failed admission; those frames were never scheduled. */
        void Reject(size_t frames) noexcept {
            if constexpr (Supported) {
                this->submitted.fetch_sub(frames);
            }
        }

        /* associate an exclusive cumulative frame count with its audible end time.
        publish after scheduling PCM; never include trailing silence in frames. */
        void Publish(uint64_t frames, uint64_t audibleEnd) noexcept {
            if constexpr (!Supported) {
                return;
            }
            this->version.fetch_add(1);
            const auto now = this->readTime();
            const auto frozen = this->frozen.load();
            const auto observationTime = frozen ? frozen : now;
            /* a later buffer can have a fresh DAC lead after an underrun.
            preserve the previous audible estimate before replacing its deadline;
            silence between runs must not turn old PCM back into pending PCM. */
            const auto audible = std::max(this->audibleFloor.load(),
                PlayedAt(this->rate.load(), this->scheduled.load(), this->deadline.load(), observationTime));
            this->audibleFloor.store(audible);
            if (frozen) {
                audibleEnd -= std::min(audibleEnd, now - frozen);
            }
            this->scheduled.store(frames);
            this->deadline.store(audibleEnd);
            this->version.fetch_add(1);
        }

        /* freeze a device timeline that stops consuming scheduled PCM. do not
        call this when a backend pauses by generating silence but still lets
        its previously scheduled PCM reach the device. repeated calls are safe. */
        void Pause() noexcept {
            if constexpr (!Supported) {
                return;
            }
            this->version.fetch_add(1);
            uint64_t running = 0;
            this->frozen.compare_exchange_strong(running, this->readTime());
            this->version.fetch_add(1);
        }
        /* shift the pending deadline by the paused duration, then unfreeze it. */
        void Resume() noexcept {
            if constexpr (!Supported) {
                return;
            }
            const auto frozen = this->frozen.load();
            if (frozen) {
                this->version.fetch_add(1);
                this->deadline.fetch_add(this->readTime() - frozen);
                this->frozen.store(0);
                this->version.fetch_add(1);
            }
        }

        /* snapshot estimated audible progress and all still-pending PCM in seconds.
        device latency is already in the deadline, so it is not reported again. */
        mc_result Read(mc_output_timing& timing) const noexcept {
            timing = MC_OUTPUT_TIMING_DEFAULT;
            if constexpr (!Supported) {
                return MC_OK;
            }
            const auto version = this->version.load();
            if (version & 1) {
                return MC_ERROR_BUSY;
            }
            const auto rate = this->rate.load();
            const auto generation = this->generation.load();
            const auto submitted = this->submitted.load();
            const auto scheduled = this->scheduled.load();
            const auto audibleFloor = this->audibleFloor.load();
            const auto deadline = this->deadline.load();
            const auto frozen = this->frozen.load();
            const auto now = this->readTime();
            const auto observationTime = frozen ? frozen : now;
            if (version != this->version.load()) {
                return MC_ERROR_BUSY;
            }
            if (!rate) {
                return MC_OK;
            }
            const auto played =
                std::min(submitted, std::max(audibleFloor, PlayedAt(rate, scheduled, deadline, observationTime)));
            timing.valid_flags = MC_OUTPUT_TIMING_PLAYED | MC_OUTPUT_TIMING_QUEUED | MC_OUTPUT_TIMING_ESTIMATED;
            timing.generation = generation;
            timing.frames_played_by_driver = played;
            timing.frames_played_timestamp_ns = now;
            timing.queued_seconds = static_cast<double>(submitted - played) / rate;
            return MC_OK;
        }

    private:
        static uint64_t PlayedAt(
            uint32_t rate, uint64_t scheduled, uint64_t deadline, uint64_t observationTime) noexcept
        {
            const double remaining = deadline > observationTime ? (deadline - observationTime) * 1e-9 * rate : 0.0;
            const auto pending = static_cast<uint64_t>(std::min<double>(scheduled, std::ceil(remaining)));
            return scheduled - pending;
        }

        /* optional timing must stay allocation-free and lock-free, including
        targets whose standard library implements 64-bit atomics with a mutex. */
        static constexpr bool Supported = std::atomic<uint64_t>::is_always_lock_free;

        TimeSource& readTime;
        std::atomic<uint64_t> version { 0 };
        std::atomic<uint32_t> rate { 0 };
        std::atomic<uint64_t> generation { 0 };
        std::atomic<uint64_t> submitted { 0 };
        std::atomic<uint64_t> scheduled { 0 };
        std::atomic<uint64_t> audibleFloor { 0 };
        std::atomic<uint64_t> deadline { 0 };
        std::atomic<uint64_t> frozen { 0 };
    };

} }
