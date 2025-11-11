#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IDecoderFactory.h>
#include "DiscordRichPresence.h"
#include <thread>

#ifdef WIN32
    #define DLLEXPORT __declspec(dllexport)
#else
    #define DLLEXPORT
#endif

using namespace musik::core::sdk;

static IDebug* debug = nullptr;

class DiscordRichPresencePlugin : public IPlugin {
    public:
        DiscordRichPresencePlugin() {
            if (debug) {
                debug->Info("DiscordRichPresencePlugin", "Discord Rich Presence Plugin initialized. :)");
            }
            init_discord();
            const char* track = "Bohemian Rhapsody";
            const char* artist = "Queen";
            const char* album = "A Night at the Opera";
            const char* cover_url = "https://0x0.st/KuY6.png";// Example cover URL
            int duration_seconds = 354; // 5:54
            update_presence(track, artist, album, cover_url, duration_seconds);
            std::thread t1(keep_connection_alive);
            t1.detach(); 
            // keep_connection_alive();
        }
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

extern "C" DLLEXPORT void SetDebug(IDebug* debug) {
    ::debug = debug;
}

extern "C" DLLEXPORT IPlugin* GetPlugin() {
    return new DiscordRichPresencePlugin();
}