#include <common/FilesystemPath.h>
#include "../../support/FatalScope.h"
#include <musikcore/audio/Output.h>

#include "../../support/plugins/PluginCandidates.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <barrier>
#include <condition_variable>
#include <exception>
#include <future>

using musik::core::audio::Output;
/* fixed test storage exercises the host boundary without decoder or catalog
services. metadata may be changed deliberately to test malformed views. */
struct Buffer final : musik::core::sdk::IBuffer {
    long SampleRate() const override { return rate; }
    void SetSampleRate(long value) override { rate = value; }
    int Channels() const override { return channels; }
    void SetChannels(int value) override { channels = value; }
    float* BufferPointer() const override { return missingData ? nullptr : const_cast<float*>(samples.data()); }
    long Samples() const override { return count; }
    void SetSamples(long value) override { count = value; }
    long Bytes() const override { return bytes; }
    void Release() override {}
    std::array<float, 32> samples {};
    long rate = 48000;
    int channels = 2;
    long count = 32;
    long bytes = sizeof(samples);
    bool missingData = false;
};
using musik::core::ffi::PluginRegistry;

/* assertions remain enabled in Release so malformed input cannot silently
reach a plugin in the configuration used by packaged applications. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "output host: %s\n", message);
        std::abort();
    }
}

/* completion must be consumed by the player worker, including inline plugin
completion. the backend callback may not invoke FFT or recycle PCM itself. */
struct Sink final : Output::CompletionSink {
    void CompleteBuffer(musik::core::sdk::IBuffer* buffer, mc_result outcome) override {
        require(outcome == MC_OK || outcome == MC_ERROR_CANCELLED || outcome == expectedFailure,
            "completion has the expected outcome");
        returnedBuffers.push_back(buffer);
        ++completed;
        cancelled += outcome == MC_ERROR_CANCELLED;
        failed += outcome != MC_OK && outcome != MC_ERROR_CANCELLED;
    }
    std::vector<musik::core::sdk::IBuffer*> returnedBuffers;
    unsigned completed = 0;
    unsigned cancelled = 0;
    unsigned failed = 0;
    mc_result expectedFailure = MC_OK;
};

/* select failure behavior only inside this isolated child process. */
static void environmentFlag(const char* name, bool enabled) {
#ifdef _WIN32
    require(_putenv_s(name, enabled ? "1" : "") == 0, "fixture environment updated");
#else
    require((enabled ? setenv(name, "1", 1) : unsetenv(name)) == 0, "fixture environment updated");
#endif
}

