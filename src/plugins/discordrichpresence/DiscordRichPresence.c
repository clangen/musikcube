#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "discord_game_sdk.h"
#include <curl/curl.h>
#include <musikcore/sdk/version.h>

struct IDiscordCore* core = NULL;

void init_discord() {
    struct DiscordCreateParams params;
    DiscordCreateParamsSetDefault(&params);
    params.client_id = 1424235979971235850; // 🔴 Replace with your actual Discord App ID
    params.flags = DiscordCreateFlags_Default;

    enum EDiscordResult result = DiscordCreate(DISCORD_VERSION, &params, &core);
    if (result != DiscordResult_Ok) {
        // printf("Failed to init Discord: %d\n", result);
        exit(1);
    }
    // printf("Discord initialized successfully!\n");
}


void update_presence(const char* track, const char* artist, const char* album, const char* cover_url, int duration_seconds) {
    struct IDiscordActivityManager* activity_manager = core->get_activity_manager(core);
    struct DiscordActivity activity;
    
    memset(&activity, 0, sizeof(activity));
    
    time_t now = time(NULL);
    activity.timestamps.start = now;
    activity.timestamps.end = now + duration_seconds;
    strcpy(activity.assets.large_image, cover_url);
    strcpy(activity.assets.large_text, album);
    strcpy(activity.assets.small_image, "icon");
    strcpy(activity.assets.small_text, cover_url);
    strcpy(activity.details, album);
    snprintf(activity.state, sizeof(activity.state), "by %s", artist);

    activity.type = DiscordActivityType_Listening;
    activity.instance = false;

    activity_manager->update_activity(activity_manager, &activity, NULL, NULL);

    // printf("Updated presence: %s by %s from %s\n", track, artist, album);
    // printf("Cover URL: %s\n", cover_url);
}

// Struct to hold the server response
struct response_data {
    char *data;
    size_t size;
};

// Write callback to collect response body
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total_size = size * nmemb;
    struct response_data *resp = (struct response_data *)userp;

    char *ptr = realloc(resp->data, resp->size + total_size + 1);
    if (!ptr) return 0;

    resp->data = ptr;
    memcpy(&(resp->data[resp->size]), contents, total_size);
    resp->size += total_size;
    resp->data[resp->size] = '\0';

    return total_size;
}

// Returns the uploaded file URL (caller must free it)
char* upload_cover_image(const char* file_path) {
    const char *url = "https://0x0.st"; 
    CURL *curl;
    CURLcode res;
    struct curl_httppost *form = NULL;
    struct curl_httppost *lastptr = NULL;
    struct response_data response = {0};

    response.data = malloc(1); // initial allocation

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        free(response.data);
        return NULL;
    }

    curl_formadd(&form, &lastptr,
                 CURLFORM_COPYNAME, "file",
                 CURLFORM_FILE, file_path,
                 CURLFORM_END);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, form);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "musikcube/%s", MUSIKCUBE_VERSION);

    // For testing only — disable SSL verification (not recommended for production)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);

    curl_formfree(form);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK) {
        fprintf(stderr, "Upload failed: %s\n", curl_easy_strerror(res));
        free(response.data);
        return NULL;
    }

    return response.data;  // Caller must free this
}

int main() {
    init_discord();
    const char* cover_path = "./businrain.gif";
    const char* track = "Bohemian Rhapsody";
    const char* artist = "Queen";
    const char* album = "A Night at the Opera";
    // const char* cover_url = "https://0x0.st/KuY6.png";// Example cover URL
    int duration_seconds = 354; // 5:54

    char* cover_url = upload_cover_image(cover_path);
    printf("Uploaded cover URL: %s\n", cover_url);

    update_presence(track, artist, album, cover_url, duration_seconds);

    // Keep Discord connection alive
    while (1) {
        core->run_callbacks(core);
        Sleep(1000);  // On Linux, use sleep(1)
    }

    return 0;
}
