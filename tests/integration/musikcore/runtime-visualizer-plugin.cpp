/* headless legacy visualizer for session-lifetime assertions. visibility and
release counts use ordinary provider preferences; no window or audio device. */
#include <musikcore/abi.h>
#include <musikcore/sdk/IPcmVisualizer.h>
#include <musikcore/sdk/IPreferences.h>
#include <musikcore/sdk/constants.h>

namespace {
    musik::core::sdk::IPreferences* preferences = nullptr;

    /* retain observations across sessions so missing teardown cannot be hidden
    by installing preferences again. */
    void increment(const char* key) {
        preferences->SetInt(key, preferences->GetInt(key, 0) + 1);
    }

    /* exposes only identity; all behavior belongs to the acquired visualizer. */
    class Plugin final : public musik::core::sdk::IPlugin {
        public:
            /* modules remain process-owned; the descriptor owns this static object. */
            void Release() override {}
            /* identify the fixture without relying on a platform-specific filename. */
            const char* Name() override { return "RuntimeVisualizer"; }
            /* supply stable discovery metadata for the legacy host. */
            const char* Version() override { return "1"; }
            /* identify the fixture author without external resources. */
            const char* Author() override { return "musikcube tests"; }
            /* keep preferences under a distinct provider identity. */
            const char* Guid() override { return "RuntimeVisualizer"; }
            /* this fixture has no configuration UI. */
            bool Configurable() override { return false; }
            /* discovery must never open a UI. */
            void Configure() override {}
            /* enablement affects the next session, with nothing to reload here. */
            void Reload() override {}
            /* use the supported legacy version so discovery reaches the test. */
            int SdkVersion() override { return musik::core::sdk::SdkVersion; }
    } plugin;

    /* models provider state that must retire even when Playback was never
    requested. each discovery owns a distinct instance. */
    class Visualizer final : public musik::core::sdk::IPcmVisualizer {
        public:
            /* record retirement separately from hiding the selected instance. */
            void Release() override {
                increment("releases");
                delete this;
            }
            /* show changes only an in-memory flag; it cannot touch the desktop. */
            void Show() override { this->visible = true; }
            /* expose automatic shutdown hiding without needing UI synchronization. */
            void Hide() override {
                increment("hides");
                this->visible = false;
            }
            /* let the public hide API distinguish shutdown from a surviving window. */
            bool Visible() override { return this->visible; }
            /* provide the public registry's stable display label. */
            const char* Name() override { return "Headless runtime visualizer"; }
            /* no audio is produced; this test exercises lifecycle only. */
            void Write(musik::core::sdk::IBuffer*) override {}

        private:
            bool visible = false;
    };
}

/* expose the same entry points a separately built legacy plugin uses. */
extern "C" MC_PLUGIN_API musik::core::sdk::IPlugin* GetPlugin() {
    return &plugin;
}

/* observe lifecycle through provider-owned preferences without private exports. */
extern "C" MC_PLUGIN_API void SetPreferences(musik::core::sdk::IPreferences* value) {
    preferences = value;
}

/* discovery transfers an owned reference that shutdown must release. */
extern "C" MC_PLUGIN_API musik::core::sdk::IPcmVisualizer* GetPcmVisualizer() {
    return new Visualizer();
}
