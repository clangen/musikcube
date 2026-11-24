#ifndef MUSIKCORE_PLUGIN_ANALYZER_H
#define MUSIKCORE_PLUGIN_ANALYZER_H

#include <musikcore/plugin/services.h>
#include <musikcore/plugin/metadata.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_ANALYZER "org.musikcube.analyzer"

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
