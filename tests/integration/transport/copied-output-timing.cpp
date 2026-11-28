#include <musikcore/context.hpp>
#include <musikcore/audio/PlaybackWorkers.h>
#include <musikcore/plugin/Plugins.h>
#include "../../support/CopiedOutput.h"
#include "../../support/plugins/OutputContract.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
    using namespace musik::core::audio;
    using output_contract::Require;
    constexpr uint32_t Rate = CopiedOutputSampleRate;
    constexpr uint32_t TrackFrames = CopiedOutputTrackFrames;
    constexpr uint32_t MixFrame = Rate;

    struct Listener : Player::EventListener {
        explicit Listener(ReadCopiedOutput read)
        : read(read) {}
        void OnPlayerBuffered(Player*) override {
            std::lock_guard lock(this->mutex);
            this->buffered = true;
            this->changed.notify_all();
        }
        void OnPlayerMixPoint(Player*, int, double) override {
            std::lock_guard lock(this->mutex);
            this->mixFrame = this->read().playedFrames;
            ++this->mixCount;
            this->changed.notify_all();
        }
        void OnPlayerFinished(Player*) override {
            std::lock_guard lock(this->mutex);
            this->finished = true;
            this->changed.notify_all();
        }
        void OnPlayerOpenFailed(Player*) override {
            std::lock_guard lock(this->mutex);
            this->failed = true;
            this->changed.notify_all();
        }
        ReadCopiedOutput read;
        std::mutex mutex;
        std::condition_variable changed;
        bool buffered = false;
        bool finished = false;
        bool failed = false;
        uint64_t mixFrame = 0;
        unsigned mixCount = 0;
    };

    void word(std::ofstream& file, uint32_t value, unsigned bytes) {
        for (unsigned i = 0; i < bytes; ++i) {
            file.put(static_cast<char>(value >> (8 * i)));
        }
    }

    template<class F> F symbol(void* module, const char* name) {
#ifdef _WIN32
        return reinterpret_cast<F>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
        return reinterpret_cast<F>(dlsym(module, name));
#endif
    }
}

int main(int argc, char** argv) {
    Require(argc == 4, "fixture, timing mode and private media directory supplied");
    const auto plugins = std::filesystem::canonical(argv[0]).parent_path() / "plugins";
    const auto fixture = plugins / argv[1];
#ifdef _WIN32
    void* module = LoadLibraryW(fixture.c_str());
#else
    void* module = dlopen(fixture.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
    Require(module != nullptr, "load device controller");
    const auto read = symbol<ReadCopiedOutput>(module, "ReadCopiedOutputState");
    const auto advance = symbol<AdvanceCopiedOutput>(module, "AdvanceCopiedOutputDevice");
    const auto wait = symbol<WaitCopiedOutput>(module, "WaitCopiedOutputBlocked");
    Require(read && advance && wait, "test device control available");

    const auto path = std::filesystem::path(argv[3]) / "constant.wav";
    {
        std::ofstream file(path, std::ios::binary);
        file.write("RIFF", 4);
        word(file, TrackFrames * 4 + 36, 4);
        file.write("WAVEfmt ", 8);
        word(file, 16, 4);
        word(file, 1, 2);
        word(file, 2, 2);
        word(file, Rate, 4);
        word(file, Rate * 4, 4);
        word(file, 4, 2);
        word(file, 16, 2);
        file.write("data", 4);
        word(file, TrackFrames * 4, 4);
        for (uint32_t i = 0; i < TrackFrames * 2; ++i) {
            word(file, 16384, 2);
        }
        Require(bool(file), "write deterministic half-scale PCM");
    }
    {
        const auto directoryBytes = plugins.u8string();
        const std::string directory(directoryBytes.begin(), directoryBytes.end());
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        options.plugin_directory = { directory.data(), directory.size() };
        musik::core::api::Context context(options);
        Listener listener(read);
        auto output = std::make_shared<Output>(
            musik::core::plugin::FindOutputFactory("test.copied.output"), "copied", "Copied output", argv[2]);
        PlaybackWorkers workers(listener);
        Require(output->Resume() == MC_OK, "resume output");
        const auto bytes = path.u8string();
        auto player = workers.Start(std::string(bytes.begin(), bytes.end()), output, Player::Drain::Yes, {});
        {
            std::unique_lock lock(listener.mutex);
            Require(listener.changed.wait_for(lock, std::chrono::seconds(5),
                        [&] {
                            return listener.buffered || listener.failed;
                        }) &&
                    !listener.failed,
                "decoder buffered without a device clock");
        }
        player->AddMixPoint(1, static_cast<double>(MixFrame) / Rate);
        player->Play();
        Require(wait(0, 5000), "device copied buffers and reached bounded capacity");
        auto state = read();
        const bool known = std::strcmp(argv[2], "unknown") != 0;
        const bool callbackCompletion = std::strcmp(argv[2], "callback") == 0;
        Require(state.playedFrames == 0 && state.returnedBuffers > 0 && state.queuedFrames > 0,
            "buffer reuse precedes audible playback");
        if (known) {
            Require(player->GetPosition() == 0, "copied buffers do not advance audible position");
        }
        else {
            Require(std::isfinite(player->GetPosition()), "unknown timing ignores unflagged NaN values");
        }
        while (state.playedFrames < TrackFrames) {
            const auto before = state.acceptedFrames;
            advance(std::min<size_t>(2048, state.queuedFrames));
            if (!state.draining) {
                Require(wait(before, 5000), "producer refilled or began final drain");
            }
            state = read();
            const auto position = player->GetPosition();
            Require(std::isfinite(position) && position >= 0 && position <= 4, "position stays bounded");
            if (known) {
                Require(std::abs(position - static_cast<double>(state.playedFrames) / Rate) <= 1,
                    "second-resolution position follows consumption rather than returned buffers");
                if (callbackCompletion) {
                    Require(position == std::round(static_cast<double>(state.playedFrames) / Rate),
                        "callback-time completion position follows audible frames without subtracting the software queue");
                }
                std::lock_guard lock(listener.mutex);
                Require(!listener.mixCount || listener.mixFrame >= MixFrame,
                    "latency-adjusted mix point cannot fire before its audible position");
                if (callbackCompletion) {
                    Require(!listener.mixCount || listener.mixFrame <= MixFrame + 4096,
                        "callback-time completion mix point is not delayed by the software queue");
                }
            }
        }
        {
            std::unique_lock lock(listener.mutex);
            Require(listener.changed.wait_for(lock, std::chrono::seconds(5),
                        [&] {
                            return listener.finished || listener.failed;
                        }) &&
                    !listener.failed && listener.mixCount == 1,
                "finish and fire the mix point exactly once");
        }
        workers.Close();
        player.reset();
        output.reset();
    }
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(module));
#else
    dlclose(module);
#endif
}
