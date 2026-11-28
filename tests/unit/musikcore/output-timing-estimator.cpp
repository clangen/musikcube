#include <plugins/common/OutputTimingEstimator.h>

#include <cstdio>
#include <cstdlib>

#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::fprintf(stderr, "output timing estimator test failed at line %d: %s\n", __LINE__, #condition);        \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (false)

namespace {
    uint64_t now = 1'000'000'000;
    musik::plugins::OutputTimingEstimator* resetOnRead = nullptr;

    uint64_t clockTime() noexcept {
        if (resetOnRead) {
            auto* estimator = resetOnRead;
            resetOnRead = nullptr;
            estimator->Reset(48000);
        }
        return now;
    }

    mc_output_timing read(const musik::plugins::OutputTimingEstimator& estimator) {
        auto timing = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
        CHECK(estimator.Read(timing) == MC_OK);
        return timing;
    }
}

int main() {
    musik::plugins::OutputTimingEstimator estimator(clockTime);
    estimator.Reset(48000);
    estimator.Submit(4800);
    estimator.Publish(4800, now + 100'000'000);
    now += 110'000'000;
    const auto first = read(estimator);
    CHECK(first.frames_played_by_driver == 4800);
    CHECK(first.queued_seconds == 0.0);

    /* a fresh DAC lead after an underrun must not make old PCM pending again. */
    estimator.Submit(480);
    estimator.Publish(5280, now + 110'000'000);
    const auto gap = read(estimator);
    CHECK(gap.generation == first.generation);
    CHECK(gap.frames_played_by_driver == 4800);
    CHECK(gap.queued_seconds == 0.01);
    now += 100'000'000;
    CHECK(read(estimator).frames_played_by_driver == 4800);
    now += 5'000'000;
    CHECK(read(estimator).frames_played_by_driver == 5040);
    now += 5'000'000;
    CHECK(read(estimator).frames_played_by_driver == 5280);
    CHECK(read(estimator).queued_seconds == 0.0);

    /* changing the lead time while a tail remains is conservative: the old
    audible floor is retained until the revised estimate catches up. */
    estimator.Submit(4800);
    estimator.Publish(10080, now + 100'000'000);
    now += 50'000'000;
    const auto beforeDelay = read(estimator);
    CHECK(beforeDelay.frames_played_by_driver == 7680);
    estimator.Submit(480);
    estimator.Publish(10560, now + 160'000'000);
    const auto delayed = read(estimator);
    CHECK(delayed.frames_played_by_driver == beforeDelay.frames_played_by_driver);
    CHECK(delayed.queued_seconds == 0.06);
    now += 200'000'000;
    CHECK(read(estimator).frames_played_by_driver == 10560);

    estimator.Submit(4800);
    estimator.Publish(15360, now + 100'000'000);
    now += 50'000'000;
    estimator.Pause();
    const auto paused = read(estimator);
    now += 1'000'000'000;
    CHECK(read(estimator).frames_played_by_driver == paused.frames_played_by_driver);
    estimator.Resume();
    CHECK(read(estimator).frames_played_by_driver == paused.frames_played_by_driver);
    now += 50'000'000;
    CHECK(read(estimator).frames_played_by_driver == 15360);

    estimator.Reset(48000);
    CHECK(read(estimator).generation != first.generation);
    CHECK(read(estimator).frames_played_by_driver == 0);
    estimator.Submit(480);
    estimator.Reject(480);
    CHECK(read(estimator).queued_seconds == 0.0);
    /* reset between the field loads and final version check. a reader must
    retry instead of publishing fields combined from two estimator generations. */
    resetOnRead = &estimator;
    auto contested = mc_output_timing MC_OUTPUT_TIMING_DEFAULT;
    CHECK(estimator.Read(contested) == MC_ERROR_BUSY);
    CHECK(contested.valid_flags == 0);
    /* the cursor must carry into the high word even on a 32-bit target. */
    constexpr size_t half = 0x8000'0000;
    estimator.Submit(half);
    const auto end = estimator.Submit(half + 1);
    CHECK(end == 0x1'0000'0001);
    estimator.Reject(1);
    estimator.Publish(end - 1, now);
    CHECK(read(estimator).frames_played_by_driver == end - 1);
    estimator.Reset(0);
    CHECK(read(estimator).valid_flags == 0);
}
