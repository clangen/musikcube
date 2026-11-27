#include "PluginMediaService.h"

#include "Plugins.h"
#include <musikcore/audio/Dsp.h>
#include <common/Contract.h>
#include <musikcore/ffi/common/error.h>
#include <musikcore/ffi/plugin_abi/AudioScratchBuffers.h>
#include <musikcore/io/LocalFileSource.h>
#include <musikcore/support/Common.h>
#include <cstring>

namespace musik { namespace core { namespace plugin {
    namespace {
        /* a declined hint is the only reason to try the next provider. once
        a factory claims a resource its error belongs to that resource. */
        template<typename Factory, typename Instance>
        mc_result tryCreate(const Factory& factory, const mc_plugin_services& services,
            const mc_media_hint& hint, Instance& output, bool& claimed, mc_error** error)
        {
            if (!factory) {
                return MC_OK;
            }
            return factory.Invoke([&](const auto& table) -> mc_result {
                uint32_t supported = 0;
                const auto probe = table.can_handle(table.context, &hint, &supported);
                if (probe != MC_OK || !supported) {
                    return probe;
                }
                claimed = true;
                void* instance = nullptr;
                const auto status = table.create(table.context, &services, &instance, error);
                if (status != MC_OK) {
                    musik::contract::Assert(!instance, "failed media factory returned an instance");
                    return status;
                }
                if (!instance) {
                    return MC_ERROR_PLUGIN;
                }
                output.instance = instance;
                output.api = table.instance_api;
                return MC_OK;
            });
        }

        /* HTTP media types are case-insensitive and can carry parameters.
        give every decoder the same MIME hint, including on an extensionless URL. */
        std::string normalizedMediaType(mc_string_view view) {
            const std::string_view text(view.data ? view.data : "", view.length);
            const auto start = text.find_first_not_of(" \t");
            if (start == std::string_view::npos) {
                return {};
            }
            const auto type = text.substr(start);
            std::string result(type.substr(0, type.find_first_of("; \t")));
            for (char& c : result) {
                if (c >= 'A' && c <= 'Z') {
                    c += 'a' - 'A';
                }
            }
            return result;
        }

        bool validView(mc_string_view text) {
            return !text.length || (text.data && !std::memchr(text.data, 0, text.length));
        }
    }

    PluginMediaService::PluginMediaService(const ffi::PluginRegistry& registry)
    : dataDirectory(GetDataDirectory())
    , api {
        .struct_size = sizeof(mc_plugin_media_service),
        .context = this,
        .create_stream = CreateStream,
        .create_decoder = CreateDecoder,
        .create_encoder = CreateEncoder,
    } {
        const auto plugins = registry.ListPlugins();
        /* services contain pointers into these entries. finalize storage before
        publishing the first address to a factory. */
        this->providers.reserve(plugins.size());
        for (const auto& plugin : plugins) {
            auto streams = registry.Find<mc_stream_factory>(plugin.id);
            auto decoders = registry.Find<mc_decoder_factory>(plugin.id);
            auto encoders = registry.Find<mc_encoder_factory>(plugin.id);
            auto dsp = registry.Find<mc_dsp_factory>(plugin.id);
            if (!plugin.enabled || (!streams && !decoders && !encoders && !dsp)) {
                continue;
            }
            auto& provider = this->providers.emplace_back();
            provider.streams = streams;
            provider.decoders = decoders;
            provider.encoders = encoders;
            provider.dsp = dsp;
            provider.settings = SettingsService(plugin.name);
            provider.services = {
                .struct_size = sizeof(mc_plugin_services),
                .cancel = nullptr,
                .audio_scratch = &ffi::audio_scratch_buffers::Host(),
                .media = &this->api,
                .data_directory = { this->dataDirectory.data(), this->dataDirectory.size() },
                .settings = &provider.settings,
            };
        }
    }

    std::vector<std::shared_ptr<audio::Dsp>> PluginMediaService::CreateDsps() const {
        std::vector<std::shared_ptr<audio::Dsp>> result;
        for (const auto& provider : this->providers) {
            if (provider.dsp) {
                auto dsp = std::make_shared<audio::Dsp>(provider.dsp, provider.services);
                if (dsp->IsValid()) {
                    result.push_back(std::move(dsp));
                }
            }
        }
        return result;
    }

