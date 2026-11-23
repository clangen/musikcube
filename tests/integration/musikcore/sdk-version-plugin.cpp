/* version-boundary fixture: the old factory must never be queried, and every
rejected plugin object must be released before its module is unloaded. */
#include <musikcore/abi.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IDataStreamFactory.h>
#include <musikcore/sdk/constants.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {
    /* keep observations outside module storage so rejection/unload is visible. */
    void mark(char phase) {
        const auto* directory = std::getenv("MUSIKCUBE_SDK_TEST_MARKERS");
        if (directory) {
            const auto file = std::filesystem::u8path(directory) / (std::string(SDK_FIXTURE_NAME) + ".log");
            std::ofstream(file, std::ios::app) << phase;
        }
    }

#ifdef __linux__
    /* Use the ELF finalizer so Zig builds record the dlclose boundary. */
    __attribute__((destructor)) void markUnload() { mark('U'); }
#else
    /* an unloaded module cannot report counters through an exported function. */
    struct UnloadMarker {
        /* observe unload after the rejected object's Release call. */
        ~UnloadMarker() { mark('U'); }
    } unloaded;
#endif

    /* identity stays ABI-compatible; the version prevents querying the changed
    stream-factory vtable. each GetPlugin call creates one owned object. */
    class Plugin final : public musik::core::sdk::IPlugin {
        public:
            /* rejection must release this object while its code is still loaded. */
            void Release() override {
                mark('R');
                delete this;
            }

            /* identify the compiled fixture variant without extra exports. */
            const char* Name() override { return SDK_FIXTURE_NAME; }

            /* provide ordinary discovery metadata. */
            const char* Version() override { return "1"; }

            /* mark this as repository-owned test code. */
            const char* Author() override { return "musikcube tests"; }

            /* isolate each fixture's provider preferences. */
            const char* Guid() override { return SDK_FIXTURE_NAME; }

            /* no desktop configuration is needed. */
            bool Configurable() override { return false; }

            /* never open UI from a test fixture. */
            void Configure() override {}

            /* this fixture has no reloadable resources. */
            void Reload() override {}

            /* SDK 22 used the incompatible factory Open/environment GetDataStream. */
            int SdkVersion() override {
#ifdef SDK_FIXTURE_OBSOLETE
                constexpr int obsoleteSdkVersion = musik::core::sdk::SdkVersion - 1;
                return obsoleteSdkVersion;
#else
                return musik::core::sdk::SdkVersion;
#endif
            }
    };
}

/* count factory entry so a duplicate GetPlugin call cannot silently leak. */
extern "C" MC_PLUGIN_API musik::core::sdk::IPlugin* GetPlugin() {
    mark('G');
    return new Plugin();
}

/* querying an old factory is an ABI violation, even before any virtual call. */
extern "C" MC_PLUGIN_API musik::core::sdk::IDataStreamFactory* GetDataStreamFactory() {
#ifdef SDK_FIXTURE_OBSOLETE
    std::abort();
#else
    return nullptr;
#endif
}
