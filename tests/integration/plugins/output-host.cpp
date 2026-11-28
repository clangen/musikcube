#include <common/FilesystemPath.h>
#include "../../support/FatalScope.h"
#include <musikcore/audio/Output.h>
#include <musikcore/audio/Buffer.h>

#include "../../support/plugins/PluginCandidates.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <thread>
#include <barrier>
#include <condition_variable>
#include <exception>
#include <future>

namespace {
    std::atomic<bool> captureAudio { false };
    std::mutex visualizationMutex;
    std::condition_variable visualizationChanged;
    size_t visualizedFrames = 0;
}

/* capture delivery at the production output boundary without a window or FFT. */
namespace musik { namespace core { namespace audio { namespace vis {
    bool HasAudioConsumers() { return captureAudio.load(); }
    void ReportCaptureFailure(mc_result) noexcept {}
    void DeliverAudio(const mc_pcm_view& pcm) {
        std::lock_guard lock(visualizationMutex);
        visualizedFrames += pcm.frame_count;
        visualizationChanged.notify_one();
    }
} } } }

using musik::core::audio::Output;
/* fixed test storage exercises the host boundary without decoder or catalog
services. metadata may be changed deliberately to test malformed views. */
struct Buffer {
    std::array<float, 32> samples {};
    musik::core::audio::Buffer block { samples.data(), static_cast<int>(samples.size()) };

