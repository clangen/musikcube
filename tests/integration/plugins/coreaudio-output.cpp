#include <musikcore/audio/Output.h>
#include <musikcore/musikcore.hpp>
#include "../../support/host-wakeup.h"
#include <fstream>
#include <chrono>
#include "../../support/plugins/PluginCandidates.h"
#include <cstdio>
#include <cstdlib>

using musik::core::audio::Output;
using musik::core::ffi::PluginRegistry;

/* silent, fixed storage exercises the actual AudioQueue without accessing
user media or changing the system's selected/default audio device. */
struct Silence final : musik::core::sdk::IBuffer {
    long SampleRate() const override { return rate; }
    void SetSampleRate(long value) override { rate = value; }
    int Channels() const override { return 2; }
    void SetChannels(int) override {}
    float* BufferPointer() const override { return const_cast<float*>(samples.data()); }
    long Samples() const override { return samples.size(); }
    void SetSamples(long) override {}
    long Bytes() const override { return samples.size() * sizeof(float); }
    void Release() override {}
    std::array<float, 8192> samples {};
    long rate = 48000;
};

/* never compile assertions out of packaged-runtime validation. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "CoreAudio output: %s\n", message);
        std::abort();
    }
}

/* only the test/player thread consumes completions; cancellation also returns
storage exactly once, while the hardware callback only publishes records. */
struct Sink final : Output::CompletionSink {
    void CompleteBuffer(musik::core::sdk::IBuffer*, mc_result result) override {
        require(result == MC_OK || result == MC_ERROR_CANCELLED, "meaningful completion outcome");
        ++count;
        successful += result == MC_OK;
    }
    unsigned count = 0;
    unsigned successful = 0;
};

/* collect the buffers returned by the device as the playback worker would. */
static void waitFor(Output::Writer& writer, Sink& sink, unsigned count) {
    while (sink.count < count) {
        writer.ProcessCompletions();
        if (sink.count < count) {
            writer.WaitForReturnedBuffer();
        }
    }
    require(sink.count == count, "each submitted buffer completes once");
}

/* write explicit little-endian WAV header fields; no user-owned media or
catalog is involved in the decoder-to-hardware integration case. */
static void littleEndian(std::ostream& output, uint32_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) {
        output.put(static_cast<char>((value >> (i * 8)) & 255));
    }
}

/* exercise the public context/player API with a real decoder and C CoreAudio,
so host-only tests cannot hide a pipeline or service teardown integration bug. */
static void checkPublicPlayback(const std::string& directory) {
    const auto filename = std::filesystem::u8path(directory) / "silence.wav";
    constexpr unsigned rate = 48000;
    const std::vector<float> samples(rate * 2 * 2, 0.0f);
    {
        std::ofstream file(filename, std::ios::binary);
        const auto bytes = static_cast<uint32_t>(samples.size() * sizeof(float));
        file.write("RIFF", 4);
        littleEndian(file, bytes + 36, 4);
        file.write("WAVEfmt ", 8);
        littleEndian(file, 16, 4);
        littleEndian(file, 3, 2);
        littleEndian(file, 2, 2);
        littleEndian(file, rate, 4);
        littleEndian(file, rate * 8, 4);
        littleEndian(file, 8, 2);
        littleEndian(file, 32, 2);
        file.write("data", 4);
        littleEndian(file, bytes, 4);
        file.write(reinterpret_cast<const char*>(samples.data()), bytes);
        require(bool(file), "silent WAV fixture written");
    }
    const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
        test_host_event_create(), test_host_event_destroy);
    require(bool(event), "public player wakeup event created");
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_PLAYBACK;
    options.plugin_directory = { directory.data(), directory.size() };
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = event.get();
    musik::core::api::Context context(options);
    const auto configuration = musik::core::api::ReadOutputConfiguration();
    require(configuration.selectedDriver == "CoreAudio" && !configuration.devices.empty(),
        "actual engine selects migrated CoreAudio");

    /* pin a device, then switch back to following macOS. callers need the
    empty ID to distinguish system default from a pin to today's default. */
    const auto& device = configuration.devices.front();
    require(musik::core::api::SelectOutputDevice("CoreAudio", device.id) == MC_OK &&
            musik::core::api::ReadOutputConfiguration().selectedDevice.id == device.id,
        "configuration reports the pinned device");
    require(musik::core::api::SelectOutputDevice("CoreAudio", {}) == MC_OK,
        "system default can replace a pin");
    const auto selected = musik::core::api::ReadOutputConfiguration().selectedDevice;
    require(selected.id.empty() && selected.name.empty(), "system default round trips as an empty selection");

    musik::core::api::Player player(context);
    const auto path = filename.u8string();
    require(player.Play(std::string(path.begin(), path.end())) == MC_OK, "public URI playback admitted");
    const auto await = [&](mc_player_state expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        while (player.Snapshot().state != expected) {
            require(player.Snapshot().state != MC_PLAYER_FAILED, "native pipeline did not fail");
            require(std::chrono::steady_clock::now() < deadline, "native pipeline reached expected state");
            test_host_event_wait_for(event.get(), 200);
            require(context.Dispatch() == MC_OK, "native player dispatch");
        }
    };
    await(MC_PLAYER_PLAYING);
    require(player.Pause() == MC_OK, "public CoreAudio pause");
    await(MC_PLAYER_PAUSED);
    require(player.Seek(0.5) == MC_OK && player.Resume() == MC_OK, "public native seek/resume");
    await(MC_PLAYER_FINISHED);
    require(player.Play(std::string(path.begin(), path.end())) == MC_OK, "native playback restarts");
    await(MC_PLAYER_PLAYING);
    require(context.Shutdown() == MC_OK, "public runtime stops live CoreAudio before unload");
    require(Output::InstanceCount() == 0, "runtime destroyed every native output before returning");
}

