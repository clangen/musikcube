#include <musikcore/sdk/constants.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IDebug.h>
#include <musikcore/sdk/IPlaybackService.h>
#include <musikcore/sdk/IPlaybackRemote.h>
#include <musikcore/sdk/ITrack.h>
#include <musikcore/sdk/IValue.h>
#include <musikcore/sdk/IEnvironment.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <thread>
#include <string>
#include <windows.h>
#include <curl/curl.h>
#include "../../3rdparty/win32_include/discord_game_sdk/discord_game_sdk.h"
#include <musikcore/sdk/version.h>

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

// =====================  MUSIKCUBE DEBUG FUNCTIONS =====================

void log_debug(const char* message, ...) {
    if (!debug) return;

    va_list args;
    va_start(args, message);
    #ifdef _MSC_VER
        int size = _vscprintf(message, args);
    #else
        int size = vsnprintf(NULL, 0, message, args);
    #endif
    va_end(args);

    if (size < 0) return;

    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) return;

    va_start(args, message);
    vsnprintf(buffer, (size_t)size + 1, message, args);
    va_end(args);

    debug->Info(TAG, buffer);
    free(buffer);
}


void log_error(const char* message, ...) {
    if (!debug) return;

    va_list args;
    va_start(args, message);
    #ifdef _MSC_VER
        int size = _vscprintf(message, args);
    #else
        int size = vsnprintf(NULL, 0, message, args);
    #endif
    va_end(args);

    if (size < 0) return;

    char* buffer = (char*)malloc((size_t)size + 1);
    if (!buffer) return;

    va_start(args, message);
    vsnprintf(buffer, (size_t)size + 1, message, args);
    va_end(args);

    debug->Error(TAG, buffer);
    free(buffer);
}

// ===================== DISCORD SDK FUNCTIONS =====================

struct IDiscordCore* core = NULL;

bool init_discord() {
    struct DiscordCreateParams params;
    DiscordCreateParamsSetDefault(&params);
    params.client_id = 1424235979971235850;
    params.flags = DiscordCreateFlags_Default;

    enum EDiscordResult result = DiscordCreate(DISCORD_VERSION, &params, &core);
    if (result != DiscordResult_Ok) {
        return false;
    }
    return true;
}

void update_presence(const char* track, const char* artist, const char* album, const char* cover_url, int duration_seconds) {
    struct IDiscordActivityManager* activity_manager = core->get_activity_manager(core);
    struct DiscordActivity activity;
    
    memset(&activity, 0, sizeof(activity));
    
    time_t now = time(NULL);
    activity.timestamps.start = now;
    activity.timestamps.end = now + duration_seconds;

    if (cover_url) {
        strncpy(activity.assets.large_image, cover_url, sizeof(activity.assets.large_image) - 1);
        activity.assets.large_image[sizeof(activity.assets.large_image) - 1] = '\0';
    }

    if (album) {
        strncpy(activity.assets.large_text, album, sizeof(activity.assets.large_text) - 1);
        activity.assets.large_text[sizeof(activity.assets.large_text) - 1] = '\0';
    }

    strncpy(activity.assets.small_image, "icon", sizeof(activity.assets.small_image) - 1);
    activity.assets.small_image[sizeof(activity.assets.small_image) - 1] = '\0';
    strncpy(activity.assets.small_text, "musikcube", sizeof(activity.assets.small_text) - 1);
    activity.assets.small_text[sizeof(activity.assets.small_text) - 1] = '\0';

    if (track) {
        strncpy(activity.details, track, sizeof(activity.details) - 1);
        activity.details[sizeof(activity.details) - 1] = '\0';
    }

    if (artist) {
        snprintf(activity.state, sizeof(activity.state), "by %s", artist);
    }

    activity.type = DiscordActivityType_Listening;
    activity.instance = false;

    activity_manager->update_activity(activity_manager, &activity, NULL, NULL);
}

struct response_data {
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    struct response_data *resp = (struct response_data *)userp;

    char *ptr = (char *)realloc(resp->data, resp->size + total_size + 1);
    if (!ptr) return 0;

    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, total_size);
    resp->size += total_size;
    resp->data[resp->size] = '\0';

    return total_size;
}

