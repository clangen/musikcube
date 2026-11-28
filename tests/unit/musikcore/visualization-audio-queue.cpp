#include <musikcore/audio/VisualizationAudioQueue.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <new>
#include <vector>

using musik::core::audio::VisualizationAudioQueue;
using namespace std::chrono_literals;

namespace {
    /* tests run one queue at a time; each queue joins before its consumer is
    replaced. production uses the process visualizer service instead. */
    std::function<void(const mc_pcm_view&)> deliver;
    std::atomic<bool> consumerEnabled { true };
    std::atomic<unsigned> captureFailures { 0 };
    std::atomic<mc_result> lastCaptureFailure { MC_OK };
}
namespace musik { namespace core { namespace audio { namespace vis {
    bool HasAudioConsumers() {
        return consumerEnabled.load();
    }
    void DeliverAudio(const mc_pcm_view& pcm) {
        deliver(pcm);
    }
    void ReportCaptureFailure(mc_result result) noexcept {
        lastCaptureFailure.store(result);
        captureFailures.fetch_add(1);
    }
} } } }


#define CHECK(condition)                                                                                               \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::fprintf(stderr, "visualization test failed at line %d: %s\n", __LINE__, #condition);                  \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (false)

/* fail exactly one allocation on the submitting thread. the visualization
worker and synchronization gates keep using the ordinary allocator. */
static thread_local bool failNextAllocation = false;

