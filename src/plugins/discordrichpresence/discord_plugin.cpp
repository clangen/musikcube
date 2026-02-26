#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/sdk/ITrack.h>
#include <musikcore/sdk/IValue.h>
#include <musikcore/sdk/IEnvironment.h>
// #include <musikcore/audio/Player.h>

#include <thread>
#include <string>
#include <windows.h>

#include "DiscordRichPresence.h"

#ifdef WIN32
    #define DLLEXPORT __declspec(dllexport)
#else
    #define DLLEXPORT
#endif

#ifdef __APPLE__
    extern __thread char threadLocalBuffer[4096];
#else
    extern thread_local char threadLocalBuffer[4096];
#endif

#define TAG "DiscordRichPresencePlugin"

using namespace musik::core::sdk;

static IDebug* debug = nullptr;
static IEnvironment* environment = nullptr;
static IPlaybackService* playbacks = nullptr;

// template <typename MetadataT>
// static std::string GetMetadataString(MetadataT* metadata, const std::string& key, const std::string& defaultValue = "missing metadata!") {
//     if (!metadata) { return defaultValue; }
//     metadata->GetString(key.c_str(), threadLocalBuffer, sizeof(threadLocalBuffer));
//     return std::string(threadLocalBuffer);
// }

// static std::string GetValueString(musik::core::sdk::IValue* value, const std::string& defaultValue = "missing metadata!") {
//     if (!value) { return defaultValue; }
//     value->GetValue(threadLocalBuffer, sizeof(threadLocalBuffer));
//     return std::string(threadLocalBuffer);
// }

std::string getThumbnailPath(int albumId) {
    char pathBuffer[4096];
    if (environment) {
        environment->GetPath(PathType::Library, pathBuffer, sizeof(pathBuffer));
    }

    if (strlen(pathBuffer) > 2) {
        return std::string(pathBuffer) + "thumbs/" + std::to_string(albumId) + ".jpg";
    } else {
        return "unknown";
    }
}

void change_presence_example() {
    if (!environment || !debug) return;
    const char* path = getThumbnailPath(3).c_str();
    debug->Info(TAG, path);
    const char* url = upload_cover_image(path);
    debug->Info(TAG, url);
    debug->Info(TAG, "hi there!");
    Sleep(30000);
    // update_presence("Don't Stop Believin'", "Journey", "Escape", "https://0x0.st/KSur.gif", 251);
    update_presence("Don't Stop Believin'", "Journey", "Escape", url, 251);
}

class DiscordRichPresencePlugin : public IPlugin {
    public:
        DiscordRichPresencePlugin() {
            bool opened = init_discord();
            if (opened) {
                update_presence("unknown", "unknown", "unknown", "unknown", 0);
                std::thread t1(keep_connection_alive);
                t1.detach();
                // const char* path = getThumbnailPath(1).c_str();
                if (debug) {
                    debug->Info(TAG, "Discord Rich Presence Plugin initialize :)");
                    // debug->Info(TAG, getThumbnailPath(1).c_str());
                }
                // std::thread t2(change_presence_example);
                // t2.detach();
            } else {
                if (debug) debug->Info(TAG, "Discord Rich Presence Plugin failed to initialize :(");
            }
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

class EventUpdater : public IPlaybackRemote {
    public:
        void Release() override {
            delete this;
        }

        void OnTrackChanged(ITrack* track) override {
            currentTrack = track;
            if (debug && track) {
                char title[512];
                char artist[512];
                char album[512];
                int thumbnail_id = track->GetInt32("thumbnail_id", -1);
                int seconds = track->GetInt32("duration", 0);

                track->GetString("title", title, sizeof(title));
                track->GetString("artist", artist, sizeof(artist));
                track->GetString("album", album, sizeof(album));
                strcpy_s(this->title, title);
                strcpy_s(this->artist, artist);
                strcpy_s(this->album, album);
                this->thumbnailId = thumbnail_id;
                this->durationSeconds = seconds;

                // debug->Info(TAG, ("Track changed: " + std::string(title) + " - " + std::string(artist) + " [" + std::string(album) + "] [" + std::to_string(thumbnail_id) + " " + std::to_string(seconds) + "]").c_str());
                std::thread t1(ChangePresence);
                t1.detach();
            }
        }

        void OnPlaybackStateChanged(PlaybackState state) override {
            currentState = state;
        }

        void SetPlaybackService(IPlaybackService* playback) override { }
        void OnPlaybackTimeChanged(double time) override { }
        void OnModeChanged(RepeatMode repeatMode, bool shuffled) override { }
        void OnVolumeChanged(double volume) override { }
        void OnPlayQueueChanged() override { }

    private:
        PlaybackState currentState = PlaybackState::Stopped;
        ITrack* currentTrack = nullptr;

        char title[512], artist[512], album[512];
        int thumbnailId, durationSeconds;

        void ChangePresence() {
            const char* path = getThumbnailPath(thumbnailId).c_str();
            const char* url = upload_cover_image(path);
            update_presence(title, artist, album, url, durationSeconds);
        }
};

extern "C" DLLEXPORT void SetDebug(IDebug* debug) {
    ::debug = debug;
}

extern "C" DLLEXPORT void SetEnvironment(IEnvironment* environment) {
    ::environment = environment;
}

extern "C" DLLEXPORT IPlaybackRemote* GetPlaybackRemote() {
    return new EventUpdater();
}

extern "C" DLLEXPORT IPlugin* GetPlugin() {
    return new DiscordRichPresencePlugin();
}