char* upload_cover_image(const char* file_path) {
    const char *urls[] = {
        "https://x0.at",
        "https://0x0.st"
    };
    CURL *curl = NULL;
    CURLcode res;
    struct response_data response = {0};

    // log_debug("upload_cover_image: Starting upload for %s", file_path ? file_path : "(null)");

    if (!file_path) {
        // log_debug("upload_cover_image: file_path is NULL");
        return NULL;
    }

    // Normalize path: convert forward slashes to backslashes on Windows
    char normalized_path[1024];
    strncpy(normalized_path, file_path, sizeof(normalized_path) - 1);
    normalized_path[sizeof(normalized_path) - 1] = '\0';
    for (int i = 0; normalized_path[i]; i++) {
        if (normalized_path[i] == '/') {
            normalized_path[i] = '\\';
        }
    }

    // Check if file exists and is readable
    FILE* test_file = fopen(normalized_path, "rb");
    if (!test_file) {
        log_error("upload_cover_image: Cannot open file: %s", normalized_path);
        return NULL;
    }
    
    // Get file size
    fseek(test_file, 0, SEEK_END);
    long file_size = ftell(test_file);
    fseek(test_file, 0, SEEK_SET);
    // log_debug("upload_cover_image: File size: %ld bytes", file_size);
    fclose(test_file);

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        log_error("upload_cover_image: curl_global_init failed");
        return NULL;
    }

    for (size_t i = 0; i < sizeof(urls) / sizeof(urls[0]); ++i) {
        const char* url = urls[i];
        response.data = (char*)malloc(1);
        if (!response.data) {
            log_error("upload_cover_image: Failed to allocate response buffer");
            break;
        }
        response.size = 0;
        response.data[0] = '\0';

        curl = curl_easy_init();
        if (!curl) {
            log_error("upload_cover_image: curl_easy_init failed");
            free(response.data);
            response.data = NULL;
            break;
        }

        curl_mimepart *part;
        curl_mime *mime = curl_mime_init(curl);
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_filedata(part, normalized_path);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);
        // for some reason, the user agent is causing 403 errors on x0.at, so we will not set it for now
        // curl_easy_setopt(curl, CURLOPT_USERAGENT, musik::cube::userAgent());
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // log_debug("upload_cover_image: Performing curl request to %s", url);
        res = curl_easy_perform(curl);

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (res == CURLE_OK && http_code >= 200 && http_code < 300) {
            // log_debug("upload_cover_image: HTTP response code: %ld", http_code);
            // log_debug("upload_cover_image: Response size: %zu bytes", response.size);


            if (response.data && response.size > 0) {
                char preview[101] = {0};
                strncpy(preview, response.data, 100);
                // log_debug("upload_cover_image: Response preview: %s", preview);
            }

            if (response.data && response.size > 0) {
                while (response.size > 0 && (response.data[response.size - 1] == '\n' || 
                                             response.data[response.size - 1] == '\r' ||
                                             response.data[response.size - 1] == ' ')) {
                    response.data[response.size - 1] = '\0';
                    response.size--;
                }
            }

            // log_debug("upload_cover_image: Upload successful, URL: %s", response.data);

            curl_mime_free(mime);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return response.data;
        }

        if (res != CURLE_OK) {
            log_error("upload_cover_image: curl_easy_perform failed for %s: %s (code %d)", url, curl_easy_strerror(res), res);
        } else {
            log_error("upload_cover_image: Rejecting non-2xx response from %s (%ld)", url, http_code);
        }

        curl_mime_free(mime);
        curl_easy_cleanup(curl);
        free(response.data);
        response.data = NULL;

        if (i + 1 < sizeof(urls) / sizeof(urls[0])) {
            log_error("upload_cover_image: retrying with fallback upload service");
        }
    }

    curl_global_cleanup();
    return NULL;
}

void keep_connection_alive() {
    while (1) {
        core->run_callbacks(core);
        Sleep(1000);
    }
}

// ===================== MUSIKCUBE PLUGIN FUNCTIONS =====================

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

class DiscordRichPresencePlugin : public IPlugin {
    public:
        DiscordRichPresencePlugin() {
            bool opened = init_discord();
            if (opened) {
                update_presence("unknown", "unknown", "unknown", "unknown", 0);
                std::thread t1(keep_connection_alive);
                t1.detach();
                log_debug("Discord Rich Presence Plugin initialized successfully :)");
            } else {
                log_error("Discord Rich Presence Plugin failed to initialize :(");
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
            if (!track) {
                currentTrack = nullptr;
                hasCurrentTrack = false;
                return;
            }

            char title[512];
            char artist[512];
            char album[512];
            int thumbnail_id = track->GetInt32("thumbnail_id", -1);
            int seconds = track->GetInt32("duration", 0);

            track->GetString("title", title, sizeof(title));
            track->GetString("artist", artist, sizeof(artist));
            track->GetString("album", album, sizeof(album));

            if (hasCurrentTrack &&
                this->durationSeconds == seconds &&
                strcmp(this->title, title) == 0 &&
                strcmp(this->artist, artist) == 0 &&
                strcmp(this->album, album) == 0)
            {
                return;
            }

            currentTrack = track;
            strcpy_s(this->title, title);
            strcpy_s(this->artist, artist);
            strcpy_s(this->album, album);
            this->durationSeconds = seconds;
            hasCurrentTrack = true;

            std::thread t1([this, thumbnail_id]() {
                this->ChangePresence(thumbnail_id);
            });
            t1.detach();
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

        char title[512] = {};
        char artist[512] = {};
        char album[512] = {};
        char url[512] = {};
        int thumbnailId = -1;
        int durationSeconds = 0;
        bool hasCurrentTrack = false;

        void ChangePresence(int thumbnail_id) {
            if (thumbnail_id != this->thumbnailId) {// helps reduce spam
                std::string path = getThumbnailPath(thumbnail_id);
                char* url = upload_cover_image(path.c_str());
                if (!url) {url = (char*)"unknown";}
                this->thumbnailId = thumbnail_id;
                strcpy_s(this->url, sizeof(this->url), url);
                free(url);
            } else {
                log_debug("Reusing cached cover image URL");
            }

            update_presence(this->title, this->artist, this->album, this->url, this->durationSeconds);
            log_debug("Updating Discord presence with cover image: %s", this->url);

            return;
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