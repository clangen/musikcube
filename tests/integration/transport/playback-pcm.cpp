#include "CoreRuntime.h"
#include "PcmCapture.h"

#include <musikcore/audio/CrossfadeTransport.h>
#include <musikcore/audio/GaplessTransport.h>
#include <musikcore/output.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    using namespace musik::core;
    using namespace musik::core::audio;
    using namespace playback_fixture;
    using namespace std::chrono_literals;
    using Clock = std::chrono::steady_clock;

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
}

int main(int argc, char** argv) {
    if (argc != 4) {
        return 1;
    }

    const std::string_view mode(argv[1]);
    const bool crossfade = mode == "crossfade";
    if (!crossfade && mode != "gapless") {
        return 1;
    }

    const auto pluginPath = std::filesystem::canonical(argv[0]).parent_path() / "plugins" / argv[2];
    const std::unique_ptr<void, decltype(&dlclose)> module { dlopen(pluginPath.c_str(), RTLD_NOW), dlclose };
    if (!module) {
        std::fprintf(stderr, "Could not load capture output: %s\n", dlerror());
        return 1;
    }
    const auto read = reinterpret_cast<ReadCapture>(dlsym(module.get(), "ReadPcmCapture"));
    if (!read) {
        std::fprintf(stderr, "Could not find PCM capture reader: %s\n", dlerror());
        return 1;
    }

    const auto first = std::filesystem::path(argv[3]) / "first.wav";
    const auto second = std::filesystem::path(argv[3]) / "second.wav";
    if (!prepareMedia(first, second, crossfade)) {
        std::fputs("Could not generate the PCM fixtures\n", stderr);
        return 1;
    }

    CoreRuntime runtime;
    musik::core::plugin::BindServices(&runtime.Queue(), nullptr, LibraryFactory::Instance().DefaultLocalLibrary());
    if (api::ReadOutputConfiguration().selectedDriver != driver) {
        std::fputs("Capture output was not selected\n", stderr);
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

    std::vector<CapturedFrame> frames(read(nullptr, 0));
    read(frames.data(), frames.size());
    return (crossfade ? checkCrossfade(frames) : checkGapless(frames)) ? 0 : 1;
}
