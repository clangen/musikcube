#ifndef MUSIKCORE_PLUGIN_SERVICES_H
#define MUSIKCORE_PLUGIN_SERVICES_H

#include <musikcore/plugin/audio.h>
#include <musikcore/plugin/settings.h>

typedef struct mc_plugin_media_service mc_plugin_media_service;
typedef struct mc_plugin_indexer_service mc_plugin_indexer_service;
typedef struct mc_host_api mc_host_api;
typedef struct mc_plugin_playback_service mc_plugin_playback_service;
typedef struct mc_plugin_library_service mc_plugin_library_service;

/* host-owned services available to a plugin instance through destroy. a
decoder receives audio_scratch so it can write directly into the host's buffer. */
struct mc_plugin_services {
    /* available bytes. initialize with MC_PLUGIN_SERVICES_DEFAULT. */
    uint32_t struct_size;

    /* optional host cancellation latch. its callbacks can run on plugin workers. */
    const mc_cancel_token* cancel;

    /* required when creating a decoder. other plugin families may receive null. */
    const mc_plugin_audio_scratch_service* audio_scratch;

    /* shared stream and decoder factories. valid for the instance lifetime. */
    const mc_plugin_media_service* media;

    /* UTF-8 application data directory. the host owns the bytes. empty if unavailable. */
    mc_string_view data_directory;

    /* saved preferences for this plugin. required for output and indexer
    source instances and valid for the instance lifetime. */
    const mc_plugin_preferences_service* preferences;

    /* granted indexing operations. sources receive source-bound rescans;
    playback remotes may receive general scan scheduling. */
    const mc_plugin_indexer_service* indexer;

    /* bootstrap callbacks, required for indexer sources and valid through destroy. */
    const mc_host_api* host;

    /* granted playback operations; valid until the plugin instance is destroyed. */
    const mc_plugin_playback_service* playback;

    /* local library operations; valid until the plugin instance is destroyed. */
    const mc_plugin_library_service* library;
};

#define MC_PLUGIN_SERVICES_DEFAULT { sizeof(mc_plugin_services), NULL, NULL, NULL, { NULL, 0 }, NULL, NULL, NULL, NULL, NULL }

#endif
