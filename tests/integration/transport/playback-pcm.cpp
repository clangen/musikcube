#include <musikcore/visualizer.h>
#include "CoreRuntime.h"
#include "PcmCapture.h"

#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/output.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <string_view>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <dlfcn.h>
#include <sys/resource.h>
#endif

namespace {
    using namespace musik::core;
    using namespace musik::core::audio;
    using namespace playback_fixture;
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

    void* openModule(const std::filesystem::path& path) {
#ifdef _WIN32
        return LoadLibraryW(path.c_str());
#else
        return dlopen(path.c_str(), RTLD_NOW);
#endif
    }

    void closeModule(void* module) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(module));
#else
        dlclose(module);
#endif
    }

    template<class Function>
    Function moduleSymbol(void* module, const char* name) {
#ifdef _WIN32
        return reinterpret_cast<Function>(GetProcAddress(static_cast<HMODULE>(module), name));
#else
        return reinterpret_cast<Function>(dlsym(module, name));
#endif
    }

    constexpr int16_t signalLevel = 16384;
    constexpr int pcmScale = 32768;
    constexpr float sampleTolerance = 0.0001f;
    constexpr std::size_t gaplessFirst = 2 * sampleRate + 137;
    constexpr std::size_t gaplessSecond = 2 * sampleRate + 239;
    constexpr std::size_t crossfadeLength = 8 * sampleRate;

    using StereoFrame = std::array<int16_t, 2>;

    StereoFrame ramp(std::size_t frame) {
        /* Nonzero samples identify extra silence and repeated or dropped frames. */
        return {
            static_cast<int16_t>(1000 + (frame * 23) % 13007),
            static_cast<int16_t>(-2000 - static_cast<int>((frame * 71) % 17011))
        };
    }

    void writeWord(std::ostream& stream, uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            stream.put(static_cast<char>(value >> (8 * i)));
        }
    }

    template<class Samples>
    bool writeWave(const std::filesystem::path& path, std::size_t frames, Samples sampleAt) {
        constexpr int channels = 2;
        constexpr int frameBytes = channels * sizeof(int16_t);
        const auto dataSize = static_cast<uint32_t>(frames * frameBytes);

        std::ofstream stream(path, std::ios::binary);
        stream.write("RIFF", 4);
        writeWord(stream, 36 + dataSize, 4);
        stream.write("WAVEfmt ", 8);
        writeWord(stream, 16, 4);
        writeWord(stream, 1, 2);
        writeWord(stream, channels, 2);
        writeWord(stream, sampleRate, 4);
        writeWord(stream, sampleRate * frameBytes, 4);
        writeWord(stream, frameBytes, 2);
        writeWord(stream, 16, 2);
        stream.write("data", 4);
        writeWord(stream, dataSize, 4);
        for (std::size_t i = 0; i < frames; ++i) {
            for (const auto sample : sampleAt(i)) {
                writeWord(stream, static_cast<uint16_t>(sample), 2);
            }
        }
        stream.close();
        return static_cast<bool>(stream);
    }

    bool prepareMedia(const std::filesystem::path& first, const std::filesystem::path& second, bool crossfade) {
        if (crossfade) {
            return writeWave(first, crossfadeLength, [](auto) { return StereoFrame { signalLevel, 0 }; }) &&
                writeWave(second, crossfadeLength, [](auto) { return StereoFrame { 0, signalLevel }; });
        }

        return writeWave(first, gaplessFirst, ramp) &&
            writeWave(second, gaplessSecond, [](auto i) { return ramp(i + gaplessFirst); });
    }

    bool play(ITransport& transport, const std::filesystem::path& first, const std::filesystem::path& second,
        ReadCapture read)
    {
        transport.Start(first.string(), ITransport::Gain {}, ITransport::StartMode::Immediate);

        const auto deadline = Clock::now() + 25s;
        bool prepared = false;
        while (Clock::now() < deadline) {
            const auto state = transport.GetPlaybackState();
            if (state == sdk::PlaybackState::Playing && !prepared) {
                /* PlaybackService prepares its automatic successor when playback starts. */
                transport.PrepareNextTrack(second.string(), ITransport::Gain {});
                prepared = true;
            }
            if (prepared && state == sdk::PlaybackState::Stopped && read(nullptr, 0) != 0) {
                return true;
            }
            std::this_thread::sleep_for(2ms);
        }

        std::fprintf(stderr, "Playback did not finish within its deadline (%zu captured frames)\n", read(nullptr, 0));
        return false;
    }

    bool checkGapless(const std::vector<CapturedFrame>& frames) {
        const auto expected = gaplessFirst + gaplessSecond;
        const auto tail = std::find_if(frames.rbegin(), frames.rend(), [](const auto& frame) {
            return !frame.underrun;
        });
        const auto captured = static_cast<std::size_t>(tail.base() - frames.begin());
        if (captured != expected) {
            std::fprintf(stderr, "Gapless timeline has %zu frames; expected %zu\n", captured, expected);
            return false;
        }
        for (std::size_t i = 0; i < expected; ++i) {
            const auto [left, right] = ramp(i);
            if (frames[i].underrun || frames[i].interrupted ||
                std::abs(frames[i].left - static_cast<float>(left) / pcmScale) > sampleTolerance ||
                std::abs(frames[i].right - static_cast<float>(right) / pcmScale) > sampleTolerance)
            {
                std::fprintf(stderr, "Gapless PCM differs at frame %zu (track boundary %zu)\n", i, gaplessFirst);
                return false;
            }
        }
        return true;
    }

    bool checkCrossfade(const std::vector<CapturedFrame>& frames) {
        const float fullLevel = static_cast<float>(signalLevel) / pcmScale;
        const float audible = fullLevel * 0.08f;

        /* Each track occupies its own channel, exposing the two gain envelopes. */
        const auto overlapStart = std::find_if(frames.begin(), frames.end(), [=](const auto& frame) {
            return frame.left > audible && frame.right > audible;
        });
        if (overlapStart == frames.end()) {
            std::fputs("Crossfade never played both tracks together\n", stderr);
            return false;
        }
        const auto overlapEnd = std::find_if(overlapStart, frames.end(), [=](const auto& frame) {
            return frame.left <= audible || frame.right <= audible;
        });

        /* The 1.5 s fade has 33 ms steps. Reserve tolerance for the PCM block
           around the mix point and the two gain thresholds. */
        const auto start = static_cast<double>(overlapStart - frames.begin()) / sampleRate;
        const auto duration = static_cast<double>(overlapEnd - overlapStart) / sampleRate;
        if (start < 6.3 || start > 6.9 || duration < 1.05 || duration > 1.5) {
            std::fprintf(stderr, "Crossfade overlap started at %.3fs and lasted %.3fs\n", start, duration);
            return false;
        }

        const auto& middle = *(overlapStart + (overlapEnd - overlapStart) / 2);
        const auto& incoming = *(overlapEnd - 1);
        const auto& outgoing = *overlapStart;
        if (outgoing.left < fullLevel * 0.7f || outgoing.right > fullLevel * 0.3f ||
            incoming.left > fullLevel * 0.3f || incoming.right < fullLevel * 0.7f ||
            middle.left < fullLevel * 0.25f || middle.right < fullLevel * 0.25f ||
            middle.left > fullLevel * 0.75f || middle.right > fullLevel * 0.75f)
        {
            std::fputs("Crossfade did not exchange the two track gains\n", stderr);
            return false;
        }

        for (auto sample = overlapStart + 1; sample != overlapEnd; ++sample) {
            const auto& previous = *(sample - 1);
            if (sample->left > previous.left + sampleTolerance ||
                sample->right < previous.right - sampleTolerance)
            {
                std::fputs("Crossfade changed gain direction during the overlap\n", stderr);
                return false;
            }
        }

        const auto afterFade = overlapEnd - frames.begin() + sampleRate;
        if (afterFade >= static_cast<std::ptrdiff_t>(frames.size()) ||
            std::abs(frames[afterFade].left) > sampleTolerance ||
            std::abs(frames[afterFade].right - fullLevel) > sampleTolerance)
        {
            std::fputs("Incoming track did not continue at full volume\n", stderr);
            return false;
        }

        return true;
    }

    bool stressPlayback(std::string_view mode, const std::filesystem::path& file,
        ReadCapture read, HoldCompletion hold, WaitCapture wait, CountCapture outstanding)
    {
        constexpr unsigned deadlineMs = 3000;
        constexpr int repetitions = 12;
        constexpr int seekSeconds = 4;
        const auto track = [seekSeconds](auto frame) {
            return StereoFrame { static_cast<int16_t>(frame < seekSeconds * sampleRate ? signalLevel : -signalLevel), 0 };
        };
        if (!writeWave(file, crossfadeLength, track)) {
            return false;
        }

        GaplessTransport transport;
        for (int iteration = 0; iteration < (mode == "seek" ? 1 : repetitions); ++iteration) {
            hold(true);
            transport.Start(file.string(), ITransport::Gain {}, ITransport::StartMode::Immediate);
            if (!wait(CaptureEvent::HeldCompletion, deadlineMs)) {
                hold(false);
                std::fputs("Output never held a buffer completion\n", stderr);
                return false;
            }

            if (mode == "seek") {
                transport.SetPosition(1);
                if (!wait(CaptureEvent::Stop, deadlineMs)) {
                    hold(false);
                    std::fputs("First seek did not reach the output flush\n", stderr);
                    return false;
                }
                transport.SetPosition(seekSeconds);
                const auto before = read(nullptr, 0);
                hold(false);
                if (!wait(CaptureEvent::Resumed, deadlineMs)) {
                    std::fputs("Output did not resume after the latest seek\n", stderr);
                    return false;
                }
                std::vector<CapturedFrame> frames(read(nullptr, 0));
                read(frames.data(), frames.size());
                if (frames.size() <= before ||
                    std::abs(frames[before].left + static_cast<float>(signalLevel) / pcmScale) > sampleTolerance)
                {
                    std::fputs("PCM from the superseded seek reached output\n", stderr);
                    return false;
                }
                transport.Stop();
            }
            else {
                /* Alternate retaining the output and replacing it with a completion outstanding. */
                if (iteration % 2 == 0) {
                    transport.Stop();
                }
                else {
                    transport.ReloadOutput();
                }
                hold(false);
            }
            Player::WaitForWorkers();
            if (!wait(CaptureEvent::Balanced, deadlineMs) || outstanding() != 0) {
                std::fprintf(stderr, "Output still owns PCM after iteration %d\n", iteration);
                return false;
            }
        }
        return true;
    }

    bool stopDuringNotification(const std::filesystem::path& file, bool detachFollower = false) {
        /* A single buffer lets teardown reach the final completion even while
           the listener is blocked. The player must outlive that listener call. */
        struct Listener final : Player::EventListener {
            void OnPlayerStarted(Player*) override {
                std::unique_lock lock(this->mutex);
                this->started = true;
                this->changed.notify_all();
                this->changed.wait(lock, [&] { return this->released; });
            }

            void OnPlayerDestroying(Player*) override {
                std::lock_guard lock(this->mutex);
                this->destroying = true;
                this->changed.notify_all();
            }

            std::mutex mutex;
            std::condition_variable changed;
            bool started {};
            bool released {};
            bool destroying {};
        } listener;

        struct Follower final : Player::EventListener {
            void OnPlayerStarted(Player*) override { this->called = true; }
            std::atomic_bool called {};
        } follower;

        if (!writeWave(file, 137, ramp)) {
            return false;
        }

        auto output = outputs::SelectedOutput();
        output->Resume();
        auto* player = Player::Create(file.string(), output, Player::DestroyMode::NoDrain, &listener, {});
        if (detachFollower) {
            player->Attach(&follower);
        }
        player->Play();

        std::unique_lock lock(listener.mutex);
        const bool started = listener.changed.wait_for(lock, 3s, [&] { return listener.started; });
        lock.unlock();

        if (started) {
            if (detachFollower) {
                /* The first callback proves the output worker already snapshotted the listeners. */
                player->Detach(&follower);
            }
            else {
                player->Destroy();
            }
        }
        output->Stop();

        lock.lock();
        const bool destroyedEarly = started && !detachFollower &&
            listener.changed.wait_for(lock, 50ms, [&] { return listener.destroying; });
        listener.released = true;
        listener.changed.notify_all();
        lock.unlock();
        Player::WaitForWorkers();

        if (!started || destroyedEarly) {
            std::fputs("Player did not stay alive through its completion notification\n", stderr);
            return false;
        }
        if (follower.called) {
            std::fputs("Detached listener was called from a stale snapshot\n", stderr);
            return false;
        }
        return true;
    }

    bool replacePreparedTrack(const std::filesystem::path& directory, ReadCapture read,
        HoldCompletion hold, WaitCapture wait, WaitCaptureFrames waitFrames)
    {
        constexpr unsigned deadlineMs = 5000;
        constexpr std::size_t firstFrames = sampleRate + 137;
        constexpr std::size_t nextFrames = sampleRate / 2;
        const auto first = directory / "current.wav";
        const auto superseded = directory / "superseded.wav";
        const auto next = directory / "next.wav";
        if (!writeWave(first, firstFrames, [](auto) { return StereoFrame { signalLevel, 0 }; }) ||
            !writeWave(superseded, nextFrames, [](auto) { return StereoFrame { -signalLevel, 0 }; }) ||
            !writeWave(next, nextFrames, [](auto) { return StereoFrame { 0, signalLevel }; }))
        {
            return false;
        }

        GaplessTransport transport;
        hold(true);
        transport.Start(first.string(), {}, ITransport::StartMode::Immediate);
        if (!wait(CaptureEvent::HeldCompletion, deadlineMs)) {
            hold(false);
            return false;
        }

        /* The bounded output queue keeps the current track away from EOF
           while both commands replace the next player. */
        transport.PrepareNextTrack(superseded.string(), {});
        transport.PrepareNextTrack(next.string(), {});
        hold(false);
        const bool captured = waitFrames(firstFrames + nextFrames, deadlineMs);
        transport.Stop();
        Player::WaitForWorkers();
        if (!captured) {
            return false;
        }

        std::vector<CapturedFrame> frames(read(nullptr, 0));
        read(frames.data(), frames.size());
        const float level = static_cast<float>(signalLevel) / pcmScale;
        for (std::size_t i = 0; i < firstFrames + nextFrames; ++i) {
            const bool current = i < firstFrames;
            if (frames[i].underrun ||
                std::abs(frames[i].left - (current ? level : 0.0f)) > sampleTolerance ||
                std::abs(frames[i].right - (current ? 0.0f : level)) > sampleTolerance)
            {
                std::fprintf(stderr, "Superseded track or discontinuity at frame %zu\n", i);
                return false;
            }
        }
        return true;
    }

    struct ProcessUsage {
        double user;
        double system;
        std::size_t peak;
    };

    bool readProcessUsage(ProcessUsage& result) {
#ifdef _WIN32
        FILETIME start, exit, system, user;
        PROCESS_MEMORY_COUNTERS memory {};
        if (!GetProcessTimes(GetCurrentProcess(), &start, &exit, &system, &user) ||
            !K32GetProcessMemoryInfo(GetCurrentProcess(), &memory, sizeof(memory)))
        {
            return false;
        }
        const auto millis = [](FILETIME value) {
            return ((static_cast<uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime) / 10000.0;
        };
        result = { millis(user), millis(system), memory.PeakWorkingSetSize };
#else
        rusage usage {};
        if (getrusage(RUSAGE_SELF, &usage) != 0) {
            return false;
        }
        const auto millis = [](timeval value) { return value.tv_sec * 1000.0 + value.tv_usec / 1000.0; };
        std::size_t peak = usage.ru_maxrss;
#ifndef __APPLE__
        peak *= 1024;
#endif
        result = { millis(usage.ru_utime), millis(usage.ru_stime), peak };
#endif
        return true;
    }

    bool benchmarkPlayback(const std::filesystem::path& first, const std::filesystem::path& second,
        StoreCapture store, WaitCaptureFrames waitFrames, CountCapture audible)
    {
        constexpr unsigned deadlineMs = 10000;
        constexpr auto frames = gaplessFirst + gaplessSecond;
        store(false);
        ProcessUsage before {};
        if (!readProcessUsage(before)) {
            return false;
        }
        const auto started = Clock::now();
        {
            GaplessTransport transport;
            transport.Start(first.string(), {}, ITransport::StartMode::Immediate);
            transport.PrepareNextTrack(second.string(), {});
            if (!waitFrames(frames, deadlineMs)) {
                transport.Stop();
                Player::WaitForWorkers();
                return false;
            }
            transport.Stop();
            /* Player notifications may already be in flight when Stop returns. */
            Player::WaitForWorkers();
        }
        const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        ProcessUsage after {};
        if (!readProcessUsage(after) || audible() != frames) {
            std::fprintf(stderr, "Benchmark rendered %zu of %zu expected PCM frames\n", audible(), frames);
            return false;
        }
        const auto userMs = after.user - before.user;
        const auto systemMs = after.system - before.system;
        std::printf("playback-benchmark {\"frames\":%zu,\"wallMs\":%.3f,\"cpuMs\":%.3f,"
            "\"userMs\":%.3f,\"systemMs\":%.3f,\"startPeakRssBytes\":%zu,\"peakRssBytes\":%zu}\n",
            frames, elapsed, userMs + systemMs, userMs, systemMs, before.peak, after.peak);
        return true;
    }

    bool destroyPausedCrossfade(const std::filesystem::path& first, const std::filesystem::path& second,
        WaitCapture wait)
    {
        constexpr unsigned overlapDeadlineMs = 18000;
        {
            CrossfadeTransport transport;
            transport.Start(first.string(), {}, ITransport::StartMode::Immediate);
            transport.PrepareNextTrack(second.string(), {});
            if (!wait(CaptureEvent::Overlap, overlapDeadlineMs) || !transport.Pause()) {
                std::fputs("Could not pause two overlapping tracks\n", stderr);
                return false;
            }
            std::fputs("Destroying the transport with both fades paused\n", stderr);
        }
        Player::WaitForWorkers();
        return true;
    }

    bool cancelSourceOpen(WaitCapture wait, void (*release)()) {
        constexpr unsigned deadlineMs = 3000;
        auto stream = Stream::Create();
        bool opened = false;
        std::thread loader([&] { opened = stream->OpenStream(stalledSource, nullptr); });
        const bool opening = wait(CaptureEvent::SourceOpening, deadlineMs);
        if (opening) {
            stream->Interrupt();
        }
        release();
        loader.join();

        if (!opening || opened || !wait(CaptureEvent::SourceInterrupted, 0)) {
            std::fputs("Cancellation did not reach the source published after Interrupt\n", stderr);
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv) {
    if (argc != 4) {
        return 1;
    }

    const std::string_view mode(argv[1]);
    const bool crossfade = mode == "crossfade";
    const bool stress = mode == "teardown" || mode == "seek" || mode == "completion" ||
        mode == "detach_listener" || mode == "replace_next";
    const bool benchmark = mode == "benchmark";
    const bool pausedFade = mode == "paused_crossfade";
    const bool sourceOpen = mode == "cancel_open";
    if (!crossfade && mode != "gapless" && !stress && !benchmark && !pausedFade && !sourceOpen) {
        return 1;
    }

    const auto pluginPath = std::filesystem::canonical(argv[0]).parent_path() / "plugins" / argv[2];
    const std::unique_ptr<void, decltype(&closeModule)> module { openModule(pluginPath), closeModule };
    if (!module) {
        std::fprintf(stderr, "Could not load capture output: %s\n", pluginPath.string().c_str());
        return 1;
    }
    const auto read = moduleSymbol<ReadCapture>(module.get(), "ReadPcmCapture");
    if (!read) {
        std::fputs("Could not find PCM capture reader\n", stderr);
        return 1;
    }

    const auto first = std::filesystem::path(argv[3]) / "first.wav";
    const auto second = std::filesystem::path(argv[3]) / "second.wav";
    if (!stress && !sourceOpen && !prepareMedia(first, second, crossfade || pausedFade)) {
        std::fputs("Could not generate the PCM fixtures\n", stderr);
        return 1;
    }

    CoreRuntime runtime;
    musik::core::plugin::BindServices(&runtime.Queue(), nullptr, LibraryFactory::Instance().DefaultLocalLibrary());
    if (api::ReadOutputConfiguration().selectedDriver != driver) {
        std::fputs("Capture output was not selected\n", stderr);
        return 1;
    }

    if (benchmark) {
        const auto store = moduleSymbol<StoreCapture>(module.get(), "StorePcmCapture");
        const auto waitFrames = moduleSymbol<WaitCaptureFrames>(module.get(), "WaitPcmCaptureFrames");
        const auto audible = moduleSymbol<CountCapture>(module.get(), "AudiblePcmCapture");
        return store && waitFrames && audible && benchmarkPlayback(first, second, store, waitFrames, audible) ? 0 : 1;
    }

    if (pausedFade) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        return wait && destroyPausedCrossfade(first, second, wait) ? 0 : 1;
    }

    if (sourceOpen) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto release = moduleSymbol<void (*)()>(module.get(), "ReleasePcmSourceOpen");
        return wait && release && cancelSourceOpen(wait, release) ? 0 : 1;
    }

    if (stress) {
        if (mode == "completion" || mode == "detach_listener") {
            return stopDuringNotification(first, mode == "detach_listener") ? 0 : 1;
        }
        const auto hold = moduleSymbol<HoldCompletion>(module.get(), "HoldPcmCompletion");
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        if (mode == "replace_next") {
            const auto frames = moduleSymbol<WaitCaptureFrames>(module.get(), "WaitPcmCaptureFrames");
            return hold && wait && frames && replacePreparedTrack(argv[3], read, hold, wait, frames) ? 0 : 1;
        }
        const auto outstanding = moduleSymbol<CountCapture>(module.get(), "OutstandingPcmCapture");
        return hold && wait && outstanding && stressPlayback(mode, first, read, hold, wait, outstanding) ? 0 : 1;
    }

    std::array<float, MC_VISUALIZER_SPECTRUM_BINS> spectrum {};
    uint32_t spectrumRate = 0;
    if (!crossfade && (mc_visualizer_read_spectrum(spectrum.data(), spectrum.size(), &spectrumRate) != MC_ERROR_NOT_FOUND ||
        mc_visualizer_set_spectrum_enabled(true) != MC_OK))
    {
        return 1;
    }

    bool played;
    if (crossfade) {
        CrossfadeTransport transport;
        played = play(transport, first, second, read);
    }
    else {
        GaplessTransport transport;
        played = play(transport, first, second, read);
    }
    Player::WaitForWorkers();
    if (!played) {
        return 1;
    }

    if (!crossfade) {
        const bool valid = mc_visualizer_read_spectrum(spectrum.data(), spectrum.size(), &spectrumRate) == MC_OK &&
            spectrumRate == sampleRate && std::any_of(spectrum.begin(), spectrum.end(), [](float value) { return value > 0; }) &&
            std::all_of(spectrum.begin(), spectrum.end(), [](float value) { return value >= 0 && value <= 80; });
        if (mc_visualizer_set_spectrum_enabled(false) != MC_OK || !valid ||
            mc_visualizer_read_spectrum(spectrum.data(), spectrum.size(), &spectrumRate) != MC_ERROR_NOT_FOUND)
        {
            std::fputs("Expected a captured spectrum from the headless playback stream\n", stderr);
            return 1;
        }
    }
    std::vector<CapturedFrame> frames(read(nullptr, 0));
    read(frames.data(), frames.size());
    return (crossfade ? checkCrossfade(frames) : checkGapless(frames)) ? 0 : 1;
}