/* load a recording DLL through the production registry, then exercise the
same owning boundary used by playback rather than a test-only validator. */
int main(int argc, char** argv) {
    const std::string mode = argc == 3 ? argv[1] : "";
    const bool fatal = mode == "--fatal-stop" || mode == "--fatal-duplicate" || mode == "--fatal-stale";
    require(argc == 2 || fatal, "recording DLL supplied");
    const char* image = argv[fatal ? 2 : 1];
    PluginCandidates candidates(musik::core::sdk::fs::pathFromUtf8(argv[0]).parent_path() / "output-host-runs");
    candidates.Add(musik::core::sdk::fs::pathFromUtf8(image), "recording");
    std::vector<std::string> calls;
    std::mutex logged;
    std::atomic<bool> blocked { false };
    std::mutex capacityMutex;
    std::condition_variable capacityChanged;
    bool capacityPending = false;
    bool permitCapacity = false;
    bool capacityReturned = false;
    std::mutex drainMutex;
    std::condition_variable drainChanged;
    bool holdDrain = false;
    bool enteredDrain = false;
    bool releaseDrain = false;
    PluginRegistry registry([&](mc_log_level, mc_string_view tag, mc_string_view text) {
        if (std::string(tag.data, tag.length) == "output-test") {
            const std::string message(text.data, text.length);
            {
                std::lock_guard lock(logged);
                calls.push_back(message);
                if (fatal && message == "destroy") {
                    std::fputs("fatal-test: output destroyed\n", stderr);
                }
            }
            if (message == "blocked") {
                blocked.store(true);
                blocked.notify_all();
            }
            if (message == "capacity-pending") {
                std::unique_lock lock(capacityMutex);
                capacityPending = true;
                capacityChanged.notify_all();
                require(capacityChanged.wait_for(lock, std::chrono::seconds(5),
                            [&] {
                                return permitCapacity;
                            }),
                    "test releases capacity gate before cleanup");
            }
            if (message == "capacity-ready") {
                std::lock_guard lock(capacityMutex);
                capacityReturned = true;
                capacityChanged.notify_all();
            }
            if (message == "drain") {
                std::unique_lock lock(drainMutex);
                if (holdDrain) {
                    enteredDrain = true;
                    drainChanged.notify_all();
                    require(drainChanged.wait_for(lock, std::chrono::seconds(5),
                                [&] {
                                    return releaseDrain;
                                }),
                        "test releases drain before cleanup");
                }
            }
        }
    });
    require(registry.Initialize(candidates.Directory()) == MC_OK, "recording DLL loads");
    Buffer buffer;
    buffer.SetSampleRate(48000);
    buffer.SetChannels(2);
    buffer.SetSamples(32);
    Sink sink;
    const auto factory = registry.Find<mc_output_factory>("output.test");
    if (!fatal) {
        /* output instances need their plugin-scoped preferences during creation. */
        environmentFlag("MUSIKCORE_TEST_OUTPUT_PREFERENCES", true);
        {
            mc_preferences_host_api preferences = MC_PREFERENCES_HOST_API_DEFAULT;
            preferences.read_boolean = [](void*, mc_string_view key, bool, bool* value) -> mc_result {
                if (std::string_view(key.data, key.length) != "fixture") {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                *value = true;
                return MC_OK;
            };
            Output output(factory, "output.test", "Recording", "", preferences);
            Output::Writer writer(output, sink);
            output.Resume();
            require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "output receives preferences at creation");
            writer.ProcessCompletions();
        }
        environmentFlag("MUSIKCORE_TEST_OUTPUT_PREFERENCES", false);
        sink.returnedBuffers.clear();
        sink.completed = 0;
        calls.clear();
    }
    if (fatal) {
        /* the runner rejects aborts after output destruction as well as normal
        return or stack unwinding. the logger records destruction below. */
        FatalScope fatalScope;
        const auto device = mode == "--fatal-duplicate" ? "duplicate" : mode == "--fatal-stale" ? "stale" : "";
        Output output(factory, "output.test", "Recording", device);
        Output::Writer writer(output, sink);
        output.Resume();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "fatal fixture prepares");
        writer.ProcessCompletions();
        if (mode == "--fatal-stale") {
            /* reuse a freed host slot, then report its previous generation's
            token: matching a slot index alone must never authorize recycling. */
            writer.TryQueueAudioBuffer(buffer);
        }
        else if (mode == "--fatal-stop") {
            environmentFlag("MUSIKCORE_TEST_FAIL_OUTPUT_STOP", true);
            output.Stop();
        }
        return 79;
    }
    {
        Output output(factory, "output.test", "Recording", "");
        Output::Writer writer(output, sink);
        output.Resume();

        /* malformed decoder metadata must fail before creating a backend. the
        C view is now built internally, so callers cannot supply mismatched
        storage, format, or struct prefixes separately. */
        buffer.missingData = true;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "missing storage");
        buffer.missingData = false;
        buffer.channels = 0;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "zero channel count");
        buffer.channels = 2;
        buffer.rate = 0;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "zero sample rate");
        if (static_cast<uint64_t>(std::numeric_limits<long>::max()) > UINT32_MAX) {
            buffer.rate = std::numeric_limits<long>::max();
            require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "sample rate exceeds C field");
        }
        buffer.rate = 48000;
        buffer.count = -1;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "negative sample count");
        buffer.count = 0;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "empty buffer");
        buffer.count = 31;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "partial audio frame");
        buffer.count = 34;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "samples exceed backing storage");
        buffer.count = std::numeric_limits<long>::max() - 1;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "oversized sample count");
        buffer.count = 32;
        buffer.bytes = -1;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "negative storage capacity");
        buffer.bytes = sizeof(buffer.samples);
        output.SetVolume(std::numeric_limits<double>::quiet_NaN());
        output.SetVolume(-0.1);
        output.SetVolume(1.1);
        require(output.GetVolume() == 1.0, "invalid normalized volume never reaches the backend");
        require(calls.empty(), "invalid views never reach downstream create/prepare/queue");

        /* valid admission prepares first; inline completion is only published,
        then consumed explicitly by the owning worker. */
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "valid PCM accepted");
        require(sink.completed == 0, "backend completion does not execute player work");
        writer.ProcessCompletions();
        require(sink.completed == 1, "worker consumes the completion once");
        const std::vector<std::string> started { "create", "stop", "prepare", "queue" };
        require(calls == started, "prepare precedes queue and host owns initial cleanup");

        /* a valid first block does not make later decoder metadata trustworthy.
        reject a later overrun without calling the already-prepared backend. */
        const auto beforeInvalid = calls.size();
        buffer.count = 34;
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "later malformed block rejected");
        buffer.count = 32;
        require(calls.size() == beforeInvalid, "invalid later block never reaches plugin");

        /* same-format gapless blocks keep the device generation alive. */
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "same-format next block");
        writer.ProcessCompletions();
        require(calls.back() == "queue" && calls.size() == started.size() + 1, "gapless does not reprepare");
        /* a different format drains the prior generation before reprepare. */
        buffer.SetSampleRate(44100);
        const auto beforeChange = calls.size();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "format-changing transition");
        writer.ProcessCompletions();
        const std::vector<std::string> transition(calls.begin() + beforeChange, calls.end());
        require(transition == std::vector<std::string>({ "drain", "stop", "prepare", "queue" }),
            "old audio drains before the device generation changes");
        buffer.SetSampleRate(48000);
        output.Stop();
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_CANCELLED, "stop closes admission");
        output.Resume();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "resume prepares a new generation");
        writer.ProcessCompletions();
    }
    require(calls.back() == "destroy" && calls[calls.size() - 2] == "stop", "stop completes before destruction");

    /* failed prepare is cleaned immediately, accepts no PCM, and remains safe
    to destroy. this policy belongs to the host, not the C++ plugin helper. */
    calls.clear();
    {
        Output output(factory, "output.test", "Recording", "fail");
        Output::Writer writer(output, sink);
        output.Resume();
        require(writer.TryQueueAudioBuffer(buffer) != MC_OK, "failed prepare accepts no PCM");
        const std::vector<std::string> expected { "create", "stop", "prepare", "stop" };
        require(calls == expected, "failed prepare is stopped before any reuse");
    }
    require(calls.back() == "destroy" && calls[calls.size() - 2] == "stop", "failed instance stops before destroy");
    /* a failed factory may transfer partial ownership; stop/destroy it now,
    then create a fresh instance on retry instead of reusing partial state. */
    calls.clear();
    {
        Output output(factory, "output.test", "Recording", "");
        Output::Writer writer(output, sink);
        output.Resume();
        environmentFlag("MUSIKCORE_TEST_PARTIAL_OUTPUT_CREATE", true);
        const auto failed = writer.TryQueueAudioBuffer(buffer);
        environmentFlag("MUSIKCORE_TEST_PARTIAL_OUTPUT_CREATE", false);
        require(failed != MC_OK && Output::InstanceCount() == 0, "partial creation is released without accepting PCM");
        require(calls == std::vector<std::string>({ "create", "stop", "destroy" }), "partial object cleanup order");
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "retry constructs a fresh instance");
        writer.ProcessCompletions();
    }

    /* a producer genuinely blocked inside the plugin must be interruptible
    without waiting on the host's ordinary-call lock. */
    {
        Output output(factory, "output.test", "Recording", "block");
        Sink cancelled;
        Output::Writer writer(output, cancelled);
        output.Resume();
        mc_result result;
        std::thread producer([&] {
            result = writer.TryQueueAudioBuffer(buffer);
        });
        blocked.wait(false);

        /* moving a volume slider must finish while the writer is still
        blocked. only the most recent setting matters when the device returns. */
        auto volumes = std::async(std::launch::async, [&] {
            for (int step = 1; step <= 32; ++step) {
                if (output.SetVolume(step / 64.0) != MC_ERROR_IN_FLIGHT) {
                    return false;
                }
            }
            return output.GetVolume() == 0.5;
        });
        const bool finished = volumes.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        output.Stop();
        producer.join();
        require(finished && volumes.get(), "volume changes retain the latest gain without waiting on the device");
        require(result == MC_OK, "accepted blocked producer reports ownership transfer");
        writer.ProcessCompletions();
        require(cancelled.completed == 1 && cancelled.cancelled == 1,
            "Stop returns blocked producer token with cancellation");
    }

    /* returning storage need not free capacity. consume that completion and
    retry while space is still unavailable, then require an independent wake.
    also cover ready arriving after the scan but before atomic wait begins. */
    for (const bool notifyBeforeWait : { false, true }) {
        capacityPending = false;
        permitCapacity = false;
        capacityReturned = false;
        Output output(factory, "output.test", "Recording", "ready");
        Sink returned;
        Output::Writer writer(output, returned);
        output.Resume();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "readiness fixture accepts first block");
        {
            std::unique_lock lock(capacityMutex);
            require(capacityChanged.wait_for(lock, std::chrono::seconds(5),
                        [&] {
                            return capacityPending;
                        }),
                "storage returns before capacity gate opens");
        }
        writer.ProcessCompletions();
        require(returned.completed == 1, "storage completion is already consumed");
        /* consume the earlier buffer notification so only the independent
        ready callback can release the next wait. */
        writer.WaitForReturnedBuffer();
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_QUEUE_FULL,
            "completion alone does not imply backend space");
        std::promise<void> starting;
        const auto releaseCapacity = [&] {
            std::lock_guard lock(capacityMutex);
            permitCapacity = true;
            capacityChanged.notify_all();
        };
        if (notifyBeforeWait) {
            releaseCapacity();
            std::unique_lock lock(capacityMutex);
            require(capacityChanged.wait_for(lock, std::chrono::seconds(5),
                        [&] {
                            return capacityReturned;
                        }),
                "ready is delivered before waiting");
        }
        auto waiting = std::async(std::launch::async, [&] {
            starting.set_value();
            for (;;) {
                writer.WaitForQueueSpace();
                writer.ProcessCompletions();
                const auto result = writer.TryQueueAudioBuffer(buffer);
                if (result != MC_ERROR_QUEUE_FULL) {
                    return result;
                }
            }
        });
        starting.get_future().wait();
        if (!notifyBeforeWait) {
            releaseCapacity();
        }
        const bool woke = waiting.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
        /* rescue the worker on failure so a missing notification cannot hide
        the assertion behind the future's joining destructor. */
        if (!woke) {
            output.NotifyWriters();
        }
        const auto result = waiting.get();
        require(woke, "capacity readiness wakes a retry without another completion");
        require(result == MC_OK, "retry uses published capacity");
        writer.ProcessCompletions();
        require(returned.completed == 2, "refused block completes only after successful retry");
    }

    /* hold an old track at its worker gate across replacement. stopping and
    resuming a shared device must not readmit the revoked track or let its seek
    and drain touch the replacement's backend generation. */
    {
        Output output(factory, "output.test", "Recording", "hold");
        Sink oldSink;
        Sink newSink;
        Output::Writer retired(output, oldSink);
        Output::Writer replacement(output, newSink);
        Buffer oldStorage;
        Buffer newStorage;
        std::promise<void> queued;
        std::promise<void> release;
        auto continueRetired = release.get_future();
        output.Resume();
        std::thread oldWorker([&] {
            require(retired.TryQueueAudioBuffer(oldStorage) == MC_OK, "old writer owns one pending block");
            queued.set_value();
            require(continueRetired.wait_for(std::chrono::seconds(5)) == std::future_status::ready,
                "test releases retired writer before cleanup");
            require(retired.TryQueueAudioBuffer(oldStorage) == MC_ERROR_CANCELLED,
                "revoked writer cannot queue after replacement resumes the output");
            require(retired.ResetForSeek() == MC_ERROR_CANCELLED, "revoked seek cannot reset the replacement");
            require(retired.Drain() == MC_ERROR_CANCELLED, "revoked drain cannot wait for the replacement");
            require(retired.StopAfterFailure() == MC_ERROR_CANCELLED, "revoked failure cannot stop the replacement");
            retired.ProcessCompletions();
            require(!retired.HasPending(), "retired worker reclaims all its own storage");
        });
        require(queued.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready,
            "old writer reaches the replacement gate");
        retired.Revoke();
        output.Stop();
        output.Resume();
        require(replacement.TryQueueAudioBuffer(newStorage) == MC_OK, "replacement writer enters the resumed output");
        const auto beforeRetired = calls.size();
        release.set_value();
        oldWorker.join();
        require(calls.size() == beforeRetired, "revoked operations never reach the backend");
        require(replacement.HasPending(), "replacement keeps its pending storage");
        output.Stop();
        replacement.ProcessCompletions();
        require(!replacement.HasPending() && newSink.cancelled == 1, "replacement completes independently");
    }

    /* a current seek resets queued storage and paused state in one operation;
    terminal transport Stop must still prevent that seek from reopening it. */
    {
        Output output(factory, "output.test", "Recording", "hold");
        Sink seekSink;
        Output::Writer writer(output, seekSink);
        output.Resume();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "seek owns queued storage");
        output.Pause();
        require(writer.ResetForSeek() == MC_OK, "current writer resets paused playback");
        writer.ProcessCompletions();
        require(!writer.HasPending() && seekSink.cancelled == 1, "seek returns old storage with cancellation");
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "seek leaves output runnable");
        output.Stop();
        writer.ProcessCompletions();
        const auto beforeStopped = calls.size();
        require(writer.ResetForSeek() == MC_ERROR_CANCELLED, "seek cannot reopen a transport-stopped output");
        require(calls.size() == beforeStopped, "stopped seek never touches backend state");
    }

    /* revocation can arrive inside the format-change drain. check again before
    any stop/prepare, rather than admitting the old track on its earlier check. */
    {
        Output output(factory, "output.test", "Recording", "hold");
        Sink oldSink;
        Sink newSink;
        Output::Writer retired(output, oldSink);
        Output::Writer replacement(output, newSink);
        Buffer oldStorage;
        Buffer changedStorage;
        changedStorage.rate = 44100;
        output.Resume();
        require(retired.TryQueueAudioBuffer(oldStorage) == MC_OK, "format transition begins with pending audio");
        {
            std::lock_guard lock(drainMutex);
            holdDrain = true;
        }
        mc_result result = MC_OK;
        std::thread oldWorker([&] {
            result = retired.TryQueueAudioBuffer(changedStorage);
        });
        {
            std::unique_lock lock(drainMutex);
            require(drainChanged.wait_for(lock, std::chrono::seconds(5),
                        [&] {
                            return enteredDrain;
                        }),
                "old writer reaches format-change drain");
        }
        retired.Revoke();
        output.Stop();
        const auto beforeRelease = calls.size();
        {
            std::lock_guard lock(drainMutex);
            releaseDrain = true;
            drainChanged.notify_all();
        }
        oldWorker.join();
        require(result == MC_ERROR_CANCELLED, "revocation during drain rejects the old block");
        require(calls.size() == beforeRelease, "revoked format transition cannot stop or prepare a backend");
        retired.ProcessCompletions();
        require(!retired.HasPending(), "old format storage is reclaimed");
        {
            std::lock_guard lock(drainMutex);
            holdDrain = false;
        }
        output.Resume();
        require(replacement.TryQueueAudioBuffer(changedStorage) == MC_OK, "replacement negotiates its format");
        output.Stop();
        replacement.ProcessCompletions();
    }

    /* gapless players share host slots. returning the old track's buffer
    lets the new track's second block reuse a lower slot than its first. */
    {
        Output output(factory, "output.test", "Recording", "");
        Sink outgoing;
        Sink incoming;
        Output::Writer oldTrack(output, outgoing);
        Output::Writer newTrack(output, incoming);
        Buffer tail;
        Buffer first;
        Buffer second;
        output.Resume();
        require(oldTrack.TryQueueAudioBuffer(tail) == MC_OK, "old track occupies the first slot");
        require(newTrack.TryQueueAudioBuffer(first) == MC_OK, "new track starts in a later slot");
        oldTrack.ProcessCompletions();
        require(newTrack.TryQueueAudioBuffer(second) == MC_OK, "next block reuses the freed slot");

        newTrack.ProcessCompletions();
        require(incoming.returnedBuffers == std::vector<musik::core::sdk::IBuffer*> { &first, &second },
            "completed buffers reach the player in track order after slot reuse");
    }

    /* a failed write can arrive before an earlier buffer comes back. the
    player needs the error to stop the device and recover that first buffer. */
    {
        Output output(factory, "output.test", "Recording", "fail-second");
        Sink returned;
        returned.expectedFailure = MC_ERROR_IO;
        Output::Writer writer(output, returned);
        Buffer held;
        Buffer failed;
        output.Resume();
        require(writer.TryQueueAudioBuffer(held) == MC_OK, "device holds first buffer until stop");
        require(writer.TryQueueAudioBuffer(failed) == MC_OK, "device accepts and then fails the next buffer");

        writer.ProcessCompletions();
        require(returned.returnedBuffers == std::vector<musik::core::sdk::IBuffer*> { &failed } &&
                returned.failed == 1,
            "worker receives the failed buffer before stopping the driver");

        require(writer.StopAfterFailure() == MC_OK, "worker can stop the failed driver");
        writer.ProcessCompletions();
        require(!writer.HasPending() && returned.completed == 2 && returned.cancelled == 1 &&
                returned.returnedBuffers.back() == &held,
            "stop returns the first buffer exactly once");
    }

    /* exhaust the bounded host records, then have two actual player workers
    consume delayed cancellation concurrently. neither may steal the other's
    last completion or sleep forever after it becomes visible again. */
    for (unsigned run = 0; run < 40; ++run) {
        Output output(factory, "output.test", "Recording", "hold");
        std::array<Buffer, 65> storage;
        Sink first;
        Sink second;
        Output::Writer firstWriter(output, first);
        Output::Writer secondWriter(output, second);
        output.Resume();
        for (size_t i = 0; i < 64; ++i) {
            require((i % 2 ? firstWriter : secondWriter).TryQueueAudioBuffer(storage[i]) == MC_OK,
                "bounded host slot accepts one owned buffer");
        }
        require(firstWriter.TryQueueAudioBuffer(storage.back()) == MC_ERROR_QUEUE_FULL,
            "host slot exhaustion is backpressure, never allocation or lost ownership");
        std::barrier ready(3);
        const auto consume = [&](Output::Writer& writer, Sink& sink) {
            ready.arrive_and_wait();
            while (sink.completed < 32) {
                writer.ProcessCompletions();
                if (sink.completed < 32) {
                    writer.WaitForReturnedBuffer();
                }
            }
        };
        std::thread a([&] {
            consume(firstWriter, first);
        });
        std::thread b([&] {
            consume(secondWriter, second);
        });
        ready.arrive_and_wait();
        output.Stop();
        a.join();
        b.join();
        require(first.completed == 32 && second.completed == 32 && first.cancelled == 32 && second.cancelled == 32,
            "shared output returns every cancelled token exactly once");
    }

    /* unplug a selected device after saving its ID. the next stream should
    use the system rate and play on the default route. reconnecting restores
    the saved pin without requiring another trip through settings. */
    {
        Output output(factory, "output.test", "Recording", "");
        require(output.SetDefaultDevice("usb"), "available USB device can be pinned");
        require(output.GetDefaultSampleRate() == 44100, "pinned device provides its own rate");
        environmentFlag("MUSIKCORE_TEST_OUTPUT_USB_UNPLUGGED", true);
        require(output.GetDefaultSampleRate() == 48000, "missing pin falls back to the system sample rate");
        Output::Writer writer(output, sink);
        output.Resume();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "missing pin plays through the system route");
        writer.ProcessCompletions();
        output.Stop();

        environmentFlag("MUSIKCORE_TEST_OUTPUT_USB_UNPLUGGED", false);
        require(output.GetDefaultSampleRate() == 44100, "reconnected device is still pinned");
    }
}
