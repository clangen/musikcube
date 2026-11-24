#include <musikcore/plugin.h>
#include <musikcore/plugin/stream.h>

#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <new>
#include <string_view>

namespace {
    struct Stream {
        std::mutex mutex;
        std::condition_variable changed;
        bool interrupted = false;
    };

    mc_result MC_CALL canHandle(void*, const mc_media_hint* hint, uint32_t* supported) {
        const std::string_view uri(hint->uri.data, hint->uri.length);
        *supported = uri.ends_with("analysis-block.wav") ? 1 : 0;
        return MC_OK;
    }

    mc_result MC_CALL create(void*, const mc_plugin_services*, void** instance, mc_error**) {
        *instance = new (std::nothrow) Stream();
        return *instance ? MC_OK : MC_ERROR_INTERNAL;
    }

    void MC_CALL destroy(void* instance) { delete static_cast<Stream*>(instance); }

    mc_result MC_CALL open(void* instance, mc_string_view, mc_stream_open_flags, mc_error**) {
        auto& stream = *static_cast<Stream*>(instance);
        std::unique_lock lock(stream.mutex);
        std::puts("analysis-open");
        std::fflush(stdout);
        stream.changed.wait(lock, [&] { return stream.interrupted; });
        return MC_ERROR_CANCELLED;
    }

    void MC_CALL interrupt(void* instance) {
        auto& stream = *static_cast<Stream*>(instance);
        std::lock_guard lock(stream.mutex);
        stream.interrupted = true;
        stream.changed.notify_all();
    }

    mc_result MC_CALL close(void*) { return MC_OK; }
    mc_result MC_CALL info(void*, mc_stream_info*) { return MC_ERROR_UNSUPPORTED; }
    mc_result MC_CALL read(void*, void*, size_t, size_t*, mc_read_state*, mc_error**) { return MC_ERROR_UNSUPPORTED; }
    mc_result MC_CALL write(void*, const void*, size_t, size_t*, mc_error**) { return MC_ERROR_UNSUPPORTED; }
    mc_result MC_CALL seek(void*, uint64_t, mc_error**) { return MC_ERROR_UNSUPPORTED; }
    mc_result MC_CALL tell(void*, uint64_t*) { return MC_ERROR_UNSUPPORTED; }

    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        static const mc_stream_vtable methods {
            .struct_size = sizeof(mc_stream_vtable),
            .destroy = destroy,
            .open = open,
            .close = close,
            .interrupt = interrupt,
            .get_info = info,
            .read = read,
            .write = write,
            .seek = seek,
            .tell = tell,
        };
        static const mc_stream_factory factory {
            .struct_size = sizeof(mc_stream_factory),
            .context = nullptr,
            .instance_api = &methods,
            .can_handle = canHandle,
            .create = create,
        };
        mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
        capability.id = { MC_PLUGIN_CAPABILITY_STREAM, sizeof(MC_PLUGIN_CAPABILITY_STREAM) - 1 };
        capability.api = &factory;
        capability.api_size = sizeof(factory);
        return host->register_plugin_capability(host->host_context, &capability);
    }

    void MC_CALL shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "test.analysis.block", 19 };
    plugin->name = { "Blocking analysis stream", 24 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
