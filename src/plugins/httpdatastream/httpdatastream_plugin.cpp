#include <musikcore/plugin/registration.hpp>
//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "config.h"
#include "HttpDataStream.h"
#include <musikcore/plugin.h>
#include <common/version.h>

namespace {
    constexpr mc_string_view text(std::string_view s) {
        return {
            .data = s.data(),
            .length = s.size(),
        };
    }

    template<typename Operation> mc_result call(Operation operation) noexcept {
        try {
            return operation();
        }
        catch (const std::filesystem::filesystem_error&) {
            return MC_ERROR_IO;
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    bool startsWith(std::string_view uri, std::string_view scheme) {
        if (uri.size() < scheme.size()) {
            return false;
        }
        for (size_t i = 0; i < scheme.size(); ++i) {
            const char c = uri[i];
            if ((c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c) != scheme[i]) {
                return false;
            }
        }
        return true;
    }

    mc_result MC_CALL canHandle(void*, const mc_media_hint* hint, uint32_t* supported) {
        const std::string_view uri(hint->uri.data ? hint->uri.data : "", hint->uri.length);
        *supported = startsWith(uri, "http://") || startsWith(uri, "https://") ||
            uri.substr(0, HttpDataStream::RemoteTrack.size()) == HttpDataStream::RemoteTrack;
        return MC_OK;
    }

    struct Provider;
    mc_result createStream(Provider&, const mc_plugin_services&, void**);

    mc_result MC_CALL create(void* context, const mc_plugin_services* services, void** instance, mc_error**) {
        if (!services) {
            return MC_ERROR_UNSUPPORTED;
        }
        return call([&] {
            return createStream(*static_cast<Provider*>(context), *services, instance);
        });
    }

    void MC_CALL destroy(void* input) {
        delete static_cast<HttpDataStream*>(input);
    }

    mc_result MC_CALL open(void* input, mc_string_view uri, mc_stream_open_flags flags, mc_error**) {
        return call([&] {
            return static_cast<HttpDataStream*>(input)->Open(uri, flags);
        });
    }

    mc_result MC_CALL close(void* input) {
        return call([&] {
            return static_cast<HttpDataStream*>(input)->Close();
        });
    }

    void MC_CALL interrupt(void* input) {
        static_cast<HttpDataStream*>(input)->Interrupt();
    }

    mc_result MC_CALL info(void* input, mc_stream_info* result) {
        return static_cast<HttpDataStream*>(input)->Info(*result);
    }

    mc_result MC_CALL read(void* input, void* dst, size_t capacity, size_t* bytes, mc_read_state* state, mc_error**) {
        return static_cast<HttpDataStream*>(input)->Read(dst, capacity, *bytes, *state);
    }

    mc_result MC_CALL write(void*, const void*, size_t, size_t* bytes, mc_error**) {
        *bytes = 0;
        return MC_ERROR_UNSUPPORTED;
    }

    mc_result MC_CALL seek(void* input, uint64_t offset, mc_error**) {
        return static_cast<HttpDataStream*>(input)->Seek(offset);
    }

    mc_result MC_CALL tell(void* input, uint64_t* offset) {
        return static_cast<HttpDataStream*>(input)->Tell(*offset);
    }

    constexpr mc_stream_vtable Methods {
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

    /* active streams share a cache index for this engine session. the registry
    keeps the immutable factory and its cache alive through the last destroy. */
    struct Provider {
        explicit Provider(const mc_host_api& host) : host(host) {}
        const mc_host_api& host;
        LruDiskCache cache;
        mc_stream_factory factory {
            .struct_size = sizeof(mc_stream_factory),
            .context = this,
            .instance_api = &Methods,
            .can_handle = canHandle,
            .create = create,
        };
    };

    mc_result createStream(Provider& provider, const mc_plugin_services& services, void** instance) {
        return HttpDataStream::Create(provider.cache, provider.host, services, instance);
    }

    mc_result MC_CALL initialize(const mc_host_api* host, void** context, mc_error**) {
        return call([&]() -> mc_result {
            auto* provider = new Provider(*host);
            *context = provider;
            const auto configured =
                musik::core::plugin::RegisterConfiguration(*host, HttpDataStream::Configuration());
            if (configured != MC_OK) {
                return configured;
            }
            return musik::core::plugin::RegisterStream(*host, provider->factory);
        });
    }

    void MC_CALL shutdown(void* context) {
        delete static_cast<Provider*>(context);
    }
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error) {
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = text("b153adad-ee98-4331-ad32-4ff7f34828cd");
    plugin->name = text("HTTP stream");
    plugin->author = text("clangen");
    plugin->version = text(MUSIKCUBE_VERSION_WITH_COMMIT_HASH);
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
