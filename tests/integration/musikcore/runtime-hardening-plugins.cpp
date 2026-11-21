#include "runtime-hardening-cases.h"
#include <musikcore/musikcore.hpp>
#include "../../support/check.h"
#include <musikcore/audio/Streams.h>
#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/Preferences.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

namespace runtime_tests {
    using namespace musik::core;

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
        if (!TEST_CHECK(mc_visualizer_list(read, &count) == MC_OK && count == 1) ||
            !TEST_CHECK(mc_visualizer_show(0) == MC_OK) ||
            !TEST_CHECK(context.Close() == MC_OK) ||
            !TEST_CHECK(preferences->GetInt("hides") == 1 && preferences->GetInt("releases") == 1) ||
            !TEST_CHECK(mc_visualizer_hide() == MC_ERROR_NOT_FOUND))
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
        if (!TEST_CHECK(readMarker("CurrentSdk.log") == "G") ||
            !TEST_CHECK(readMarker("ObsoleteSdk.log") == "GRU"))
        {
            return false;
        }

        const auto path = std::filesystem::u8path(directory) / "not-opened-yet.bin";
        const auto encoded = path.u8string();
        const std::string filename(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        auto& environment = musik::core::plugin::Environment();
        /* own unopened streams too, including early assertion failures. */
        const auto release = [](sdk::IDataStream* stream) {
            if (stream) {
                stream->Release();
            }
        };
        std::unique_ptr<sdk::IDataStream, decltype(release)> source(
            environment.CreateDataStream(filename.c_str()), release);
        if (!TEST_CHECK(bool(source)) || !TEST_CHECK(!std::filesystem::exists(path))) {
            return false;
        }

        /* abort publication before any Open call, including writable streams. */
        bool published = false;
        const auto cancelled = io::DataStreamFactory::OpenSharedDataStream(filename.c_str(), sdk::OpenFlags::Write,
            [&](const auto& pending) {
                published = bool(pending);
                pending->Interrupt();
                return false;
            });
        if (!TEST_CHECK(published && !cancelled && !std::filesystem::exists(path))) {
            return false;
        }

        /* creation did not choose flags or touch disk; the caller opens its
        already-owned resource explicitly and retains normal write behavior. */
        const char bytes[] = "sdk23";
        if (!TEST_CHECK(source->Open(filename.c_str(), sdk::OpenFlags::Write)) ||
            !TEST_CHECK(source->Write((void*) bytes, sizeof(bytes)) == sizeof(bytes)) ||
            !TEST_CHECK(source->Close()) || !TEST_CHECK(std::filesystem::file_size(path) == sizeof(bytes)))
        {
            return false;
        }
        source.reset();

        /* nested adapters must release unopened streams and forward cancellation
        that arrives before their child is opened. successful Open first proves
        provider selection, rather than accidentally testing a local-path fallback. */
        const auto checkNested = [&](const char* prefix, const char* name) -> bool {
            const auto child = std::filesystem::u8path(directory) / name;
            std::filesystem::copy_file(path, child);
            const auto childEncoded = child.u8string();
            const std::string uri = std::string(prefix) + "://0/" +
                std::string(reinterpret_cast<const char*>(childEncoded.data()), childEncoded.size());
            std::unique_ptr<sdk::IDataStream, decltype(release)> unopened(
                environment.CreateDataStream(uri.c_str()), release);
            if (!TEST_CHECK(bool(unopened))) {
                return false;
            }
            unopened.reset();
            std::unique_ptr<sdk::IDataStream, decltype(release)> opened(
                environment.CreateDataStream(uri.c_str()), release);
            if (!TEST_CHECK(opened && opened->Open(uri.c_str(), sdk::OpenFlags::Read)) ||
                !TEST_CHECK(opened->Close()))
            {
                return false;
            }
            opened.reset();
            std::unique_ptr<sdk::IDataStream, decltype(release)> interrupted(
                environment.CreateDataStream(uri.c_str()), release);
            if (!TEST_CHECK(bool(interrupted))) {
                return false;
            }
            interrupted->Interrupt();
            return TEST_CHECK(!interrupted->Open(uri.c_str(), sdk::OpenFlags::Read));
        };
#ifdef TEST_STREAM_gmedecoder
        if (!checkNested("gme", "nested.spc")) {
            return false;
        }
#endif
#ifdef TEST_STREAM_openmptdecoder
        if (!checkNested("libopenmpt", "nested.mod")) {
            return false;
        }
#endif
        return TEST_CHECK(context.Close() == MC_OK);
    }

    /* every provider must lose all host pointers despite a throwing peer.
    shutdown reports failure, drains requests, and leaves destruction retryable. */
    bool checkFailure(bool caches) {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        auto first = Preferences::ForPlugin("RuntimeFailureFirst");
        auto second = Preferences::ForPlugin("RuntimeFailureSecond");
        {
            unsigned completions = 0;
            mc_result completed = MC_ERROR_BUSY;
            api::Context context(options);
            auto playback = context.GetPlayback();
            if (caches) {
                /* exercise concurrent first lookup; no fixture accepts the URI. */
                const auto path = std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "uncreated-runtime-stream";
                const auto text = path.u8string();
                const std::string uri(reinterpret_cast<const char*>(text.data()), text.size());
                std::vector<std::thread> readers;
                for (unsigned reader = 0; reader < 4; ++reader) {
                    readers.emplace_back([&] {
                        const auto stream = io::DataStreamFactory::OpenSharedDataStream(uri.c_str(), sdk::OpenFlags::Read);
                    });
                }
                for (auto& reader : readers) {
                    reader.join();
                }
                audio::streams::GetEncoderForType("runtime-fixture-unsupported-codec");
            }
            api::OpenTrackSearch(context, {}, [&](mc_result status, api::TrackResult, std::string_view) {
                ++completions;
                completed = status;
            });
            const int expectedMask = caches ? 127 : 15;
            if (!TEST_CHECK(context.Shutdown() == MC_ERROR_INTERNAL) ||
                !TEST_CHECK((first->GetInt("cleanup_mask") & expectedMask) == expectedMask) ||
                !TEST_CHECK((second->GetInt("cleanup_mask") & expectedMask) == expectedMask) ||
                !TEST_CHECK(completions == 1 && (completed == MC_OK || completed == MC_ERROR_CANCELLED)) ||
                !TEST_CHECK(!playback.Snapshot()) ||
                !TEST_CHECK(context.Close() == MC_OK))
            {
                return false;
            }
        }
        if (caches) {
            for (const auto& preferences : { first, second }) {
                for (const std::string kind : { "stream", "decoder", "encoder" }) {
                    if (!TEST_CHECK(preferences->GetInt(kind + "_acquisitions") > 0) ||
                        !TEST_CHECK(preferences->GetInt(kind + "_acquisitions") ==
                            preferences->GetInt(kind + "_releases")))
                    {
                        return false;
                    }
                }
            }
        }
        /* a failed startup must release the process slot even if rollback hooks fail. */
        first->SetBool("fail_startup", true);
        mc_context* failed = nullptr;
        const auto result = mc_context_create(&options, &failed, nullptr);
        first->SetBool("fail_startup", false);
        if (!TEST_CHECK(result == MC_ERROR_INTERNAL && !failed)) {
            return false;
        }
        api::Context reopened(options);
        return TEST_CHECK(reopened.Shutdown() == MC_ERROR_INTERNAL) &&
            TEST_CHECK(reopened.Close() == MC_OK);
    }
}
