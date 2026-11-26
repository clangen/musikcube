#include <musikcore/plugin.h>
#include <musikcore/plugin/playback-remote.h>
#include <musikcore/plugin/indexer.h>
#include SDBUS_HEADER
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>
#include <poll.h>
#include <thread>
#include <unistd.h>

static void require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "MPRIS: %s\n", message);
        std::abort();
    }
}

static const mc_playback_remote_factory* factory = nullptr;
static std::atomic<mc_playback_command> lastCommand { 0 };
static std::atomic<double> lastValue { 0 };
static std::atomic<bool> rejectCommand { false };
static std::thread commandCompletion;
static mc_result MC_CALL registerCapability(void*, const mc_plugin_capability* capability) {
    if (capability->id.length == std::strlen(MC_PLUGIN_CAPABILITY_PLAYBACK_REMOTE) &&
        std::memcmp(capability->id.data, MC_PLUGIN_CAPABILITY_PLAYBACK_REMOTE, capability->id.length) == 0)
    {
        factory = static_cast<const mc_playback_remote_factory*>(capability->api);
    }
    return MC_OK;
}
static void MC_CALL logMessage(void*, mc_log_level, mc_string_view, mc_string_view) {}
static mc_result MC_CALL createError(void*, mc_result, mc_string_view, mc_error**) { return MC_ERROR_INTERNAL; }
static void MC_CALL destroyError(void*, mc_error**) {}
static mc_result MC_CALL readState(void*, mc_playback_remote_state_fn read, void* user) {
    mc_playback_snapshot snapshot = MC_PLAYBACK_SNAPSHOT_DEFAULT;
    snapshot.volume = 0.25;
    read(user, &snapshot, nullptr);
    return MC_OK;
}
static mc_result MC_CALL control(void*, mc_playback_command command, double value,
    mc_playback_remote_completion_fn completion, void* user)
{
    if (rejectCommand.load()) {
        return MC_ERROR_QUEUE_FULL;
    }
    if (commandCompletion.joinable()) {
        commandCompletion.join();
    }
    lastCommand.store(command);
    lastValue.store(value);
    commandCompletion = std::thread([completion, user] { completion(user, MC_OK); });
    return MC_OK;
}
static mc_result MC_CALL withPlayback(void*, mc_playback_remote_access_fn completion, void* data) {
    completion(data, MC_ERROR_CANCELLED, nullptr);
    return MC_OK;
}
static mc_result MC_CALL schedule(void*, mc_indexer_scan, mc_playback_remote_completion_fn, void*) {
    return MC_ERROR_UNSUPPORTED;
}

