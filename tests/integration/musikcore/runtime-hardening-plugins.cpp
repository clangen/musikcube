#include "runtime-hardening-cases.h"
#include <musikcore/musikcore.hpp>
#include "../../support/check.h"
#include <musikcore/audio/Outputs.h>
#include <musikcore/audio/Dsp.h>
#include <musikcore/plugin/media.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/Preferences.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <exception>
#include <iterator>
#include <thread>

namespace runtime_tests {
    using namespace musik::core;

#ifdef _WIN32
    bool checkWindowsPlugins() {
        const std::string_view modules[] = {
            "nullout.dll", "directsoundout.dll", "wasapiout.dll", "waveout.dll",
            "win32gdivis.dll", "win32globalhotkeys.dll", "cddadecoder.dll",
        };
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;

        /* exercise actual DLL discovery and persisted state across plugin sessions. */
        const auto check = [&](bool enabled, bool nextEnabled) -> bool {
            api::Context context(options);
            const auto plugins = api::ListPlugins();
            /* check C output discovery alongside the legacy DLL catalog. */
            {
                const auto outputs = audio::outputs::GetAllOutputs();
                if (!TEST_CHECK(outputs.size() == (enabled ? 2u : 0u))) {
                    return false;
                }
                if (enabled && !TEST_CHECK(std::string_view(outputs.front()->Name()) == "Null Output")) {
                    return false;
                }
                if (enabled && !TEST_CHECK(std::string_view(outputs.back()->Name()) == "WASAPI")) {
                    return false;
                }
                const auto visualizers = api::ListVisualizers();
                if (!TEST_CHECK((std::find(visualizers.begin(), visualizers.end(), "GdiVis") != visualizers.end()) == enabled)) {
                    return false;
                }
            }
            for (const auto module : modules) {
                const auto matches = [module](const api::PluginInfo& item) {
                    return std::filesystem::path(item.filename).filename().string() == module;
                };
                const auto plugin = std::find_if(plugins.begin(), plugins.end(), matches);
                if (plugin == plugins.end() || plugin->id.empty() || plugin->version.empty() ||
                    plugin->enabled != enabled || std::count_if(plugins.begin(), plugins.end(), matches) != 1)
                {
                    std::fprintf(stderr, "Unexpected Windows plugin catalog entry for %s\n", module.data());
                    return false;
                }
                if (enabled != nextEnabled &&
                    !TEST_CHECK(api::SetPluginEnabled(plugin->filename, nextEnabled) == MC_OK))
                {
                    return false;
                }
            }
            return TEST_CHECK(context.Close() == MC_OK);
        };
        return check(true, false) && check(false, true) && check(true, true);
    }
#endif

