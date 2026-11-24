#include "CddaPcmDecoder.h"
#include "CddaStream.h"
#include "CddaIndexer.h"

#include <musikcore/plugin/registration.hpp>
#include <musikcore/sdk/version.h>

namespace {
    template<typename Operation> mc_result Call(Operation&& operation) noexcept {
        try {
            return operation();
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    mc_result MC_CALL CanHandle(void*, const mc_media_hint* hint, uint32_t* supported) {
        *supported = CddaStream::CanHandle(*hint);
        return MC_OK;
    }

    mc_result MC_CALL Create(void*, const mc_plugin_services*, void** instance, mc_error**) {
        *instance = nullptr;
        return Call([&]() -> mc_result {
            *instance = new CddaStream;
            return MC_OK;
        });
    }

    const mc_stream_vtable StreamMethods {
        .struct_size = sizeof(mc_stream_vtable),
        .destroy =
            [](void* instance) {
                delete static_cast<CddaStream*>(instance);
            },
        .open =
            [](void* instance, mc_string_view uri, mc_stream_open_flags flags, mc_error**) {
                return Call([&] {
                    return static_cast<CddaStream*>(instance)->Open(uri, flags);
                });
            },
        .close =
            [](void* instance) {
                return Call([&] {
                    return static_cast<CddaStream*>(instance)->Close();
                });
            },
        .interrupt =
            [](void* instance) {
                static_cast<CddaStream*>(instance)->Interrupt();
            },
        .get_info =
            [](void* instance, mc_stream_info* info) {
                return static_cast<CddaStream*>(instance)->Info(*info);
            },
        .read =
            [](void* instance, void* destination, size_t capacity, size_t* bytes, mc_read_state* state, mc_error**) {
                return Call([&] {
                    return static_cast<CddaStream*>(instance)->Read(destination, capacity, *bytes, *state);
                });
            },
        .write = [](void*, const void*, size_t, size_t* bytes, mc_error**) -> mc_result {
            *bytes = 0;
            return MC_ERROR_UNSUPPORTED;
        },
        .seek =
            [](void* instance, uint64_t position, mc_error**) {
                return static_cast<CddaStream*>(instance)->Seek(position);
            },
        .tell =
            [](void* instance, uint64_t* position) {
                return static_cast<CddaStream*>(instance)->Tell(*position);
            },
    };

    const mc_stream_factory StreamFactory {
        .struct_size = sizeof(mc_stream_factory),
        .context = nullptr,
        .instance_api = &StreamMethods,
        .can_handle = CanHandle,
        .create = Create,
    };

    mc_result MC_CALL Initialize(const mc_host_api* host, void** state, mc_error**) {
        *state = nullptr;
        auto result = musik::core::plugin::RegisterStream(*host, StreamFactory);
        if (result != MC_OK) {
            return result;
        }
        result = musik::core::plugin::RegisterDecoder<CddaPcmDecoder>(*host);
        if (result != MC_OK) {
            return result;
        }
        return musik::core::plugin::RegisterIndexerSource<CddaIndexer>(*host);
    }

    void MC_CALL Shutdown(void*) {
    }
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error) {
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "0862b76d-67cd-4e54-b5d1-6a2c8e5101a4", 36 };
    plugin->name = { "CD Audio IDecoder, IDataStream", sizeof("CD Audio IDecoder, IDataStream") - 1 };
    plugin->version = { MUSIKCUBE_VERSION_WITH_COMMIT_HASH, sizeof(MUSIKCUBE_VERSION_WITH_COMMIT_HASH) - 1 };
    plugin->author = { "musikcube", 9 };
    plugin->initialize = Initialize;
    plugin->shutdown = Shutdown;
    return MC_OK;
}