int main(int argc, char** argv) {
    require(argc == 3, "CoreAudio and decoder DLLs supplied");
    PluginCandidates candidates(std::filesystem::u8path(argv[0]).parent_path() / "coreaudio-runs");
    candidates.Add(std::filesystem::u8path(argv[1]), "coreaudio");
    candidates.Add(std::filesystem::u8path(argv[2]), "decoder");
    PluginRegistry registry;
    require(registry.Initialize(candidates.Directory()) == MC_OK, "C CoreAudio plugin loads");
    const auto factory = registry.Find<mc_output_factory>("7277a19f-a5f7-4123-ac2d-c36273097b72");
    require(bool(factory), "C output factory registered");
    Output probe(factory, "coreaudio", "CoreAudio", "");
    const auto devices = probe.Devices();
    size_t available = 0;
    const auto enumerated = factory.Invoke([&](const mc_output_factory& api) {
        return api.visit_devices(
            api.context,
            [](void* state, const mc_output_device*) {
                ++*static_cast<size_t*>(state);
            },
            &available);
    });
    require(enumerated == MC_OK && available == devices.size(), "native enumeration succeeds before absence can skip");
    std::string nativeDefault;
    const auto defaultStatus = factory.Invoke([&](const mc_output_factory& api) {
        return api.read_default_device(
            api.context,
            [](void* state, const mc_output_device* device) {
                static_cast<std::string*>(state)->assign(device->id.data, device->id.length);
            },
            &nativeDefault);
    });
    require(defaultStatus == MC_OK || defaultStatus == MC_ERROR_NOT_FOUND,
        "default device query is not an operational failure");
    if (defaultStatus == MC_ERROR_NOT_FOUND) {
        std::puts("no physical output device available; native AudioQueue validation skipped");
        return 77;
    }
    require(!nativeDefault.empty(), "default device metadata remains usable");

    /* explicitly select the reported UID as well as using the default path.
    repeat construction/destruction with queued and paused audio. */
    for (unsigned run = 0; run < 3; ++run) {
        Output output(factory, "coreaudio", "CoreAudio", run == 1 ? nativeDefault : "");
        Silence silence;
        silence.rate = output.GetDefaultSampleRate();
        require(silence.rate > 0, "device preferred format is available before prepare");
        Sink sink;
        Output::Writer writer(output, sink);
        output.Resume();
        require(writer.TryQueueAudioBuffer(silence) == MC_OK, "hardware accepts silence");
        /* drain immediately after admission: an initial IsRunning=false must
        not be mistaken for completion while AudioQueue is still starting. */
        const auto started = std::chrono::steady_clock::now();
        require(writer.Drain() == MC_OK, "immediate native drain succeeds");
        writer.ProcessCompletions();
        require(sink.count == 1 && sink.successful == 1, "immediate drain finishes accepted PCM without cancellation");
        const auto elapsed = std::chrono::steady_clock::now() - started;
        require(elapsed >= std::chrono::milliseconds(40), "drain does not truncate queued silence");
        require(
            writer.Drain() == MC_OK && writer.Drain() == MC_OK, "native drain waits for playback and repeats safely");
        require(writer.TryQueueAudioBuffer(silence) == MC_OK, "next same-format buffer");
        output.Pause();
        output.Stop();
        waitFor(writer, sink, 2);

        /* stop closes admission until the host resumes and prepares a fresh
        device generation; a format change must also drain/reprepare safely. */
        output.Resume();
        silence.rate = silence.rate == 44100 ? 48000 : 44100;
        require(writer.TryQueueAudioBuffer(silence) == MC_OK, "new generation accepts changed rate");
        waitFor(writer, sink, 3);
        output.Stop();
        output.Stop();
        writer.ProcessCompletions();
        require(sink.count == 3, "repeated stop creates no duplicate completion");
    }
    checkPublicPlayback(candidates.Directory());
}
