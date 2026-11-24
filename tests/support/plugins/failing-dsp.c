#include <musikcore/plugin.h>
#include <musikcore/plugin/dsp.h>

/* overwrite a sample before failing so playback must discard the whole block. */
static mc_result MC_CALL process(void* instance, mc_audio_buffer_view* audio) {
    (void) instance;
    audio->data[0] = 99.0f;
    return MC_ERROR_PLUGIN;
}

static void MC_CALL destroy(void* instance) { (void) instance; }
static void MC_CALL reload(void* context) { (void) context; }

static mc_result MC_CALL create(void* context, const mc_plugin_services* services, void** instance, mc_error** error) {
    (void) context;
    (void) services;
    (void) error;
    static int state;
    *instance = &state;
    return MC_OK;
}

static mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error** error) {
    (void) error;
    *state = NULL;
    static const mc_dsp_vtable methods = { sizeof(mc_dsp_vtable), destroy, process };
    static const mc_dsp_factory factory = { sizeof(mc_dsp_factory), NULL, &methods, create, reload };
    mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
    capability.id = (mc_string_view) { MC_PLUGIN_CAPABILITY_DSP, sizeof(MC_PLUGIN_CAPABILITY_DSP) - 1 };
    capability.api = &factory;
    capability.api_size = sizeof(factory);
    return host->register_capability(host->host_context, &capability);
}

static void MC_CALL shutdown_plugin(void* state) { (void) state; }

MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    mc_result status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = (mc_plugin_api) MC_PLUGIN_API_DEFAULT;
    plugin->id = (mc_string_view) { "test.failing.dsp", 16 };
    plugin->name = (mc_string_view) { "Failing DSP", 11 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown_plugin;
    return MC_OK;
}
