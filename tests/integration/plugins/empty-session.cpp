#include <musikcore/musikcore.h>

#include <cstdio>
#include <cstring>

static bool require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "empty plugin session: %s\n", message);
    }
    return condition;
}

static void MC_CALL readOutputs(void* user, const mc_output_configuration* configuration) {
    *static_cast<bool*>(user) = configuration && configuration->driver_count == 0;
}

int main(int argc, char** argv) {
    if (!require(argc == 2, "expected empty plugin directory")) {
        return 1;
    }
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.plugin_directory = { argv[1], std::strlen(argv[1]) };
    options.services = MC_SERVICE_PLAYBACK;
    mc_context* context = nullptr;
    if (!require(mc_context_create(&options, &context, nullptr) == MC_OK, "start engine")) {
        return 1;
    }
    bool noOutputs = false;
    mc_playback* playback = nullptr;
    mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
    const bool okay =
        require(mc_output_read_configuration(readOutputs, &noOutputs) == MC_OK && noOutputs, "no output drivers") &&
        require(mc_context_get_playback(context, &playback) == MC_OK && playback, "obtain playback") &&
        require(mc_playback_get_snapshot(playback, &snapshot) == MC_OK && snapshot.state == MC_PLAYBACK_STOPPED,
            "playback remains stopped") &&
        require(mc_playback_control(playback, MC_PLAYBACK_STOP, 0) == MC_OK, "stop idle playback") &&
        require(mc_context_dispatch(context) == MC_OK, "dispatch while idle");
    mc_playback_destroy(&playback);
    return require(mc_context_destroy(&context) == MC_OK && !context, "shut down engine") && okay ? 0 : 1;
}
