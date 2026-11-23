/* legacy hooks deliberately fail so cleanup must visit every provider and
phase. preference markers survive because modules remain process-owned here. */
#include <musikcore/abi.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IEncoderFactory.h>
#include <stdexcept>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>

namespace {
    musik::core::sdk::IPreferences* preferences = nullptr;
    bool tearingDown = false;

    /* use ordinary SDK preferences rather than exporting test-only core APIs. */
    void mark(int bit) {
        preferences->SetInt("cleanup_mask", preferences->GetInt("cleanup_mask", 0) | bit);
        if (std::getenv("MUSIKCORE_TEST_EXPECT_ABORT")) {
            std::fprintf(stderr, "fatal-test: cleanup-bit=%d\n", bit);
        }
    }

    /* acquisition/release counts also distinguish a cached reference from the
    indexer's independent decoder-factory reference. */
    void acquire(const char* key) {
        preferences->SetInt(key, preferences->GetInt(key, 0) + 1);
    }

    /* the encoder family still uses session-owned C++ factories. */
    class CodecFactory final : public musik::core::sdk::IEncoderFactory {
    public:
        musik::core::sdk::IEncoder* CreateEncoder(const char*) override { return nullptr; }
        bool CanHandle(const char*) const override { return false; }
        void Release() override {
            mark(64);
            acquire("encoder_releases");
        }
    } encoderFactory;

    /* static provider state is reset by each SetPreferences installation. */
    class FailurePlugin final : public musik::core::sdk::IPlugin {
    public:
        void Release() override {}
        const char* Name() override { return RUNTIME_FIXTURE_NAME; }
        const char* Version() override { return "1"; }
        const char* Author() override { return "musikcube tests"; }
        const char* Guid() override { return RUNTIME_FIXTURE_NAME; }
        bool Configurable() override { return false; }
        void Configure() override {}
        void Reload() override {}
        int SdkVersion() override { return musik::core::sdk::SdkVersion; }
    } plugin;
}

/* expose only the existing SDK entry points; no engine implementation is linked. */
extern "C" MC_PLUGIN_API musik::core::sdk::IPlugin* GetPlugin() { return &plugin; }

/* a fresh session must not inherit the preceding session's teardown markers. */
extern "C" MC_PLUGIN_API void SetPreferences(musik::core::sdk::IPreferences* value) {
    preferences = value;
    tearingDown = false;
    preferences->SetInt("cleanup_mask", 0);
    for (const auto* key : { "stream_acquisitions", "stream_releases", "decoder_acquisitions",
        "decoder_releases", "encoder_acquisitions", "encoder_releases" })
    {
        preferences->SetInt(key, 0);
    }
}

/* fail the first teardown phase on both modules, then verify all later phases. */
extern "C" MC_PLUGIN_API void SetMetadataProxy(void* proxy) {
    if (!proxy) {
        tearingDown = true;
        mark(1);
        if (preferences->GetBool("fail_teardown", false)) {
            throw std::runtime_error("deliberate metadata teardown failure");
        }
    }
}

/* later hooks still have to run after the preceding hook failed. */
extern "C" MC_PLUGIN_API void SetIndexerNotifier(void* notifier) {
    if (!notifier) {
        mark(2);
    }
}

/* optionally fail after playback/catalog creation to exercise startup rollback. */
extern "C" MC_PLUGIN_API void SetEnvironment(void* environment) {
    if (!environment) {
        mark(4);
    }
    else if (preferences->GetBool("fail_startup", false)) {
        throw std::runtime_error("deliberate startup failure");
    }
}

/* shutdown installs a null debug implementation, which is still a non-null pointer. */
extern "C" MC_PLUGIN_API void SetDebug(void*) {
    if (tearingDown) {
        mark(8);
    }
}

/* discovery keeps each factory until the managed session has quiesced. */
extern "C" MC_PLUGIN_API musik::core::sdk::IEncoderFactory* GetEncoderFactory() { acquire("encoder_acquisitions"); return &encoderFactory; }
