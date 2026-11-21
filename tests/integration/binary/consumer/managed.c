#include <musikcore/musikcore.h>

#include <string.h>

static void MC_CALL check_plugin(void* data, const mc_plugin_info* plugin) {
    const char name[] = "Null IOutput";
    if (plugin->name.length == sizeof(name) - 1 &&
        memcmp(plugin->name.data, name, sizeof(name) - 1) == 0)
    {
        *(bool*) data = true;
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }

    mc_context* context = NULL;
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.flags = MC_CONTEXT_MANAGED_RUNTIME;
    options.plugin_directory = (mc_string_view) { argv[1], strlen(argv[1]) };
    if (mc_context_create(&options, &context, NULL) != MC_OK) {
        return 1;
    }

    bool found = false;
    const mc_result status = mc_plugins_list(check_plugin, &found);
    const mc_result shutdown = mc_context_shutdown(context);
    mc_context_destroy(&context);
    return status == MC_OK && found && shutdown == MC_OK && !context ? 0 : 1;
}