void* operator new(std::size_t count) {
    if (failNextAllocation) {
        failNextAllocation = false;
        throw std::bad_alloc();
    }
    if (void* memory = std::malloc(count ? count : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

/* reset waits for the current plugin call, then discards the remaining history. */
static void checkResetDuringDelivery() {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool release = false;
    std::atomic<bool> resetStarted { false };
    std::atomic<bool> resetFinished { false };
    deliver = [&](const mc_pcm_view&) {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        CHECK(changed.wait_for(lock, 2s, [&] {
            return release;
        }));
        CHECK(!resetFinished.load());
    };
    VisualizationAudioQueue visualization([] {
        return std::optional<uint64_t>(512);
    });
    float samples[1024] {};
    auto pcm = mc_pcm_view MC_PCM_VIEW_DEFAULT;
    pcm.data = samples;
    pcm.frame_count = 1024;
    pcm.format.sample_rate = 48000;
    pcm.format.channels = 1;
    visualization.Enqueue(pcm, 0);
    std::unique_lock lock(mutex);
    CHECK(changed.wait_for(lock, 2s, [&] {
        return entered;
    }));
    std::thread reset([&] {
        resetStarted.store(true);
        changed.notify_all();
        visualization.Reset();
        resetFinished.store(true);
        changed.notify_all();
    });
    CHECK(changed.wait_for(lock, 2s, [&] {
        return resetStarted.load();
    }));
    CHECK(!resetFinished.load());
    release = true;
    changed.notify_all();
    CHECK(changed.wait_for(lock, 2s, [&] {
        return resetFinished.load();
    }));
    lock.unlock();
    reset.join();
}

/* optional capture can fail after output admission. report its resource error
without throwing, and keep later captures usable. */
static void checkAllocationFailure() {
    std::atomic<uint64_t> played { 0 };
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<float> received;
    deliver = [&](const mc_pcm_view& pcm) {
        std::lock_guard lock(mutex);
        received.push_back(pcm.data[0]);
        changed.notify_one();
    };
    VisualizationAudioQueue synchronization([&] {
        return std::optional<uint64_t>(played.load());
    });
    float samples[512] {};
    auto pcm = mc_pcm_view MC_PCM_VIEW_DEFAULT;
    pcm.data = samples;
    pcm.frame_count = 512;
    pcm.format.sample_rate = 48000;
    pcm.format.channels = 1;

    /* the first capture allocates the worker's thread state. */
    failNextAllocation = true;
    synchronization.Enqueue(pcm, 0);
    CHECK(lastCaptureFailure.load() == MC_ERROR_INTERNAL);
    CHECK(captureFailures.load() == 1);
    CHECK(!failNextAllocation);
    std::fill_n(samples, 512, 0.25f);
    synchronization.Enqueue(pcm, 512);

    /* with the worker already running, fail the sample copy instead. */
    failNextAllocation = true;
    synchronization.Enqueue(pcm, 1024);
    CHECK(captureFailures.load() == 1);
    CHECK(!failNextAllocation);
    std::fill_n(samples, 512, 0.75f);
    synchronization.Enqueue(pcm, 1536);
    played.store(2048);
    std::unique_lock lock(mutex);
    CHECK(changed.wait_for(lock, 2s, [&] {
        return received.size() == 1;
    }));
    CHECK(received[0] == 0.75f);
}

int main() {
    checkResetDuringDelivery();
    checkAllocationFailure();
    std::atomic<uint64_t> played { 0 };
    std::atomic<bool> clockAvailable { true };
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<float> received;
    std::vector<size_t> sizes;
    deliver = [&](const mc_pcm_view& pcm) {
        std::lock_guard lock(mutex);
        received.push_back(pcm.data[0]);
        sizes.push_back(pcm.frame_count);
        changed.notify_one();
    };
    VisualizationAudioQueue visualization([&] {
        return clockAvailable.load() ? std::optional<uint64_t>(played.load()) : std::nullopt;
    });
    auto waitFor = [&](size_t count) {
        std::unique_lock lock(mutex);
        CHECK(changed.wait_for(lock, 2s, [&] {
            return received.size() == count;
        }));
    };
    auto staysAt = [&](size_t count) {
        std::unique_lock lock(mutex);
        CHECK(!changed.wait_for(lock, 40ms, [&] {
            return received.size() != count;
        }));
    };
    std::vector<float> samples(2048, 0.25f);
    auto pcm = mc_pcm_view MC_PCM_VIEW_DEFAULT;
    pcm.data = samples.data();
    pcm.frame_count = 1024;
    pcm.format.sample_rate = 48000;
    pcm.format.channels = 2;
    consumerEnabled.store(false);
    failNextAllocation = true;
    visualization.Enqueue(pcm, 0);
    CHECK(failNextAllocation);
    failNextAllocation = false;
    staysAt(0);
    consumerEnabled.store(true);
    visualization.Enqueue(pcm, 0);
    /* the output can immediately recycle its own buffer. delayed delivery
    must use our copy and must not advance merely because storage returned. */
    std::fill(samples.begin(), samples.end(), 0.9f);
    staysAt(0);
    played.store(511);
    staysAt(0);
    played.store(512);
    waitFor(1);
    CHECK(received[0] == 0.25f && sizes[0] == 512);
    visualization.SetPaused(true);
    played.store(1024);
    staysAt(1);
    visualization.SetPaused(false);
    waitFor(2);
    CHECK(received[1] == 0.25f);

    visualization.Enqueue(pcm, 1024);
    visualization.Reset();
    played.store(2048);
    staysAt(2);
    std::fill_n(samples.begin(), 1024, 0.6f);
    visualization.Enqueue(pcm, 2048);
    played.store(3072);
    waitFor(3);
    staysAt(3);
    CHECK(received.back() == 0.9f);
    /* advancing by several windows produces only the current picture. */
    CHECK(sizes.back() == 512);
    /* an unavailable frame reading holds queued PCM until the next usable
    snapshot; this queue never substitutes its own timing estimate. */
    clockAvailable.store(false);
    visualization.Enqueue(pcm, 3072);
    played.store(4096);
    staysAt(3);
    clockAvailable.store(true);
    waitFor(4);
    /* more than the bounded capture capacity is safely skipped. */
    pcm.frame_count = 1024 * 1024;
    visualization.Enqueue(pcm, 0);
    staysAt(4);
}
