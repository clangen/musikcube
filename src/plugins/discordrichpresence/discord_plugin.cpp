#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/sdk/ITrack.h>
#include <musikcore/sdk/IValue.h>
#include <musikcore/sdk/IEnvironment.h>

#include <thread>
#include <string>

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
bool opened = false;

std::string get_thumbnail_path(int albumId) {
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

class DiscordRichPresencePlugin : public IPlugin {
    public:
        DiscordRichPresencePlugin() {
            if (!has_internet_connection()) {
                if (debug) {debug->Warning(TAG, "No internet connection detected");}
                return;
            }
            
            opened = init_discord();
            if (opened) {
                update_presence("unknown", "unknown", "unknown", "unknown", 0);
                std::thread t1(keep_connection_alive);
                t1.detach();
                if (debug) {debug->Info(TAG, "Discord Rich Presence Plugin initialize :)");}
            } else {
                if (debug) {debug->Info(TAG, "Discord Rich Presence Plugin failed to initialize :(");}
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
        void Release() override { delete this; }

        void OnTrackChanged(ITrack* track) override {
            if (this->onCooldown || !opened) return;
            char title[128] = {'\0'};
            char artist[128] = {'\0'};
            char album[128] = {'\0'};
            int thumbnail_id = track->GetInt32("thumbnail_id", 0);
            int seconds = track->GetInt32("duration", 0);

            track->GetString("title", title, sizeof(title));
            track->GetString("artist", artist, sizeof(artist));
            track->GetString("album", album, sizeof(album));

            if (strcmp(title, this->title) == 0) return;// prevent duplicate updates
            strcpy_s(this->title, title);
            std::thread t1([this, title = std::string(title), artist = std::string(artist), 
                            album = std::string(album), thumbnailId = thumbnail_id, 
                            durationSeconds = seconds]() {
                this->change_presence(title.c_str(), artist.c_str(), album.c_str(), thumbnailId, durationSeconds);
            });
            t1.detach();
            std::thread t2([this]() {
                cooldown();
            });
            t2.detach();
        }

        void OnPlaybackStateChanged(PlaybackState state) override { }
        void SetPlaybackService(IPlaybackService* playback) override { }
        void OnPlaybackTimeChanged(double time) override { }
        void OnModeChanged(RepeatMode repeatMode, bool shuffled) override { }
        void OnVolumeChanged(double volume) override { }
        void OnPlayQueueChanged() override { }

    private:
        // previous presence data to prevent duplicate updates
        char title[128], url[128];// 128 is the max length for discord sdk can hold
        int thumbnailId;
        bool onCooldown = false;

        void change_presence(const char* title, const char* artist, const char* album, int thumbnailId, int durationSeconds) {
            if (!opened || !has_internet_connection()) return;
            char* url = "unknown";
            if (thumbnailId == this->thumbnailId) {//checking new thumbnail id against previous one to prevent duplicate uploads since different songs can have same album art
                url = this->url;
            } else {// uploading a new thumbnail is quite slow, so we only do it when the album changes
                if (thumbnailId != 0) {// 0 means no thumbnail
                    const char* path = get_thumbnail_path(thumbnailId).c_str();
                    if (strcmp(path, "unknown") != 0) {// valid path
                        char* temp = upload_cover_image(path);
                        url = temp;
                        free(temp);
                    }
                }
            }
            if (strcmp(url, "unknown") == 0) {debug->Warning(TAG, "potential thumbnail problem");}
            this->thumbnailId = thumbnailId;
            strcpy_s(this->url, url);
            update_presence(title, artist, album, url, durationSeconds);
            debug->Info(TAG, ("Updated Discord Rich Presence: " + std::string(title) + " by " + std::string(artist)).c_str());
        }

        void cooldown() {// simple cooldown to prevent being rate limited by Discord or 0x0.st
            this->onCooldown = true;
            sleep(10000);
            this->onCooldown = false;
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