#include <musikcore/plugin/playback-remote.hpp>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

/* fail with the specific contract that the generated C adapter violated. */
static void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "playback remote adapter: %s\n", message);
        std::abort();
    }
}

/* record calls made through the generated C table without inheriting an SDK class. */
struct TestRemote {
    explicit TestRemote(const mc_plugin_services&) {}

    /* remember the playback event delivered by the host. */
    void Changed(const mc_plugin_playback_event& event) noexcept { lastEvent = event.event; }

    /* distinguish queue notifications from playback-state notifications. */
    void QueueChanged() noexcept { ++queueChanges; }

    /* simulate a failed start after the plugin began preparing workers. */
    mc_result Start() { throw std::runtime_error("start failed"); }

    /* record the cleanup the host performs after a failed start. */
    void RequestStop() noexcept { stopped = true; }

    /* the host can join even when start did not succeed. */
    mc_result Join() { return stopped ? MC_OK : MC_ERROR_PLUGIN; }

    mc_playback_event lastEvent { MC_PLAYBACK_CHANGED_TRACK };
    unsigned queueChanges { 0 };
    bool stopped { false };
};

/* capture the plugin-owned table, as the real registry does at registration. */
static mc_result MC_CALL capture(void* context, const mc_plugin_capability* capability) {
    *static_cast<const mc_playback_remote_factory**>(context) =
        static_cast<const mc_playback_remote_factory*>(capability->api);
    return MC_OK;
}

/* failed startup remains recoverable only if stop and join can still run. */
int main() {
    const mc_playback_remote_factory* factory = nullptr;
    mc_host_api host = MC_HOST_API_DEFAULT;
    host.host_context = &factory;
    host.register_plugin_capability = capture;
    require(musik::core::plugin::RegisterPlaybackRemote<TestRemote>(host) == MC_OK && factory,
        "register factory");

    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    void* instance = nullptr;
    require(factory->create(factory->context, &services, &instance, nullptr) == MC_OK && instance,
        "create instance");
    const auto* methods = factory->instance_api;
    require(methods->start(instance, nullptr) == MC_ERROR_PLUGIN, "contain startup exception");
    mc_plugin_playback_event event = MC_PLUGIN_PLAYBACK_EVENT_DEFAULT;
    event.event = MC_PLAYBACK_CHANGED_VOLUME;
    methods->changed(instance, &event);
    methods->queue_changed(instance);
    const auto* remote = static_cast<const TestRemote*>(instance);
    require(remote->lastEvent == MC_PLAYBACK_CHANGED_VOLUME && remote->queueChanges == 1,
        "forward playback and queue notifications");
    methods->request_stop(instance);
    require(methods->join(instance, nullptr) == MC_OK, "join after failed startup");
    methods->destroy(instance);
    return 0;
}