    Buffer() { block.SetSampleRate(48000); }
    operator musik::core::audio::Buffer&() { return block; }
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
    void CompleteBuffer(musik::core::audio::Buffer* buffer, mc_result outcome) override {
        require(outcome == MC_OK || outcome == MC_ERROR_CANCELLED || outcome == expectedFailure,
            "completion has the expected outcome");
        returnedBuffers.push_back(buffer);
        ++completed;
        cancelled += outcome == MC_ERROR_CANCELLED;
        failed += outcome != MC_OK && outcome != MC_ERROR_CANCELLED;
    }
    std::vector<musik::core::audio::Buffer*> returnedBuffers;
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
    /* a failed resize must leave the fixed playback slice valid. */
    std::array<float, 32> fixedStorage {};
    musik::core::audio::Buffer fixed(fixedStorage.data(), static_cast<int>(fixedStorage.size()));
    bool rejected = false;
    try {
        fixed.SetSamples(34);
    }
    catch (const std::exception&) {
        rejected = true;
    }
    require(rejected && fixed.Samples() == 32 && fixed.BufferPointer() == fixedStorage.data(),
        "failed resize preserves the fixed playback slice");
    rejected = false;
    try {
        fixed.Copy(fixedStorage.data(), 2, 31);
    }
    catch (const std::exception&) {
        rejected = true;
    }
    require(rejected && fixed.Samples() == 32 && fixed.BufferPointer() == fixedStorage.data(),
        "out-of-bounds copy preserves the fixed playback slice");
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
    std::mutex prepareMutex;
    std::condition_variable prepareChanged;
    bool holdPrepare = false;
    bool enteredPrepare = false;
    bool releasePrepare = false;
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
            if (message == "prepare") {
                std::unique_lock lock(prepareMutex);
                if (holdPrepare) {
                    enteredPrepare = true;
                    prepareChanged.notify_all();
                    require(prepareChanged.wait_for(lock, std::chrono::seconds(5), [&] {
                        return releasePrepare;
                    }), "test releases prepare before cleanup");
                }
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
    buffer.block.SetSampleRate(48000);
    buffer.block.SetChannels(2);
    buffer.block.SetSamples(32);
    Sink sink;
    const auto factory = registry.Find<mc_output_factory>("output.test");
    if (!fatal) {
        /* output instances need their plugin-scoped settings during creation. */
        environmentFlag("MUSIKCORE_TEST_OUTPUT_SETTINGS", true);
        {
            mc_plugin_settings_service settings = MC_PLUGIN_SETTINGS_SERVICE_DEFAULT;
            settings.read_boolean = [](void*, mc_string_view key, bool, bool* value) -> mc_result {
                if (std::string_view(key.data, key.length) != "fixture") {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
                *value = true;
                return MC_OK;
            };
            Output output(factory, "output.test", "Recording", "", settings);
            require(output.GetDefaultSampleRate() == 44100,
                "format query receives scoped settings before playback");
            require(Output::InstanceCount() == 1, "format query creates one unopened output");
            Output::Writer writer(output, sink);
            output.Resume();
            require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "output receives settings at creation");
            writer.ProcessCompletions();
        }
        environmentFlag("MUSIKCORE_TEST_OUTPUT_SETTINGS", false);
        require(Output::InstanceCount() == 0, "queried output is destroyed at scope exit");
        sink.returnedBuffers.clear();
        sink.completed = 0;
        calls.clear();
    }
    if (!fatal) {
        calls.clear();
        {
            Output output(factory, "output.test", "Recording", "");
            environmentFlag("MUSIKCORE_TEST_PARTIAL_OUTPUT_CREATE", true);
            require(output.GetDefaultSampleRate() < 0, "failed early creation reports an unknown format");
            environmentFlag("MUSIKCORE_TEST_PARTIAL_OUTPUT_CREATE", false);
            require(Output::InstanceCount() == 0 &&
                calls == std::vector<std::string>({ "create", "stop", "destroy" }),
                "failed early creation cleans partial state before returning");
        }
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
        /* position snapshots run on UI readers too. an output still opening
        its device must not make those readers wait for the backend. */
        Output output(factory, "output.test", "Recording", "");
        Sink returned;
        Output::Writer writer(output, returned);
        output.Resume();
        {
            std::lock_guard lock(prepareMutex);
            holdPrepare = true;
        }
        mc_result admitted = MC_ERROR_INTERNAL;
        std::thread producer([&] { admitted = writer.TryQueueAudioBuffer(buffer); });
        {
            std::unique_lock lock(prepareMutex);
            require(prepareChanged.wait_for(lock, std::chrono::seconds(2), [&] {
                return enteredPrepare;
            }), "producer reached the blocked output preparation");
        }
        std::promise<void> readerStarted;
        auto started = readerStarted.get_future();
        auto reading = std::async(std::launch::async, [&] {
            readerStarted.set_value();
            return writer.GetPosition();
        });
        require(started.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
            "position reader started while preparation remained blocked");
        const bool ready = reading.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready;
        {
            std::lock_guard lock(prepareMutex);
            releasePrepare = true;
            holdPrepare = false;
            prepareChanged.notify_all();
        }
        producer.join();
        const auto position = reading.get();
        writer.ProcessCompletions();
        require(admitted == MC_OK, "device preparation resumes after the gate opens");
        require(ready, "position reader must not wait for blocked output preparation");
        require(!position, "busy output position is unavailable rather than guessed");
        require(writer.GetPosition().has_value(), "position reads recover after preparation");
    }
    calls.clear();
    {
        /* immediate storage completion must not display audio before the
        device clock advances. exercise the actual output admission path. */
        std::atomic<uint64_t> played { UINT64_MAX };
        mc_plugin_settings_service settings = MC_PLUGIN_SETTINGS_SERVICE_DEFAULT;
        settings.context = &played;
        settings.read_boolean = [](void*, mc_string_view, bool fallback, bool* value) -> mc_result {
            *value = fallback;
            return MC_OK;
        };
        settings.read_number = [](void* data, mc_string_view, double, double* value) -> mc_result {
            const auto played = static_cast<std::atomic<uint64_t>*>(data)->load();
            if (played == UINT64_MAX) {
                return MC_ERROR_BUSY;
            }
            *value = played;
            return MC_OK;
        };
        Output output(factory, "output.test", "Recording", "", settings);
        Sink returned;
        Output::Writer writer(output, returned);
        output.Resume();
        captureAudio.store(true);
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "clock-controlled output accepts PCM");
        writer.ProcessCompletions();
        require(returned.completed == 1, "output returns storage immediately");
        {
            std::unique_lock lock(visualizationMutex);
            require(!visualizationChanged.wait_for(lock, std::chrono::milliseconds(40), [&] {
                return visualizedFrames != 0;
            }), "busy clock must not fall back to early storage completion");
        }
        played.store(0);
        {
            std::unique_lock lock(visualizationMutex);
            require(!visualizationChanged.wait_for(lock, std::chrono::milliseconds(40), [&] {
                return visualizedFrames != 0;
            }), "valid zero cursor still holds inaudible PCM");
        }
        played.store(16);
        {
            std::unique_lock lock(visualizationMutex);
            require(visualizationChanged.wait_for(lock, std::chrono::seconds(2), [&] {
                return visualizedFrames == 16;
            }), "audible PCM dispatches after the device clock advances");
        }
        captureAudio.store(false);
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "inactive visualizer does not block audio");
        writer.ProcessCompletions();
        played.store(32);
        output.Stop();
        require(visualizedFrames == 16, "inactive visualization captures no further PCM");
    }
    {
        /* delayed recycling from before a seek must not advance the new
        generation when this backend has no timing measurements. */
        Output output(factory, "output.test", "Recording", "");
        Sink returned;
        Output::Writer writer(output, returned);
        output.Resume();
        captureAudio.store(true);
        const auto before = visualizedFrames;
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "queue old fallback generation");
        require(writer.ResetForSeek() == MC_OK, "seek resets fallback generation");
        writer.ProcessCompletions();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "queue fresh fallback generation");
        {
            std::unique_lock lock(visualizationMutex);
            require(!visualizationChanged.wait_for(lock, std::chrono::milliseconds(40), [&] {
                return visualizedFrames != before;
            }), "old completion cannot dispatch new-generation PCM early");
        }
        writer.ProcessCompletions();
        {
            std::unique_lock lock(visualizationMutex);
            require(visualizationChanged.wait_for(lock, std::chrono::seconds(2), [&] {
                return visualizedFrames == before + 16;
            }), "new completion advances the fallback cursor");
        }
        captureAudio.store(false);
    }
    calls.clear();
    {
        Output output(factory, "output.test", "Recording", "");
        Output::Writer writer(output, sink);
        output.Resume();

        /* malformed decoder metadata must fail before creating a backend. the
        C view is now built internally, so callers cannot supply mismatched
        storage, format, or struct prefixes separately. */
        musik::core::audio::Buffer missing(nullptr, 32);
        require(writer.TryQueueAudioBuffer(missing) == MC_ERROR_INVALID_ARGUMENT, "missing storage");
        buffer.block.SetChannels(0);
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "zero channel count");
        buffer.block.SetChannels(2);
        buffer.block.SetSampleRate(0);
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "zero sample rate");
        if (static_cast<uint64_t>(std::numeric_limits<long>::max()) > UINT32_MAX) {
            buffer.block.SetSampleRate(std::numeric_limits<long>::max());
            require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "sample rate exceeds C field");
        }
        buffer.block.SetSampleRate(48000);
        buffer.block.SetSamples(0);
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "empty buffer");
        buffer.block.SetSamples(31);
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "partial audio frame");
        buffer.block.SetSamples(32);
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
        buffer.block.SetChannels(3);
        require(writer.TryQueueAudioBuffer(buffer) == MC_ERROR_INVALID_ARGUMENT, "later malformed block rejected");
        buffer.block.SetChannels(2);
        require(calls.size() == beforeInvalid, "invalid later block never reaches plugin");

        /* same-format gapless blocks keep the device generation alive. */
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "same-format next block");
        writer.ProcessCompletions();
        require(calls.back() == "queue" && calls.size() == started.size() + 1, "gapless does not reprepare");
        /* a different format drains the prior generation before reprepare. */
        buffer.block.SetSampleRate(44100);
        const auto beforeChange = calls.size();
        require(writer.TryQueueAudioBuffer(buffer) == MC_OK, "format-changing transition");
        writer.ProcessCompletions();
        const std::vector<std::string> transition(calls.begin() + beforeChange, calls.end());
        require(transition == std::vector<std::string>({ "drain", "stop", "prepare", "queue" }),
            "old audio drains before the device generation changes");
        buffer.block.SetSampleRate(48000);
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
        changedStorage.block.SetSampleRate(44100);
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
        require(incoming.returnedBuffers == std::vector<musik::core::audio::Buffer*> { &first.block, &second.block },
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
        require(returned.returnedBuffers == std::vector<musik::core::audio::Buffer*> { &failed.block } &&
                returned.failed == 1,
            "worker receives the failed buffer before stopping the driver");

        require(writer.StopAfterFailure() == MC_OK, "worker can stop the failed driver");
        writer.ProcessCompletions();
        require(!writer.HasPending() && returned.completed == 2 && returned.cancelled == 1 &&
                returned.returnedBuffers.back() == &held.block,
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