int main(int argc, char** argv) {
    require(argc == 2 || argc == 3, "plugin path supplied");
    void* library = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    require(library != nullptr, "plugin loads");
    auto entry = reinterpret_cast<mc_plugin_get_api_fn>(dlsym(library, "mc_plugin_get_api"));
    require(entry != nullptr, "C plugin entry exists");
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = logMessage;
    host.create_error = createError;
    host.destroy_error = destroyError;
    host.register_plugin_capability = registerCapability;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "negotiate plugin");
    void* state = nullptr;
    require(plugin.initialize(&host, &state, nullptr) == MC_OK && factory, "register playback remote");
    mc_plugin_playback_service remote = MC_PLUGIN_PLAYBACK_SERVICE_DEFAULT;
    remote.read_state = readState;
    remote.control_async = control;
    remote.with_playback_async = withPlayback;
    mc_plugin_indexer_service indexer = MC_PLUGIN_INDEXER_SERVICE_DEFAULT;
    indexer.schedule_async = schedule;
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    services.host = &host;
    services.playback = &remote;
    services.indexer = &indexer;
    void* instance = nullptr;
    require(factory->create(factory->context, &services, &instance, nullptr) == MC_OK && instance, "create remote");
    const auto* api = factory->instance_api;
    require(api->start(instance, nullptr) == MC_OK, "start D-Bus remote");
    sd_bus* bus = nullptr;
    require(sd_bus_open_user(&bus) >= 0, "open session bus");
    const auto name = std::string("org.mpris.MediaPlayer2.musikcube.instance") + std::to_string(getpid());
    const char* object = "/org/mpris/MediaPlayer2";
    const char* player = "org.mpris.MediaPlayer2.Player";
    char* text = nullptr;
    require(sd_bus_get_property_string(bus, name.c_str(), object, player, "PlaybackStatus", nullptr, &text) >= 0 &&
        text && std::strcmp(text, "Stopped") == 0, "initial state exposed on D-Bus");
    std::free(text);
    if (argc == 3) {
        int ready[2];
        int release[2];
        require(pipe(ready) == 0 && pipe(release) == 0, "create artwork gates");
        const auto readyFd = std::to_string(ready[1]);
        const auto releaseFd = std::to_string(release[0]);
        setenv("MUSIKCUBE_TEST_ARTWORK_READY", readyFd.c_str(), 1);
        setenv("MUSIKCUBE_TEST_ARTWORK_RELEASE", releaseFd.c_str(), 1);
        mc_plugin_playback_event artwork = MC_PLUGIN_PLAYBACK_EVENT_DEFAULT;
        artwork.event = MC_PLAYBACK_CHANGED_TRACK;
        api->changed(instance, &artwork);
        pollfd gate { .fd = ready[0], .events = POLLIN, .revents = 0 };
        require(poll(&gate, 1, 5000) == 1, "artwork read reached gate");
        char signal;
        require(read(ready[0], &signal, 1) == 1, "consume artwork notification");
        require(sd_bus_set_method_call_timeout(bus, 1000000) >= 0, "bound D-Bus call");
        text = nullptr;
        const auto status = sd_bus_get_property_string(bus, name.c_str(), object, player,
            "PlaybackStatus", nullptr, &text);
        require(write(release[1], "x", 1) == 1, "release artwork read");
        require(poll(&gate, 1, 5000) == 1 && read(ready[0], &signal, 1) == 1,
            "artwork read exits gate");
        require(status >= 0, "D-Bus responds while artwork is blocked");
        std::free(text);
        unsetenv("MUSIKCUBE_TEST_ARTWORK_READY");
        unsetenv("MUSIKCUBE_TEST_ARTWORK_RELEASE");
        close(ready[0]);
        close(ready[1]);
        close(release[0]);
        close(release[1]);
    }
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Next", &error, &reply, "") >= 0,
        "next command accepted over D-Bus");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    require(lastCommand.load() == MC_PLAYBACK_NEXT, "next reaches the playback host");
    constexpr mc_playback_command pauseCommand = MC_PLAYBACK_PAUSE;
    constexpr mc_playback_command resumeCommand = MC_PLAYBACK_RESUME;
    reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Pause", &error, &reply, "") >= 0,
        "pause command accepted over D-Bus");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    require(lastCommand.load() == pauseCommand, "pause reaches the playback host directly");
    reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Play", &error, &reply, "") >= 0,
        "play command accepted over D-Bus");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    require(lastCommand.load() == resumeCommand, "play reaches the playback host directly");
    require(sd_bus_set_property(bus, name.c_str(), object, player, "Volume", &error, "d", 0.75) >= 0,
        "volume command accepted over D-Bus");
    require(lastCommand.load() == MC_PLAYBACK_SET_VOLUME && lastValue.load() == 0.75,
        "volume reaches the playback host");
    sd_bus_error_free(&error);
    rejectCommand.store(true);
    reply = nullptr;
    require(sd_bus_call_method(bus, name.c_str(), object, player, "Next", &error, &reply, "") < 0,
        "a full host queue reports an error to the D-Bus caller");
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
    rejectCommand.store(false);
    mc_plugin_playback_event event = MC_PLUGIN_PLAYBACK_EVENT_DEFAULT;
    event.event = MC_PLAYBACK_CHANGED_STATE;
    event.playback.state = MC_PLAYBACK_PAUSED;
    event.playback.position = 12.5;
    api->changed(instance, &event);
    text = nullptr;
    require(sd_bus_get_property_string(bus, name.c_str(), object, player, "PlaybackStatus", nullptr, &text) >= 0 &&
        text && std::strcmp(text, "Paused") == 0, "playback events update D-Bus state");
    std::free(text);
    int64_t position = 0;
    require(sd_bus_get_property_trivial(bus, name.c_str(), object, player, "Position", nullptr, 'x', &position) >= 0 &&
        position == 12500000, "position is reported in microseconds");
    api->request_stop(instance);
    require(api->join(instance, nullptr) == MC_OK, "stop and join D-Bus worker");
    if (commandCompletion.joinable()) {
        commandCompletion.join();
    }
    api->destroy(instance);
    sd_bus_flush_close_unref(bus);
    plugin.shutdown(state);
    dlclose(library);
}
