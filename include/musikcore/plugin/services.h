#ifndef MUSIKCORE_PLUGIN_SERVICES_H
#define MUSIKCORE_PLUGIN_SERVICES_H

#include <musikcore/plugin/audio.h>
#include <musikcore/plugin/settings.h>

typedef struct mc_media_host_api mc_media_host_api;
typedef struct mc_indexer_source_host_api mc_indexer_source_host_api;
typedef struct mc_host_api mc_host_api;
typedef struct mc_playback_remote_host_api mc_playback_remote_host_api;
typedef struct mc_library_host_api mc_library_host_api;

/* host-owned services available to a plugin instance through destroy. a
decoder receives audio_scratch so it can write directly into the host's buffer. */
struct mc_plugin_services {
    /* available bytes. initialize with MC_PLUGIN_SERVICES_DEFAULT. */
    uint32_t struct_size;

    /* optional host cancellation latch. its callbacks can run on plugin workers. */
    const mc_cancel_token* cancel;

    /* required when creating a decoder. other plugin families may receive null. */
    const mc_audio_scratch_buffer_api* audio_scratch;

    /* shared stream and decoder factories. valid for the instance lifetime. */
    const mc_media_host_api* media;

    /* UTF-8 application data directory. the host owns the bytes. empty if unavailable. */
    mc_string_view data_directory;

    /* saved preferences for this plugin. required for indexer sources and
    valid for the instance lifetime. */
    const mc_preferences_host_api* preferences;

    /* source-scoped rescan requests. required for indexer source instances. */
    const mc_indexer_source_host_api* indexer_source;

    /* bootstrap callbacks, required for indexer sources and valid through destroy. */
    const mc_host_api* host;

    /* playback commands for a playback remote; valid until the remote is destroyed. */
    const mc_playback_remote_host_api* playback_remote;

    /* local library operations; valid until the plugin instance is destroyed. */
    const mc_library_host_api* library;
};

#define MC_PLUGIN_SERVICES_DEFAULT { sizeof(mc_plugin_services), NULL, NULL, NULL, { NULL, 0 }, NULL, NULL, NULL, NULL, NULL }

#endif
