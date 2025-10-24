#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IDecoderFactory.h>
#include "DiscordRichPresence.h"

#ifdef WIN32
    #define DLLEXPORT __declspec(dllexport)
#else
    #define DLLEXPORT
#endif

class DiscordRichPresencePlugin : public musik::core::sdk::IPlugin {
    public:
        DiscordRichPresencePlugin() { }
        void Release() override { };
        const char* Name() override { return "Discord Rich Presence"; }
        const char* Version() override { return MUSIKCUBE_VERSION_WITH_COMMIT_HASH; }
        const char* Author() override { return "pyrokn8"; }
        const char* Guid() override { return "e214ab47-8f47-472d-b5d5-c6950e9762c8"; }
        bool Configurable() override { return false; }
        void Configure() override { }
        void Reload() override { }
        int SdkVersion() override { return musik::core::sdk::SdkVersion; }
};

extern "C" DLLEXPORT musik::core::sdk::IPlugin* GetPlugin() {
    return new DiscordRichPresencePlugin();
}