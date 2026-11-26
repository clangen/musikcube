#include <musikcore/plugin.h>
#include <musikcore/plugin/stream.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <string>

namespace {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    int calls = 0;
    int instances = 0;
    int shutdowns = 0;
    bool failCreation = false;

    struct Stream {
        std::mutex mutex;
        std::condition_variable changed;
        std::string uri;
        bool interrupted = false;
        bool opened = false;
        size_t position = 0;
    };

    void record() {
        std::lock_guard<std::mutex> lock(mutex);
        ++calls;
    }

    mc_result block(Stream& stream) {
        std::unique_lock<std::mutex> lock(stream.mutex);
        {
            std::lock_guard<std::mutex> controlLock(mutex);
            entered = true;
            changed.notify_all();
        }
        if (!stream.changed.wait_for(lock, std::chrono::seconds(5),
                [&] {
                    return stream.interrupted;
                }))
        {
            std::abort();
        }
        return MC_ERROR_CANCELLED;
    }

    mc_result MC_CALL canHandle(void*, const mc_media_hint* hint, uint32_t* supported) {
        record();
        *supported = hint->uri.length >= 10 && std::memcmp(hint->uri.data, "testmedia:", 10) == 0;
        return MC_OK;
    }
    mc_result MC_CALL create(void*, const mc_plugin_services*, void** instance, mc_error**) {
        record();
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (failCreation) {
                failCreation = false;
                return MC_ERROR_IO;
            }
        }
        *instance = new (std::nothrow) Stream();
        if (*instance) {
            std::lock_guard<std::mutex> lock(mutex);
            ++instances;
        }
        return *instance ? MC_OK : MC_ERROR_INTERNAL;
    }
    void MC_CALL destroy(void* instance) {
        record();
        delete static_cast<Stream*>(instance);
        std::lock_guard<std::mutex> lock(mutex);
        --instances;
    }
    mc_result MC_CALL open(void* instance, mc_string_view uri, mc_stream_open_flags, mc_error**) {
        record();
        auto& stream = *static_cast<Stream*>(instance);
        stream.uri.assign(uri.data, uri.length);
        if (stream.uri == "testmedia:block-open") {
            return block(stream);
        }
        if (stream.uri == "testmedia:open-fail") {
            return MC_ERROR_IO;
        }
        stream.opened = true;
        return MC_OK;
    }
    mc_result MC_CALL close(void* instance) {
        record();
        auto& stream = *static_cast<Stream*>(instance);
        stream.opened = false;
        return stream.uri == "testmedia:close-fail" ? MC_ERROR_IO : MC_OK;
    }
    void MC_CALL interrupt(void* instance) {
        auto& stream = *static_cast<Stream*>(instance);
        std::lock_guard<std::mutex> lock(stream.mutex);
        stream.interrupted = true;
        stream.changed.notify_all();
    }
    mc_result MC_CALL info(void* instance, mc_stream_info* info) {
        record();
        auto& stream = *static_cast<Stream*>(instance);
        if (!stream.opened) {
            return MC_ERROR_IO;
        }
        info->flags = MC_STREAM_READABLE | MC_STREAM_WRITABLE | MC_STREAM_LENGTH_KNOWN;
        if (stream.uri == "testmedia:unknown-length") {
            info->flags &= ~MC_STREAM_LENGTH_KNOWN;
        }
        info->length_bytes = 4;
        info->uri = { stream.uri.data(), stream.uri.size() };
        info->extension = { "bytes", 5 };
        return MC_OK;
    }
    mc_result MC_CALL read(
        void* instance, void* data, size_t capacity, size_t* count, mc_read_state* state, mc_error**)
    {
        record();
        auto& stream = *static_cast<Stream*>(instance);
        if (stream.uri == "testmedia:block-read" || stream.uri == "testmedia:block-decode") {
            return block(stream);
        }
        const unsigned char bytes[] = { 64, 128, 192, 255 };
        *count = std::min(capacity, sizeof(bytes) - stream.position);
        std::memcpy(data, bytes + stream.position, *count);
        stream.position += *count;
        *state = stream.position == sizeof(bytes) ? MC_READ_END : MC_READ_MORE;
        return MC_OK;
    }
    mc_result MC_CALL write(void* instance, const void*, size_t size, size_t* count, mc_error**) {
        record();
        auto& stream = *static_cast<Stream*>(instance);
        if (stream.uri == "testmedia:block-write") {
            return block(stream);
        }
        *count = size ? 1 : 0;
        stream.position += *count;
        return stream.uri == "testmedia:write-fail" ? MC_ERROR_IO : MC_OK;
    }
    mc_result MC_CALL seek(void*, uint64_t, mc_error**) {
        record();
        return MC_ERROR_UNSUPPORTED;
    }
    mc_result MC_CALL tell(void* instance, uint64_t* position) {
        record();
        *position = static_cast<Stream*>(instance)->position;
        return MC_OK;
    }
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
    void MC_CALL shutdown(void*) {
        std::lock_guard<std::mutex> lock(mutex);
        if (instances) {
            std::fputs("application media survived provider shutdown\n", stderr);
            std::abort();
        }
        ++shutdowns;
    }
}

extern "C" MC_PLUGIN_API int test_media_control(int operation) {
    std::unique_lock<std::mutex> lock(mutex);
    switch (operation) {
        case 0:
            return calls;
        case 1:
            return instances;
        case 2:
            return changed.wait_for(lock, std::chrono::seconds(5),
                       [] {
                           return entered;
                       })
                ? 1
                : 0;
        case 3:
            entered = false;
            return 0;
        case 4:
            return shutdowns;
        case 5:
            failCreation = true;
            return 0;
        default:
            return -1;
    }
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "test.application.media", 22 };
    plugin->name = { "Application media fixture", 25 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
