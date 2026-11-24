#include "../musikcore/runtime-hardening-cases.h"
#include "../../support/check.h"

#include <musikcore/audio/Buffer.h>
#include <musikcore/audio/Dsp.h>
#include <musikcore/audio/Stream.h>
#include <musikcore/paths.hpp>
#include <common/FilesystemPath.h>
#include <fstream>
#include <filesystem>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/musikcore.hpp>

#include <algorithm>
#include <cmath>

namespace runtime_tests {

    /* a failing plugin must not send its partly modified PCM to an output. */
    bool checkDspFailure() {
        using namespace musik::core;
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        api::Context context(options);
        if (!TEST_CHECK(plugin::CreateDsps().size() == 1)) {
            return false;
        }
        const auto filename = sdk::fs::pathFromUtf8(api::ReadPath(MC_PATH_DATA)) / "failure.bytes";
        std::filesystem::create_directories(filename.parent_path());
        std::ofstream(filename, std::ios::binary).write("12345678", 8);
        {
            auto stream = audio::Stream::Create(4, 1.0);
            if (!TEST_CHECK(stream->OpenStream(filename.string(), nullptr)) ||
                !TEST_CHECK(stream->GetNextProcessedOutputBuffer() == nullptr) ||
                !TEST_CHECK(stream->DecodeStatus() == MC_ERROR_PLUGIN))
            {
                return false;
            }
        }
        return TEST_CHECK(context.Close() == MC_OK);
    }

    /* two active playback processors must see a setting changed through the
    public API without being recreated. disabled audio must pass unchanged. */
    bool checkDspProcessing() {
        using namespace musik::core;

        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        api::Context context(options);
        if (!TEST_CHECK(mc_equalizer_set_enabled(false) == MC_OK)) {
            return false;
        }

        {
            auto first = plugin::CreateDsps();
            auto second = plugin::CreateDsps();
            if (!TEST_CHECK(first.size() == 1 && second.size() == 1)) {
                return false;
            }
            audio::Buffer buffer;
            buffer.SetChannels(2);
            buffer.SetSampleRate(44100);
            buffer.SetSamples(4096);
            auto* samples = buffer.BufferPointer();

            const auto fill = [&] {
                for (long i = 0; i < buffer.Samples(); ++i) {
                    samples[i] = static_cast<float>(0.2 * std::sin((i / 2) * 0.1));
                }
            };
            fill();
            const float before = samples[100];
            first.front()->Process(buffer);
            if (!TEST_CHECK(samples[100] == before)) {
                return false;
            }

            double gains[MC_EQUALIZER_BAND_COUNT];
            std::fill(std::begin(gains), std::end(gains), 15.0);
            if (!TEST_CHECK(mc_equalizer_set_gains(gains, MC_EQUALIZER_BAND_COUNT) == MC_OK) ||
                !TEST_CHECK(mc_equalizer_set_enabled(true) == MC_OK))
            {
                return false;
            }
            const auto changed = [&](const auto& dsp) {
                /* the equalizer has a filter delay. allow its tables to settle
                before comparing the samples from the existing instance. */
                for (int block = 0; block < 8; ++block) {
                    fill();
                    dsp->Process(buffer);
                }
                return std::abs(samples[100] - before) > 0.01f;
            };
            if (!TEST_CHECK(changed(first.front())) || !TEST_CHECK(changed(second.front()))) {
                return false;
            }

            if (!TEST_CHECK(mc_equalizer_set_enabled(false) == MC_OK)) {
                return false;
            }
            fill();
            second.front()->Process(buffer);
            if (!TEST_CHECK(samples[100] == before)) {
                return false;
            }
        }
        return TEST_CHECK(context.Close() == MC_OK);
    }
}
