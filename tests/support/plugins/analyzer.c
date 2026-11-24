#include <musikcore/plugin.h>
#include <musikcore/plugin/analyzer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* record callbacks in a private test file so the integration runner can
verify actual indexer calls after the plugin has been unloaded. */
static void record(const char* text) {
    const char* path = getenv("MUSIKCUBE_ANALYZER_TEST_LOG");
    FILE* file = path ? fopen(path, "a") : NULL;
    if (file) {
        fprintf(file, "%s\n", text);
        fclose(file);
    }
}

typedef struct Analyzer { size_t frames; } Analyzer;

static mc_result MC_CALL create(void* context, const mc_plugin_services* services, void** instance, mc_error** error) {
    (void) context;
    (void) services;
    (void) error;
#ifdef ANALYZER_FAILING
    if (getenv("MUSIKCUBE_ANALYZER_FAIL_CREATE")) {
        return MC_ERROR_PLUGIN;
    }
    if (getenv("MUSIKCUBE_ANALYZER_NULL_CREATE")) {
        return MC_OK;
    }
#endif
    *instance = calloc(1, sizeof(Analyzer));
    return *instance ? MC_OK : MC_ERROR_INTERNAL;
}

static void MC_CALL destroy(void* instance) { free(instance); }

static mc_result MC_CALL begin(void* instance, const mc_metadata_writer* writer, mc_error** error) {
    ((Analyzer*) instance)->frames = 0;
    (void) error;
    /* reject impossible lengths before the host inspects caller memory. */
    const mc_string_view oversized = { "x", SIZE_MAX };
    uint32_t ignored = 0;
    if (writer->contains(writer->context, oversized, &ignored) != MC_ERROR_INVALID_ARGUMENT) {
        return MC_ERROR_PLUGIN;
    }
    const mc_string_view key = { "analyzer_test", 13 };
    const mc_string_view value = { "present", 7 };
    uint32_t present = 0;
    if (writer->set_value(writer->context, key, value) != MC_OK ||
        writer->contains(writer->context, key, &present) != MC_OK || !present)
    {
        return MC_ERROR_PLUGIN;
    }
    if (getenv("MUSIKCUBE_ANALYZER_WRITE_ARTWORK")) {
        const unsigned char bytes[] = { 1, 2, 3, 4 };
        const mc_byte_view artwork = { bytes, sizeof(bytes) };
        if (writer->set_artwork(writer->context, artwork) != MC_OK) {
            return MC_ERROR_PLUGIN;
        }
    }
    record("begin");
    return MC_OK;
}

static mc_result MC_CALL process(void* instance, const mc_pcm_view* pcm,
    const mc_metadata_writer* writer, mc_error** error)
{
    (void) instance;
    (void) writer;
    (void) error;
    if (!pcm->data || !pcm->frame_count || !pcm->format.channels || !pcm->format.sample_rate) {
        return MC_ERROR_PLUGIN;
    }
    ((Analyzer*) instance)->frames += pcm->frame_count;
    record("process");
#ifdef ANALYZER_FAILING
    return MC_ERROR_PLUGIN;
#else
    return MC_OK;
#endif
}

static mc_result MC_CALL finish(void* instance, const mc_metadata_writer* writer, uint32_t commit, mc_error** error) {
    (void) instance;
    (void) writer;
    (void) error;
    const char* gate = getenv("MUSIKCUBE_ANALYZER_FINISH_GATE");
    if (gate) {
        /* hold the worker until the parent has cancelled the scan. */
        puts("analysis-finish");
        fflush(stdout);
        FILE* fifo = fopen(gate, "r");
        if (!fifo) {
            return MC_ERROR_IO;
        }
        (void) fgetc(fifo);
        fclose(fifo);
    }
    char frames[64];
    snprintf(frames, sizeof(frames), "frames=%zu", ((Analyzer*) instance)->frames);
    record(frames);
    record(commit ? "commit" : "cancel");
    return MC_OK;
}


/* publish one factory with no plugin-owned shared state. */
static mc_result MC_CALL initialize(const mc_host_api* host, void** context, mc_error** error) {
    (void) context;
    (void) error;
    static const mc_analyzer_vtable methods = {
        .struct_size = sizeof(mc_analyzer_vtable),
        .destroy = destroy,
        .begin = begin,
        .process = process,
        .finish = finish,
    };
    static const mc_analyzer_factory factory = {
        .struct_size = sizeof(mc_analyzer_factory),
        .instance_api = &methods,
        .create = create,
    };
    const mc_plugin_capability capability = {
        .struct_size = sizeof(mc_plugin_capability),
        .id = { MC_PLUGIN_CAPABILITY_ANALYZER, sizeof(MC_PLUGIN_CAPABILITY_ANALYZER) - 1 },
        .api = &factory,
        .api_size = sizeof(factory),
    };
    return host->register_plugin_capability(host->host_context, &capability);
}

static void MC_CALL shutdown_plugin(void* context) { (void) context; }

MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const mc_result status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = (mc_plugin_api) MC_PLUGIN_API_DEFAULT;
#ifdef ANALYZER_FAILING
    plugin->id = (mc_string_view) { "test.analyzer.failure", 21 };
    plugin->name = (mc_string_view) { "Failing analyzer fixture", 24 };
#else
    plugin->id = (mc_string_view) { "test.analyzer", 13 };
    plugin->name = (mc_string_view) { "Analyzer fixture", 16 };
#endif
    plugin->initialize = initialize;
    plugin->shutdown = shutdown_plugin;
    return MC_OK;
}
