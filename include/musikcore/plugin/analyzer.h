#ifndef MUSIKCORE_PLUGIN_ANALYZER_H
#define MUSIKCORE_PLUGIN_ANALYZER_H

#include <musikcore/plugin/services.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_ANALYZER "org.musikcube.analyzer"

/* replay gain values as stored by the current library. gain is in decibels,
peak is a linear amplitude. the current store uses 1.0 as its unset sentinel.
all four values are supplied together. */
typedef struct mc_replay_gain_values {
    /* available bytes. initialize with MC_REPLAY_GAIN_VALUES_DEFAULT. */
    uint32_t struct_size;

    /* track gain in decibels, track peak, album gain in decibels, and album
    peak. each defaults to the current store's 1.0 unset sentinel. */
    float track_gain;
    float track_peak;
    float album_gain;
    float album_peak;
} mc_replay_gain_values;

#define MC_REPLAY_GAIN_VALUES_DEFAULT { sizeof(mc_replay_gain_values), 1.0f, 1.0f, 1.0f, 1.0f }

/* lets an analyzer update the indexed track it is currently processing.
the writer and its context are valid only during the analyzer call. */
typedef struct mc_metadata_writer {
    /* available bytes. initialize with MC_METADATA_WRITER_DEFAULT. */
    uint32_t struct_size;

    /* host-owned track state. pass unchanged to each callback. */
    void* context;

    /* append a nonempty UTF-8 value for the given nonempty UTF-8 key. */
    mc_result (MC_CALL* set_value)(void* context, mc_string_view key, mc_string_view value);

    /* remove every value for the key. */
    mc_result (MC_CALL* clear)(void* context, mc_string_view key);

    /* write one if the track has a value for the key, otherwise zero. */
    mc_result (MC_CALL* contains)(void* context, mc_string_view key, uint32_t* present);

    /* copy nonempty artwork bytes into the track. */
    mc_result (MC_CALL* set_artwork)(void* context, mc_byte_view bytes);

    /* write one when the track has artwork, otherwise zero. */
    mc_result (MC_CALL* contains_artwork)(void* context, uint32_t* present);

    /* replace all four replay-gain factors. */
    mc_result (MC_CALL* set_replay_gain)(void* context, const mc_replay_gain_values* gain);
} mc_metadata_writer;

#define MC_METADATA_WRITER_DEFAULT { sizeof(mc_metadata_writer), NULL, NULL, NULL, NULL, NULL, NULL, NULL }

/* analyzes decoded audio on the indexer worker and may update track metadata.
the host calls finish after begin, including on failure or cancellation.
calls run on the indexer worker. cancellation waits for the current call to return. */
typedef struct mc_analyzer_vtable {
    /* available bytes. initialize with MC_ANALYZER_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* destroy after the host has finished all calls. */
    void (MC_CALL* destroy)(void* instance);

    /* begin analyzing a track using the call-scoped writer. */
    mc_result (MC_CALL* begin)(void* instance, const mc_metadata_writer* writer, mc_error** error);

    /* consume call-scoped PCM and metadata before returning. */
    mc_result (MC_CALL* process)(void* instance, const mc_pcm_view* pcm,
        const mc_metadata_writer* writer, mc_error** error);

    /* finish a track. commit is zero when decoding or analysis failed.
    the host saves the shared track only if every analyzer finishes successfully. */
    mc_result (MC_CALL* finish)(void* instance, const mc_metadata_writer* writer, uint32_t commit, mc_error** error);
} mc_analyzer_vtable;

#define MC_ANALYZER_VTABLE_DEFAULT { sizeof(mc_analyzer_vtable), NULL, NULL, NULL, NULL }

/* creates independent analyzer instances. the host destroys instances before
plugin shutdown and serializes each instance's ordinary calls. */
typedef struct mc_analyzer_factory {
    /* available bytes. initialize with MC_ANALYZER_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned factory state, passed unchanged to create. */
    void* context;

    /* plugin-owned operations that remain valid until shutdown. */
    const mc_analyzer_vtable* instance_api;

    /* create into an empty slot. leave it empty on failure. */
    mc_result (MC_CALL* create)(void* context, const mc_plugin_services* services, void** out_instance, mc_error** error);
} mc_analyzer_factory;

#define MC_ANALYZER_FACTORY_DEFAULT { sizeof(mc_analyzer_factory), NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