    /* rejecting a directory before first discovery must not pin the default
    directory as a side effect of rollback. a later valid custom root must work. */
    bool checkPluginDirectory() {
        const auto directory = std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "custom-plugins";
        const auto encoded = directory.u8string();
        const std::string path(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        options.plugin_directory = { path.data(), path.size() };
        mc_context* failed = nullptr;
        if (!TEST_CHECK(mc_context_create(&options, &failed, nullptr) == MC_ERROR_INVALID_ARGUMENT && !failed)) {
            return false;
        }
        std::filesystem::create_directories(directory);
        api::Context recovered(options);
        return TEST_CHECK(recovered.Services() == MC_SERVICE_PLUGINS) && TEST_CHECK(recovered.Close() == MC_OK);
    }

    /* public visualizer APIs work with Plugins alone. session shutdown must hide
    and release providers before a disabled replacement session rediscovers them. */
    bool checkVisualizerSession() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        auto preferences = Preferences::ForPlugin("RuntimeVisualizer");
        api::Context context(options);
        size_t count = 0;
        const auto read = [](void* data, const mc_string_view*, size_t size) {
            *static_cast<size_t*>(data) = size;
        };
        bool visible = true;
        if (!TEST_CHECK(mc_visualizer_is_visible(nullptr) == MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(mc_visualizer_is_visible(&visible) == MC_OK && !visible) ||
            !TEST_CHECK(mc_visualizer_list(read, &count) == MC_OK && count == 1) ||
            !TEST_CHECK(mc_visualizer_show(0) == MC_OK) ||
            !TEST_CHECK(mc_visualizer_is_visible(&visible) == MC_OK && visible) ||
            !TEST_CHECK(context.Close() == MC_OK) ||
            !TEST_CHECK(preferences->GetInt("hides") == 1 && preferences->GetInt("releases") == 1) ||
            !TEST_CHECK(mc_visualizer_hide() == MC_ERROR_NOT_FOUND) ||
            !TEST_CHECK(mc_visualizer_is_visible(&visible) == MC_OK && !visible))
        {
            return false;
        }
        const auto plugins = api::ListPlugins();
        const auto plugin = std::find_if(plugins.begin(), plugins.end(), [](const auto& item) {
            return item.name == "RuntimeVisualizer";
        });
        if (!TEST_CHECK(plugin != plugins.end()) || !TEST_CHECK(api::SetPluginEnabled(plugin->filename, false) == MC_OK)) {
            return false;
        }
        api::Context reopened(options);
        count = 99;
        return TEST_CHECK(mc_visualizer_list(read, &count) == MC_OK && count == 0) &&
            TEST_CHECK(mc_visualizer_show(0) == MC_ERROR_NOT_FOUND);
    }

    /* validate the SDK break through actual module discovery, then exercise
    create-before-open ownership through the environment and shared core path. */
    bool checkStreamSdkContract() {
        const auto directory = api::ReadPath(MC_PATH_DATA);
#ifdef WIN32
        _putenv_s("MUSIKCUBE_SDK_TEST_MARKERS", directory.c_str());
#else
        setenv("MUSIKCUBE_SDK_TEST_MARKERS", directory.c_str(), 1);
#endif
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        api::Context context(options);
        /* unload observations survive after the obsolete module is gone. */
        const auto readMarker = [&](const char* name) {
            std::ifstream file(std::filesystem::u8path(directory) / name);
            return std::string(std::istreambuf_iterator<char>(file), {});
        };
        /* require one object released before unload. additional native probes
        may unload the image without creating another object. */
        const auto obsolete = readMarker("ObsoleteSdk.log");
        const bool releasedBeforeUnload = obsolete.size() >= 3 && obsolete.substr(0, 3) == "GRU" &&
            obsolete.find_first_not_of('U', 3) == std::string::npos;
        if (!TEST_CHECK(readMarker("CurrentSdk.log") == "G") || !TEST_CHECK(releasedBeforeUnload))
        {
            return false;
        }

        const auto path = std::filesystem::u8path(directory) / "not-opened-yet.bin";
        const auto encoded = path.u8string();
        const std::string filename(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        const auto& media = musik::core::plugin::Environment().Media();
        /* invalid encoder requests must fail before any factory can create one. */
        mc_encoder invalid = MC_ENCODER_DEFAULT;
        if (!TEST_CHECK(media.create_encoder(media.context, { nullptr, 0 }, &invalid, nullptr) ==
                MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(media.create_encoder(media.context, { ".mp3", 4 }, nullptr, nullptr) ==
                MC_ERROR_INVALID_ARGUMENT) ||
            !TEST_CHECK(media.create_encoder(media.context, { nullptr, 4 }, &invalid, nullptr) ==
                MC_ERROR_INVALID_ARGUMENT))
        {
            return false;
        }
        const mc_string_view pathView { filename.data(), filename.size() };
        const auto writeFlags = MC_STREAM_OPEN_WRITE | MC_STREAM_OPEN_CREATE | MC_STREAM_OPEN_TRUNCATE;
        mc_stream source = MC_STREAM_DEFAULT;
        if (!TEST_CHECK(media.create_stream(media.context, pathView, &source, nullptr) == MC_OK)) {
            return false;
        }
        /* Stop before open must prevent side effects even for writable media. */
        source.api->interrupt(source.instance);
        auto status = source.api->open(source.instance, pathView, writeFlags, nullptr);
        source.api->destroy(source.instance);
        if (!TEST_CHECK(status == MC_ERROR_CANCELLED && !std::filesystem::exists(path))) {
            return false;
        }

        source = MC_STREAM_DEFAULT;
        if (!TEST_CHECK(media.create_stream(media.context, pathView, &source, nullptr) == MC_OK)) {
            return false;
        }
        const char bytes[] = "media";
        size_t written = 0;
        /* an encoder's writable sink must not be probed as a decoder input. */
        mc_decoder codec = MC_DECODER_DEFAULT;
        const bool saved = source.api->open(source.instance, pathView, writeFlags, nullptr) == MC_OK &&
            media.create_decoder(media.context, &source, &codec, nullptr) == MC_ERROR_UNSUPPORTED &&
            !codec.instance && source.api->write(source.instance, bytes, sizeof(bytes), &written, nullptr) == MC_OK &&
            written == sizeof(bytes) && source.api->close(source.instance) == MC_OK;
        source.api->destroy(source.instance);
        if (!TEST_CHECK(saved && std::filesystem::file_size(path) == sizeof(bytes))) {
            return false;
        }

        /* exercise nested factory selection and interruption before child open;
        a local-file fallback cannot open the virtual subsong URI. */
        const auto checkNested = [&](const char* prefix, const char* name) -> bool {
            const auto child = std::filesystem::u8path(directory) / name;
            std::filesystem::copy_file(path, child);
            const auto childEncoded = child.u8string();
            const std::string uri = std::string(prefix) + "://0/" +
                std::string(reinterpret_cast<const char*>(childEncoded.data()), childEncoded.size());
            const mc_string_view view { uri.data(), uri.size() };
            for (bool cancel : { false, true }) {
                mc_stream nested = MC_STREAM_DEFAULT;
                if (!TEST_CHECK(media.create_stream(media.context, view, &nested, nullptr) == MC_OK)) {
                    return false;
                }
                if (cancel) {
                    nested.api->interrupt(nested.instance);
                }
                const auto opened = nested.api->open(nested.instance, view, MC_STREAM_OPEN_READ, nullptr);
                nested.api->destroy(nested.instance);
                if (!TEST_CHECK(opened == (cancel ? MC_ERROR_CANCELLED : MC_OK))) {
                    return false;
                }
            }
            return true;
        };
#ifdef TEST_STREAM_gmedecoder
        if (!TEST_CHECK(bool(musik::core::plugin::FindIndexerSourceFactory("org.musikcube.decoder.gme"))) ||
            !checkNested("gme", "nested.spc")) {
            return false;
        }
#endif
#ifdef TEST_STREAM_openmptdecoder
        if (!TEST_CHECK(bool(musik::core::plugin::FindIndexerSourceFactory("org.musikcube.decoder.openmpt"))) ||
            !checkNested("libopenmpt", "nested.mod")) {
            return false;
        }
#endif
        return TEST_CHECK(context.Close() == MC_OK);
    }

    /* healthy sessions retire providers; failed host detachment must
    abort before visiting later hooks or releasing playback/catalog owners. */
    bool checkFailure(bool caches) {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        auto first = Preferences::ForPlugin("RuntimeFailureFirst");
        auto second = Preferences::ForPlugin("RuntimeFailureSecond");
        first->SetBool("fail_teardown", !caches);
        second->SetBool("fail_teardown", !caches);
        {
            unsigned completions = 0;
            mc_result completed = MC_ERROR_BUSY;
            api::Context context(options);
            auto playback = context.GetPlayback();
            if (caches) {
                const auto& media = musik::core::plugin::Environment().Media();
                mc_encoder encoder = MC_ENCODER_DEFAULT;
                constexpr char format[] = "runtime-fixture-unsupported-codec";
                if (!TEST_CHECK(media.create_encoder(media.context, { format, sizeof(format) - 1 }, &encoder,
                        nullptr) == MC_ERROR_UNSUPPORTED)) {
                    return false;
                }
            }
            api::OpenTrackSearch(context, {}, [&](mc_result status, api::TrackResult, std::string_view) {
                ++completions;
                completed = status;
            });
            const int expectedMask = 15;
            if (!TEST_CHECK(context.Shutdown() == MC_OK) ||
                !TEST_CHECK((first->GetInt("cleanup_mask") & expectedMask) == expectedMask) ||
                !TEST_CHECK((second->GetInt("cleanup_mask") & expectedMask) == expectedMask) ||
                !TEST_CHECK(completions == 1 && (completed == MC_OK || completed == MC_ERROR_CANCELLED)) ||
                !TEST_CHECK(!playback.Snapshot()) ||
                !TEST_CHECK(context.Close() == MC_OK))
            {
                return false;
            }
        }
        /* a failed startup releases the process slot after successful rollback.
        unsafe rollback instead follows the fatal detachment subprocess above. */
        first->SetBool("fail_startup", true);
        mc_context* failed = nullptr;
        const auto result = mc_context_create(&options, &failed, nullptr);
        first->SetBool("fail_startup", false);
        if (!TEST_CHECK(result == MC_ERROR_INTERNAL && !failed)) {
            return false;
        }
        api::Context reopened(options);
        return TEST_CHECK(reopened.Shutdown() == MC_OK) &&
            TEST_CHECK(reopened.Close() == MC_OK);
    }
}
