#include <musikcore/visualizer.h>
#include "CoreRuntime.h"
#include "FatalScope.h"
#include "PcmCapture.h"

#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/audio/Outputs.h>
#include <musikcore/audio/PlaybackWorkers.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/output.hpp>
#include <musikcore/player.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
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
            if (!wait(CaptureEvent::Balanced, deadlineMs) || outstanding() != 0) {
                std::fprintf(stderr, "Output still owns PCM after iteration %d\n", iteration);
                return false;
            }
        }
        return true;
    }

    /* a track may be stopped while its transport is handling the first PCM
    event. joining the session must wait for that callback to return. */
    bool stopDuringNotification(const std::filesystem::path& file) {
        struct Listener final : Player::EventListener {
            void OnPlayerStarted(Player*) override {
                std::unique_lock lock(this->mutex);
                this->started = true;
                this->changed.notify_all();
                this->changed.wait(lock, [&] { return this->release; });
            }

            void OnPlayerStopped(Player*) override {
                this->finished = true;
            }

            std::mutex mutex;
            std::condition_variable changed;
            bool started = false;
            bool release = false;
            std::atomic_bool finished { false };
        } listener;

        if (!writeWave(file, 137, ramp)) {
            return false;
        }
        PlaybackWorkers playbackWorkers(listener);
        auto output = outputs::SelectedOutput();
        output->Resume();
        auto player = playbackWorkers.Start(file.string(), output, Player::Drain::No, {});
        player->Play();

        std::unique_lock lock(listener.mutex);
        const bool started = listener.changed.wait_for(lock, 3s, [&] { return listener.started; });
        lock.unlock();
        player.reset();

        std::promise<void> entering;
        auto closing = std::async(std::launch::async, [&] {
            entering.set_value();
            playbackWorkers.Close();
        });
        entering.get_future().wait();
        /* even after the caller drops its track, Close must wait for the
        callback using the listener. release it on both success and failure. */
        const bool stillClosing = closing.wait_for(50ms) == std::future_status::timeout;
        lock.lock();
        listener.release = true;
        listener.changed.notify_all();
        lock.unlock();
        closing.get();
        return started && stillClosing && listener.finished.load();
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

    /* backend failures after ownership transfer must stop the session, return
    pending buffers, and publish failure instead of EOF or automatic next-track
    playback. CTest bounds worker cleanup as a final deadlock safeguard. */
    bool failOutput(const std::filesystem::path& file, bool callbackFailure) {
        if (!writeWave(file, sampleRate, [](auto) { return StereoFrame {}; })) {
            return false;
        }
        const char* key = callbackFailure ? "MUSIKCORE_TEST_OUTPUT_COMPLETION_ERROR" : "MUSIKCORE_TEST_OUTPUT_QUEUE_ERROR";
#ifdef _WIN32
        _putenv_s(key, "1");
#else
        setenv(key, "1", 1);
#endif
        GaplessTransport transport;
        std::mutex mutex;
        std::condition_variable changed;
        unsigned failures = 0;
        bool finished = false;
        auto connection = transport.StreamEvent.Subscribe([&](auto state, const std::string&) {
            std::lock_guard lock(mutex);
            failures += state == musik::core::sdk::StreamState::OpenFailed;
            finished |= state == musik::core::sdk::StreamState::Finished;
            changed.notify_all();
        });
        transport.Start(file.string(), {}, ITransport::StartMode::Immediate);
        bool observed;
        {
            std::unique_lock lock(mutex);
            observed = changed.wait_for(lock, 3s, [&] { return failures || finished; });
        }
        transport.Destroy();
#ifdef _WIN32
        _putenv_s(key, "");
#else
        unsetenv(key);
#endif
        return observed && failures == 1 && !finished && Output::InstanceCount() == 0;
    }

    enum class PausedReplacement { Direct, Stop, FailedOpen };

    /* the old track is on the left and its replacement is on the right.
    a paused fade worker would leave the replacement running at zero gain. */
    bool replacePausedCrossfade(const std::filesystem::path& first, const std::filesystem::path& second,
        PausedReplacement action, ReadCapture read, WaitCaptureFrames waitFrames)
    {
        CrossfadeTransport transport;
        transport.Start(first.string(), {}, ITransport::StartMode::Immediate);
        if (!waitFrames(sampleRate / 4, 3000) || transport.Pause() != MC_OK) {
            return false;
        }
        if (action == PausedReplacement::Stop) {
            transport.Stop();
        }
        else if (action == PausedReplacement::FailedOpen) {
            std::promise<void> failure;
            auto failed = transport.StreamEvent.Subscribe([&](auto state, const std::string&) {
                if (state == sdk::StreamState::OpenFailed) {
                    failure.set_value();
                }
            });
            std::atomic_bool claimedPlaying = false;
            auto playing = transport.PlaybackEvent.Subscribe([&](auto state) {
                if (state == sdk::PlaybackState::Playing) {
                    claimedPlaying = true;
                }
            });
            transport.Start((first.parent_path() / "absent.wav").string(), {}, ITransport::StartMode::Immediate);
            if (failure.get_future().wait_for(3s) != std::future_status::ready ||
                transport.GetPlaybackState() != sdk::PlaybackState::Stopped || claimedPlaying.load())
            {
                std::fputs("Failed replacement did not stop cleanly from pause\n", stderr);
                return false;
            }
        }

        const auto start = read(nullptr, 0);
        transport.Start(second.string(), {}, ITransport::StartMode::Immediate);
        if (!waitFrames(start + sampleRate, 3000)) {
            std::fputs("Playback did not resume with the replacement track\n", stderr);
            return false;
        }
        std::vector<CapturedFrame> frames(read(nullptr, 0));
        read(frames.data(), frames.size());
        const auto replacement = frames.begin() + start;
        const bool audible = std::any_of(replacement, frames.end(), [](const auto& frame) {
            return frame.right > 0.05f;
        });
        const bool oldTrackHeard = std::any_of(replacement, frames.end(), [](const auto& frame) {
            return frame.left > 0.01f;
        });
        if (!audible || (action != PausedReplacement::Direct && oldTrackHeard) ||
            transport.GetPlaybackState() != sdk::PlaybackState::Playing)
        {
            std::fputs("Replacement track remained silent or a stopped track resumed\n", stderr);
            return false;
        }
        return true;
    }

    /* seeking resumes playback. after seeking past an overlap, the chosen
    track must return to full gain and the previous track must stay silent. */
    bool seekPausedCrossfade(const std::filesystem::path& first, const std::filesystem::path& second,
        ReadCapture read, WaitCapture wait, WaitCaptureFrames waitFrames)
    {
        CrossfadeTransport transport;
        transport.Start(first.string(), {}, ITransport::StartMode::Immediate);
        if (!waitFrames(sampleRate / 4, 3000)) {
            return false;
        }
        transport.Start(second.string(), {}, ITransport::StartMode::Immediate);
        if (!wait(CaptureEvent::Overlap, 3000) || transport.Pause() != MC_OK) {
            return false;
        }
        const auto start = read(nullptr, 0);
        transport.SetPosition(2.0);
        if (!waitFrames(start + 2 * sampleRate, 4500)) {
            std::fputs("Paused crossfade did not play after a seek\n", stderr);
            return false;
        }
        std::vector<CapturedFrame> frames(read(nullptr, 0));
        read(frames.data(), frames.size());
        const bool reachedFullGain = std::any_of(frames.begin() + start, frames.end(), [](const auto& frame) {
            return frame.right > 0.475f && std::abs(frame.left) < sampleTolerance;
        });
        if (!reachedFullGain || transport.GetPlaybackState() != sdk::PlaybackState::Playing) {
            std::fputs("Seek left the selected track at its paused fade gain\n", stderr);
            return false;
        }
        return true;
    }

    /* act on the initial open so decoder readiness always arrives after the
    command. an explicit pause stays paused; restoring a position during
    Prepare leaves the track ready for the listener's first Play. */
    bool checkCrossfadeOpenIntent(const std::filesystem::path& file, ITransport::StartMode start) {
        CrossfadeTransport transport;
        const bool prepare = start == ITransport::StartMode::Wait;
        std::promise<void> buffered;
        auto ready = buffered.get_future();
        mc_result command = MC_OK;
        auto subscription = transport.StreamEvent.Subscribe([&](auto state, const std::string&) {
            if (state == sdk::StreamState::Buffering) {
                if (prepare) {
                    transport.SetPosition(0.25);
                }
                else {
                    command = transport.Pause();
                }
            }
            else if (state == sdk::StreamState::Buffered) {
                buffered.set_value();
            }
        });
        transport.Start(file.string(), {}, start);
        const auto expected = prepare ? sdk::PlaybackState::Prepared : sdk::PlaybackState::Paused;
        return ready.wait_for(3s) == std::future_status::ready && command == MC_OK &&
            transport.GetPlaybackState() == expected && transport.Resume() == MC_OK;
    }

    /* a failed device control must reach the C API without publishing a
    playback state that the device never entered. */
    bool checkDeviceControls(const std::filesystem::path& file, bool fade, CoreRuntime& runtime,
        SetControlFailure setFailure)
    {
        std::shared_ptr<ITransport> transport;
        if (fade) {
            transport = std::make_shared<CrossfadeTransport>();
        }
        else {
            transport = std::make_shared<GaplessTransport>();
        }
        PlaybackService service(runtime.Queue(), LibraryFactory::Instance().DefaultLocalLibrary(), transport);
        auto playback = OpenFixturePlayback(service);
        std::promise<void> started;
        auto connection = transport->PlaybackEvent.Subscribe([&](auto state) {
            if (state == sdk::PlaybackState::Playing) {
                started.set_value();
            }
        });
        transport->Start(file.string(), {}, ITransport::StartMode::Immediate);
        if (started.get_future().wait_for(3s) != std::future_status::ready) {
            return false;
        }
        connection = {};

        setFailure(ControlFailure::Pause);
        if (playback.Control(MC_PLAYBACK_TOGGLE_PAUSE) != MC_ERROR_IO ||
            transport->GetPlaybackState() != sdk::PlaybackState::Playing)
        {
            std::fputs("Failed device pause was reported as a successful transition\n", stderr);
            return false;
        }
        setFailure(ControlFailure::None);
        if (playback.Control(MC_PLAYBACK_TOGGLE_PAUSE) != MC_OK) {
            return false;
        }
        setFailure(ControlFailure::Resume);
        if (playback.Control(MC_PLAYBACK_TOGGLE_PAUSE) != MC_ERROR_IO ||
            transport->GetPlaybackState() != sdk::PlaybackState::Paused)
        {
            std::fputs("Failed device resume was reported as a successful transition\n", stderr);
            return false;
        }
        setFailure(ControlFailure::None);
        return playback.Control(MC_PLAYBACK_TOGGLE_PAUSE) == MC_OK &&
            transport->GetPlaybackState() == sdk::PlaybackState::Playing;
    }

    /* read known PCM back from the output of a public URI player. the C
    decoder publishes EOF with its last samples. both a malformed empty block
    and a later read error must surface as failure rather than normal EOF. */
    bool checkDecoderPlayback(const std::filesystem::path& directory, ReadCapture read, WaitCapture wait) {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        api::Context context(options);
        api::Player player(context);

        for (const auto* name : { "samples.bytes", "empty-more.bytes", "read-error.bytes" }) {
            const bool complete = std::string_view(name) == "samples.bytes";
            const auto path = directory / name;
            constexpr size_t frameCount = 48001;
            {
                std::ofstream file(path, std::ios::binary);
                for (size_t frame = 0; frame < frameCount; ++frame) {
                    file.put(static_cast<char>(1 + frame % 127));
                }
                if (!file) {
                    return false;
                }
            }
            if (player.Play(path.string()) != MC_OK) {
                return false;
            }
            auto state = player.Snapshot().state;
            const auto deadline = Clock::now() + 5s;
            while (state != MC_PLAYER_FINISHED && state != MC_PLAYER_FAILED && Clock::now() < deadline) {
                std::this_thread::sleep_for(2ms);
                state = player.Snapshot().state;
            }
            if (state != (complete ? MC_PLAYER_FINISHED : MC_PLAYER_FAILED)) {
                std::fprintf(stderr, "%s ended in unexpected player state %d\n", name, static_cast<int>(state));
                return false;
            }
            if (complete) {
                std::vector<CapturedFrame> frames(read(nullptr, 0));
                read(frames.data(), frames.size());
                size_t index = 0;
                for (const auto& frame : frames) {
                    if (frame.underrun) {
                        continue;
                    }
                    const auto expected = static_cast<float>(1 + index % 127) / 256;
                    if (frame.left != expected || frame.right != expected) {
                        return false;
                    }
                    ++index;
                }
                if (index != frameCount) {
                    return false;
                }
            }
        }
        /* the fixture signals from inside the decoder's first input read.
        shutdown must wake that call and destroy its source before plugin unload. */
        if (player.Play(stalledDecoderInput) != MC_OK || !wait(CaptureEvent::SourceOpening, 3000)) {
            return false;
        }
        return context.Shutdown() == MC_OK && wait(CaptureEvent::SourceInterrupted, 500) &&
            wait(CaptureEvent::SourceReleased, 500);
    }

    /* keep an accepted token on the shared device while a decoder blocks.
    stopping the reader with Drain::Yes must let the device finish that token. */
    bool drainInterruptedDecoder(WaitCapture wait, HoldCompletion hold) {
        CoreRuntime runtime;
        auto output = outputs::SelectedOutput();
        struct Sink final : Output::CompletionSink {
            void CompleteBuffer(sdk::IBuffer*, mc_result result) override { outcome = result; }
            mc_result outcome = MC_ERROR_IN_FLIGHT;
        } sink;
        std::array<float, 2> samples {};
        Buffer tail(samples.data(), samples.size());
        tail.SetSampleRate(sampleRate);
        Output::Writer previous(*output, sink);
        Player::EventListener listener;
        PlaybackWorkers workers(listener);

        hold(true);
        if (output->Resume() != MC_OK || previous.TryQueueAudioBuffer(tail) != MC_OK) {
            hold(false);
            return false;
        }
        auto track = workers.Start(stalledDecoderInput, output, Player::Drain::Yes, {});
        track->Play();
        const bool blocked = wait(CaptureEvent::HeldCompletion, 3000) && wait(CaptureEvent::SourceOpening, 3000);
        track->RequestStop(Player::Drain::Yes);
        const bool draining = blocked && wait(CaptureEvent::Draining, 3000);
        hold(false);
        const bool balanced = wait(CaptureEvent::Balanced, 3000);
        previous.ProcessCompletions();
        if (!balanced || !draining || sink.outcome != MC_OK) {
            std::fprintf(stderr, "interrupted decoder lost output drain (blocked=%d, drain=%d, completion=%d)\n",
                blocked, draining, sink.outcome);
            return false;
        }
        return true;
    }

    /* URI players bypass the catalog playback service. an unsuccessful
    device command must reach this caller with its previous state intact. */
    bool checkUriControls(const std::filesystem::path& file, SetControlFailure setFailure,
        WaitCaptureFrames waitFrames)
    {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        api::Context context(options);
        api::Player player(context);
        const auto path = file.u8string();
        if (player.Play(std::string(path.begin(), path.end())) != MC_OK || !waitFrames(sampleRate / 4, 3000)) {
            std::fputs("URI player did not reach the capture device\n", stderr);
            return false;
        }

        setFailure(ControlFailure::Pause);
        if (player.Pause() != MC_ERROR_IO || player.Snapshot().state != MC_PLAYER_PLAYING) {
            std::fputs("URI player lost the device pause error or changed state\n", stderr);
            return false;
        }
        setFailure(ControlFailure::None);
        if (player.Pause() != MC_OK) {
            return false;
        }

        setFailure(ControlFailure::Resume);
        if (player.Resume() != MC_ERROR_IO || player.Snapshot().state != MC_PLAYER_PAUSED) {
            std::fputs("URI player lost the device resume error or changed state\n", stderr);
            return false;
        }
        setFailure(ControlFailure::None);
        return player.Resume() == MC_OK && player.Stop() == MC_OK;
    }

    /* a plugin can hold a write until Stop. the control thread must stay
    available to issue that Stop even if someone first asks it to pause. */
    bool stopBlockedControl(const std::filesystem::path& file, WaitCapture wait, void (*gateQueue)()) {
        GaplessTransport transport;
        transport.Start(file.string(), {}, ITransport::StartMode::Immediate);
        gateQueue();
        if (!wait(CaptureEvent::HeldRetiredQueue, 3000)) {
            std::fputs("Output never entered the blocked write\n", stderr);
            return false;
        }

        auto control = std::async(std::launch::async, [&] {
            const auto result = transport.Pause();
            transport.SetVolume(0.25);
            transport.Stop();
            return result;
        });
        if (control.wait_for(3s) != std::future_status::ready) {
            std::fputs("A blocked write made Pause and Stop unreachable\n", stderr);
            std::abort();
        }
        return control.get() == MC_ERROR_IN_FLIGHT &&
            transport.GetPlaybackState() == sdk::PlaybackState::Stopped;
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
                return false;
            }
            transport.Stop();
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
        WaitCapture wait, SetControlFailure setFailure, CoreRuntime& runtime, bool throughService, bool shortSource)
    {
        constexpr unsigned overlapDeadlineMs = 18000;
        auto transport = std::make_shared<CrossfadeTransport>();
        std::unique_ptr<PlaybackService> service;
        if (throughService) {
            service = std::make_unique<PlaybackService>(runtime.Queue(),
                LibraryFactory::Instance().DefaultLocalLibrary(), transport);
        }
        transport->Start(first.string(), {}, ITransport::StartMode::Immediate);
        if (!shortSource) {
            transport->PrepareNextTrack(second.string(), {});
        }
        const auto ready = shortSource ? CaptureEvent::Resumed : CaptureEvent::Overlap;
        if (!wait(ready, overlapDeadlineMs)) {
            std::fputs("Output never reached the requested playback state\n", stderr);
            return false;
        }
        if (!throughService) {
            /* a selected-device failure leaves the overlap alone. after the
            selected track accepts Pause, an uncooperative tail is stopped. */
            setFailure(ControlFailure::Pause);
            if (transport->Pause() != MC_ERROR_IO || !wait(CaptureEvent::BothOutputsPlaying, 0)) {
                return false;
            }
            setFailure(ControlFailure::PauseOutgoing);
            if (transport->Pause() != MC_OK ||
                transport->GetPlaybackState() != sdk::PlaybackState::Paused ||
                !wait(CaptureEvent::OnlyOneOutputPaused, 0))
            {
                std::fputs("Pausing the selected track did not retire the failing fade tail\n", stderr);
                return false;
            }
            setFailure(ControlFailure::None);
        }
        if (transport->Pause() != MC_OK) {
            std::fputs("Could not pause the output teardown fixture\n", stderr);
            return false;
        }

        /* give paused teardown its own deadline. plugin startup and paced audio
        must not consume the time reserved for detecting a shutdown deadlock. */
        std::mutex teardownMutex;
        std::condition_variable_any teardownChanged;
        std::jthread deadline([&](std::stop_token stop) {
            std::unique_lock lock(teardownMutex);
            teardownChanged.wait_for(lock, stop, 3s, [] { return false; });
            if (!stop.stop_requested()) {
                std::fputs("Paused crossfade teardown exceeded its deadline\n", stderr);
                std::abort();
            }
        });
        if (shortSource) {
            /* stopping a short/non-fadeable track must retain cancellation
            access until terminal cleanup has joined its retired player. */
            transport->Stop();
        }
        if (service) {
            /* managed shutdown joins producers before destroying transports.
            it must cancel paused fades while the transport is still alive. */
            service->Shutdown();
            if (Output::InstanceCount() != 0) {
                std::fputs("Playback shutdown retained a C output instance\n", stderr);
                return false;
            }
        }
        std::fputs("Destroying the transport with both fades paused\n", stderr);
        transport.reset();
        return true;
    }

    class GatedFadeTransport final : public CrossfadeTransport {
    public:
        ~GatedFadeTransport() override {
            this->ReleaseTick();
            this->Destroy();
        }

        double Volume() noexcept override {
            std::unique_lock lock(this->mutex);
            if (this->armed) {
                /* the fader reads volume while holding its context-list lock.
                retain that lock until the retired producer is blocked too. */
                this->entered = true;
                this->changed.notify_all();
                this->changed.wait(lock, [&] { return this->released; });
            }
            return CrossfadeTransport::Volume();
        }

        void Arm() {
            std::lock_guard lock(this->mutex);
            this->armed = true;
        }

        bool WaitForTick() {
            std::unique_lock lock(this->mutex);
            return this->changed.wait_for(lock, 3s, [&] { return this->entered; });
        }

        void ReleaseTick() {
            std::lock_guard lock(this->mutex);
            this->released = true;
            this->changed.notify_all();
        }

    private:
        std::mutex mutex;
        std::condition_variable changed;
        bool armed {};
        bool entered {};
        bool released {};
    };

    bool destroyBlockedRetiredCrossfade(const std::filesystem::path& first, const std::filesystem::path& second,
        WaitCapture wait, void (*gateQueue)(), CoreRuntime& runtime)
    {
        auto transport = std::make_shared<GatedFadeTransport>();
        PlaybackService service(runtime.Queue(), LibraryFactory::Instance().DefaultLocalLibrary(), transport);
        transport->Start(first.string(), {}, ITransport::StartMode::Immediate);
        if (!wait(CaptureEvent::Resumed, 3000)) {
            std::fputs("First crossfade output did not start\n", stderr);
            return false;
        }
        transport->Start(second.string(), {}, ITransport::StartMode::Immediate);
        if (!wait(CaptureEvent::SecondPrepared, 3000)) {
            std::fputs("Replacement crossfade output did not prepare\n", stderr);
            return false;
        }

        transport->Arm();
        if (!transport->WaitForTick()) {
            std::fputs("Crossfader did not enter the volume gate\n", stderr);
            std::abort();
        }
        gateQueue();
        if (!wait(CaptureEvent::HeldRetiredQueue, 3000)) {
            std::fputs("Retired output did not enter the producer gate\n", stderr);
            std::abort();
        }

        /* both sides of the cycle are now held: the fader owns its context
        lock and the retired producer owns its output's ordinary-call lock. */
        std::mutex mutex;
        std::condition_variable changed;
        bool destroyed = false;
        std::thread shutdown([&] {
            service.Shutdown();
            std::lock_guard lock(mutex);
            destroyed = true;
            changed.notify_all();
        });
        transport->ReleaseTick();
        {
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, 3s, [&] { return destroyed; })) {
                std::fputs("Retired crossfade shutdown waited on the fader before interrupting its output\n", stderr);
                std::abort();
            }
        }
        shutdown.join();
        return Output::InstanceCount() == 0;
    }

    bool failPreparedCrossfade(const std::filesystem::path& first, WaitCapture wait, void (*release)(),
        ReadCapture read, WaitCaptureFrames waitFrames)
    {
        std::mutex mutex;
        std::condition_variable changed;
        bool playing = false;
        CrossfadeTransport transport;
        auto connection = transport.PlaybackEvent.Subscribe([&](auto state) {
            if (state == sdk::PlaybackState::Playing) {
                std::lock_guard lock(mutex);
                playing = true;
                changed.notify_one();
            }
        });
        transport.Start(first.string(), {}, ITransport::StartMode::Immediate);
        {
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, 3s, [&] { return playing; })) {
                std::fputs("Current track never reached Playing\n", stderr);
                return false;
            }
        }
        transport.PrepareNextTrack(stalledSource, {});
        /* hold the next decoder in open until the active output has rendered
        another block, then fail that open and check playback continues. */
        const bool opening = wait(CaptureEvent::SourceOpening, 3000);
        const bool advancedWhileOpening = opening && waitFrames(read(nullptr, 0) + 2048, 3000);
        release();
        if (!advancedWhileOpening) {
            std::fputs("Active output stalled while preparing the next source\n", stderr);
            return false;
        }
        if (!wait(CaptureEvent::SourceReleased, 3000)) {
            std::fputs("Failed prepared source did not finish cleanup\n", stderr);
            return false;
        }
        if (transport.GetPlaybackState() != sdk::PlaybackState::Playing || transport.Uri() != first.string()) {
            std::fputs("Failure preparing the next crossfade source stopped the active track\n", stderr);
            return false;
        }
        const auto before = read(nullptr, 0);
        const bool continued = waitFrames(before + 2048, 3000);
        transport.Destroy();
        return continued && Output::InstanceCount() == 0;
    }

    bool seekWhileDraining(const std::filesystem::path& file, ReadCapture read,
        HoldCompletion hold, WaitCapture wait)
    {
        constexpr auto deadline = 3000;
        constexpr double seconds = 0.025;
        if (!writeWave(file, sampleRate / 10, ramp)) {
            return false;
        }
        GaplessTransport transport;
        std::mutex mutex;
        std::condition_variable changed;
        bool decodedEof = false;
        auto connection = transport.StreamEvent.Subscribe([&](auto state, const std::string&) {
            if (state == sdk::StreamState::AlmostDone) {
                std::lock_guard lock(mutex);
                decodedEof = true;
                changed.notify_all();
            }
        });
        hold(true);
        transport.Start(file.string(), {}, ITransport::StartMode::Immediate);
        {
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, 3s, [&] { return decodedEof; })) {
                hold(false);
                std::fputs("Short track never reached decoded EOF\n", stderr);
                return false;
            }
        }
        if (!wait(CaptureEvent::HeldCompletion, deadline)) {
            hold(false);
            std::fputs("Short track did not retain output at decoded EOF\n", stderr);
            return false;
        }
        const auto before = read(nullptr, 0);
        transport.SetPosition(seconds);
        if (!wait(CaptureEvent::Stop, deadline)) {
            hold(false);
            std::fputs("Accepted seek during AlmostDone never flushed output\n", stderr);
            return false;
        }
        hold(false);
        if (!wait(CaptureEvent::Resumed, deadline)) {
            std::fputs("Seek during AlmostDone never resumed PCM\n", stderr);
            return false;
        }
        std::vector<CapturedFrame> frames(read(nullptr, 0));
        read(frames.data(), frames.size());
        const auto expected = ramp(static_cast<size_t>(seconds * sampleRate));
        const bool correct = frames.size() > before &&
            std::abs(frames[before].left - static_cast<float>(expected[0]) / pcmScale) < sampleTolerance &&
            std::abs(frames[before].right - static_cast<float>(expected[1]) / pcmScale) < sampleTolerance;
        transport.Destroy();
        if (!correct) {
            std::fputs("Seek during AlmostDone returned stale PCM\n", stderr);
        }
        return correct;
    }

    /* a seek after prefetch reached EOF must restart both the decoder and
    the stream pool, including when the requested position is fractional. */
    bool seekAfterEof(const std::filesystem::path& file) {
        constexpr double seconds = 0.5;
        const auto correctFirstFrame = [](sdk::IBuffer& buffer) {
            const auto expected = ramp(sampleRate / 2);
            return buffer.Samples() >= 2 &&
                std::abs(buffer.BufferPointer()[0] - static_cast<float>(expected[0]) / pcmScale) < sampleTolerance &&
                std::abs(buffer.BufferPointer()[1] - static_cast<float>(expected[1]) / pcmScale) < sampleTolerance;
        };

        /* small slices leave part of FFmpeg's decoded block unconsumed, so
        seeking must discard scratch PCM even before either layer reaches EOF. */
        {
            auto buffered = Stream::Create(257, 0.05);
            if (!buffered->OpenStream(file.string(), nullptr) || buffered->Eof() ||
                buffered->SetPosition(seconds) != seconds)
            {
                std::fputs("Could not seek a stream with prefetched decoder PCM\n", stderr);
                return false;
            }
            auto* buffer = buffered->GetNextProcessedOutputBuffer();
            if (!buffer) {
                std::fputs("Seeking before EOF did not return PCM\n", stderr);
                return false;
            }
            const bool correct = correctFirstFrame(*buffer);
            buffered->OnBufferProcessedByPlayer(buffer);
            if (!correct) {
                std::fputs("Seeking before EOF returned stale or nonfractional PCM\n", stderr);
                return false;
            }
        }

        auto stream = Stream::Create();
        if (!stream->OpenStream(file.string(), nullptr)) {
            return false;
        }
        while (auto* buffer = stream->GetNextProcessedOutputBuffer()) {
            stream->OnBufferProcessedByPlayer(buffer);
        }
        if (!stream->Eof() || stream->SetPosition(seconds) != seconds) {
            std::fputs("Could not seek a stream after decoder EOF\n", stderr);
            return false;
        }
        auto* buffer = stream->GetNextProcessedOutputBuffer();
        if (!buffer) {
            std::fputs("Seeking after decoder EOF did not return PCM\n", stderr);
            return false;
        }
        const bool correct = correctFirstFrame(*buffer);
        stream->OnBufferProcessedByPlayer(buffer);
        if (!correct) {
            std::fputs("Seeking after EOF returned stale or nonfractional PCM\n", stderr);
        }
        return correct;
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
    if (argc == 2 && std::string_view(argv[1]) == "partial_media_create") {
        FatalScope fatal;
        CoreRuntime runtime;
        return Stream::Create()->OpenStream(stalledDecoderInput, nullptr) ? 0 : 1;
    }
    if (argc != 4) {
        return 1;
    }

    const std::string_view mode(argv[1]);
    const bool crossfade = mode == "crossfade";
    const bool stress = mode == "teardown" || mode == "seek" || mode == "completion" ||
        mode == "replace_next";
    const bool benchmark = mode == "benchmark";
    const bool seekEof = mode == "seek_eof";
    const bool seekTail = mode == "seek_tail";
    const bool shortPaused = mode == "paused_short_runtime";
    const bool pausedFade = mode == "paused_crossfade" || mode == "paused_runtime" || shortPaused;
    const bool sourceOpen = mode == "cancel_open";
    const bool outputFailure = mode == "output_queue_failure" || mode == "output_callback_failure";
    const bool controlFailure = mode == "gapless_control_failure" || mode == "crossfade_control_failure" ||
        mode == "uri_control_failure";
    const bool pausedReplacement = mode == "replace_paused_crossfade" || mode == "replace_stopped_crossfade" ||
        mode == "replace_failed_crossfade";
    const bool pausedSeek = mode == "seek_paused_crossfade";
    const bool blockedControl = mode == "blocked_control";
    const bool retiredFade = mode == "retired_crossfade";
    const bool failedNextFade = mode == "failed_next_crossfade";
    if (!crossfade && mode != "gapless" && mode != "decoder_contract" && mode != "decoder_drain" &&
        !seekEof && !seekTail && !stress && !benchmark && !pausedFade && !sourceOpen && !outputFailure && !retiredFade && !failedNextFade &&
        !controlFailure && !blockedControl && !pausedReplacement && !pausedSeek)
    {
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

    if (mode == "decoder_drain") {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto hold = moduleSymbol<HoldCompletion>(module.get(), "HoldPcmCompletion");
        return wait && hold && drainInterruptedDecoder(wait, hold) ? 0 : 1;
    }
    if (mode == "decoder_contract") {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        return wait && checkDecoderPlayback(argv[3], read, wait) ? 0 : 1;
    }
    const auto first = std::filesystem::path(argv[3]) / "first.wav";
    const auto second = std::filesystem::path(argv[3]) / "second.wav";
    const bool longTracks = crossfade || retiredFade || failedNextFade || pausedReplacement || pausedSeek ||
        (pausedFade && !shortPaused);
    if (!stress && !sourceOpen && !outputFailure && !prepareMedia(first, second, longTracks)) {
        std::fputs("Could not generate the PCM fixtures\n", stderr);
        return 1;
    }

    if (mode == "uri_control_failure") {
        const auto inject = moduleSymbol<SetControlFailure>(module.get(), "SetPcmControlFailure");
        const auto waitFrames = moduleSymbol<WaitCaptureFrames>(module.get(), "WaitPcmCaptureFrames");
        return inject && waitFrames && checkUriControls(first, inject, waitFrames) ? 0 : 1;
    }

    CoreRuntime runtime;
    musik::core::plugin::BindServices(&runtime.Queue(), nullptr, LibraryFactory::Instance().DefaultLocalLibrary());
    if (api::ReadOutputConfiguration().selectedDriver != driver) {
        std::fputs("Capture output was not selected\n", stderr);
        return 1;
    }

    if (controlFailure) {
        const auto inject = moduleSymbol<SetControlFailure>(module.get(), "SetPcmControlFailure");
        const bool crossfadeControls = mode == "crossfade_control_failure";
        if (crossfadeControls && (!checkCrossfadeOpenIntent(first, ITransport::StartMode::Immediate) ||
            !checkCrossfadeOpenIntent(first, ITransport::StartMode::Wait)))
        {
            std::fputs("Decoder readiness changed the requested playback state\n", stderr);
            return 1;
        }
        return inject && checkDeviceControls(first, crossfadeControls, runtime, inject) ? 0 : 1;
    }
    if (pausedReplacement) {
        const auto frames = moduleSymbol<WaitCaptureFrames>(module.get(), "WaitPcmCaptureFrames");
        auto action = PausedReplacement::Direct;
        if (mode == "replace_stopped_crossfade") {
            action = PausedReplacement::Stop;
        }
        else if (mode == "replace_failed_crossfade") {
            action = PausedReplacement::FailedOpen;
        }
        return frames && replacePausedCrossfade(first, second, action, read, frames) ? 0 : 1;
    }
    if (pausedSeek) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto frames = moduleSymbol<WaitCaptureFrames>(module.get(), "WaitPcmCaptureFrames");
        return wait && frames && seekPausedCrossfade(first, second, read, wait, frames) ? 0 : 1;
    }
    if (blockedControl) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto gate = moduleSymbol<void (*)()>(module.get(), "GateRetiredPcmQueue");
        return wait && gate && stopBlockedControl(first, wait, gate) ? 0 : 1;
    }
    if (seekEof) {
        return seekAfterEof(first) ? 0 : 1;
    }
    if (seekTail) {
        const auto hold = moduleSymbol<HoldCompletion>(module.get(), "HoldPcmCompletion");
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        return hold && wait && seekWhileDraining(first, read, hold, wait) ? 0 : 1;
    }

    if (benchmark) {
        const auto store = moduleSymbol<StoreCapture>(module.get(), "StorePcmCapture");
        const auto waitFrames = moduleSymbol<WaitCaptureFrames>(module.get(), "WaitPcmCaptureFrames");
        const auto audible = moduleSymbol<CountCapture>(module.get(), "AudiblePcmCapture");
        return store && waitFrames && audible && benchmarkPlayback(first, second, store, waitFrames, audible) ? 0 : 1;
    }

    if (failedNextFade) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto release = moduleSymbol<void (*)()>(module.get(), "ReleasePcmSourceOpen");
        const auto frames = moduleSymbol<WaitCaptureFrames>(module.get(), "WaitPcmCaptureFrames");
        return wait && release && frames && failPreparedCrossfade(first, wait, release, read, frames) ? 0 : 1;
    }

    if (retiredFade) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto gate = moduleSymbol<void (*)()>(module.get(), "GateRetiredPcmQueue");
        return wait && gate && destroyBlockedRetiredCrossfade(first, second, wait, gate, runtime) ? 0 : 1;
    }

    if (pausedFade) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto setFailure = moduleSymbol<SetControlFailure>(module.get(), "SetPcmControlFailure");
        return wait && setFailure &&
            destroyPausedCrossfade(first, second, wait, setFailure, runtime, mode != "paused_crossfade", shortPaused) ? 0 : 1;
    }

    if (outputFailure) {
        return failOutput(first, mode == "output_callback_failure") ? 0 : 1;
    }

    if (sourceOpen) {
        const auto wait = moduleSymbol<WaitCapture>(module.get(), "WaitPcmCapture");
        const auto release = moduleSymbol<void (*)()>(module.get(), "ReleasePcmSourceOpen");
        return wait && release && cancelSourceOpen(wait, release) ? 0 : 1;
    }

    if (stress) {
        if (mode == "completion") {
            return stopDuringNotification(first) ? 0 : 1;
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