    mc_result MC_CALL PluginMediaService::CreateStream(void* context, mc_string_view uri, mc_stream* output, mc_error** error) {
        return ffi::exception_barrier(error, [&]() -> mc_result {
            if (!output || output->struct_size < sizeof(*output) || output->instance || !uri.length || !validView(uri)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            auto& host = *static_cast<PluginMediaService*>(context);
            mc_media_hint hint = MC_MEDIA_HINT_DEFAULT;
            hint.uri = uri;
            for (const auto& provider : host.providers) {
                bool claimed = false;
                const auto status = tryCreate(provider.streams, provider.services, hint, *output, claimed, error);
                if (status != MC_OK || claimed) {
                    return status;
                }
            }
            return io::CreateLocalFileSource(*output);
        });
    }

    mc_result MC_CALL PluginMediaService::CreateDecoder(void* context, const mc_stream* input, mc_decoder* output, mc_error** error) {
        return ffi::exception_barrier(error, [&]() -> mc_result {
            if (!output || output->struct_size < sizeof(*output) || output->instance || !input ||
                input->struct_size < sizeof(*input) || !input->instance || !input->api ||
                input->api->struct_size < sizeof(mc_stream_vtable) || !input->api->get_info ||
                !input->api->read || !input->api->seek || !input->api->tell)
            {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            mc_stream_info info = MC_STREAM_INFO_DEFAULT;
            const auto status = input->api->get_info(input->instance, &info);
            if (status != MC_OK) {
                return status;
            }
            if (info.struct_size < sizeof(info) || !validView(info.uri) || !validView(info.content_type) ||
                !validView(info.extension))
            {
                return MC_ERROR_PLUGIN;
            }
            if (!(info.flags & MC_STREAM_READABLE)) {
                return MC_ERROR_UNSUPPORTED;
            }
            const auto mime = normalizedMediaType(info.content_type);
            const mc_media_hint hint {
                .struct_size = sizeof(mc_media_hint),
                .uri = info.uri,
                .content_type = { mime.data(), mime.size() },
                .extension = info.extension,
            };
            auto& host = *static_cast<PluginMediaService*>(context);
            for (const auto& provider : host.providers) {
                bool claimed = false;
                const auto result = tryCreate(provider.decoders, provider.services, hint, *output, claimed, error);
                if (result != MC_OK || claimed) {
                    return result;
                }
            }
            return MC_ERROR_UNSUPPORTED;
        });
    }
    mc_result MC_CALL PluginMediaService::CreateEncoder(void* context, mc_string_view format, mc_encoder* output, mc_error** error) {
        return ffi::exception_barrier(error, [&]() -> mc_result {
            if (!output || output->struct_size < sizeof(*output) || output->instance ||
                !format.length || !validView(format))
            {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            auto& host = *static_cast<PluginMediaService*>(context);
            for (const auto& provider : host.providers) {
                if (!provider.encoders) {
                    continue;
                }
                bool claimed = false;
                const auto status = provider.encoders.Invoke([&](const auto& factory) -> mc_result {
                    uint32_t supported = 0;
                    const auto result = factory.can_handle(factory.context, format, &supported);
                    if (result != MC_OK || !supported) {
                        return result;
                    }
                    claimed = true;
                    mc_encoder created = MC_ENCODER_DEFAULT;
                    const auto made = factory.create(factory.context, &provider.services, format, &created, error);
                    if (made != MC_OK) {
                        musik::contract::Assert(!created.instance, "failed encoder factory returned an instance");
                        return made;
                    }
                    if (!created.instance || !created.api ||
                        reinterpret_cast<uintptr_t>(created.api) % alignof(mc_encoder_vtable) ||
                        created.api->struct_size < sizeof(mc_encoder_vtable) || !created.api->destroy ||
                        !created.api->initialize || !created.api->encode || !created.api->flush ||
                        !created.api->finalize || created.api->streaming > 1)
                    {
                        musik::contract::Assert(false, "encoder factory returned an invalid instance");
                    }
                    *output = created;
                    return MC_OK;
                });
                if (status != MC_OK || claimed) {
                    return status;
                }
            }
            return MC_ERROR_UNSUPPORTED;
        });
    }
} } }
