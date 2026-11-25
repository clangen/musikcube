#include "FfmpegEncoder.h"
#include "LameEncoder.h"
#include <musikcore/plugin.h>
#include <musikcore/plugin/registration.hpp>
#include <musikcore/plugin/encoder.hpp>
#include <common/version.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <string_view>

namespace {
    const std::set<std::string> formats {
        ".mp3", "audio/mpeg", ".ogg", "audio/ogg", ".opus", ".flac", "audio/flac",
        ".alac", ".aac", "audio/aac", ".m4a", ".wma", "audio/x-ms-wma", ".wv"
    };

    std::string normalize(mc_string_view text) {
        std::string result(text.data, text.length);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return result;
    }

    template<typename Work> mc_result invoke(Work&& work) noexcept {
        try {
            return work();
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    struct PluginState {
        mc_host_api host;
        mc_encoder_factory factory;
    };

    const mc_encoder_factory factoryTemplate {
        sizeof(mc_encoder_factory),
        nullptr,
        [](void*, mc_string_view format, uint32_t* supported) {
            return invoke([&]() -> mc_result {
                *supported = formats.count(normalize(format)) ? 1 : 0;
                return MC_OK;
            });
        },
        [](void* context, const mc_plugin_services*, mc_string_view format, mc_encoder* output, mc_error**) {
            return invoke([&]() -> mc_result {
                auto type = normalize(format);
                if (type == ".mp3" || type == "audio/mpeg") {
                    output->instance = new LameEncoder();
                    output->api = &musik::core::plugin::StreamingEncoderApi<LameEncoder>();
                }
                else {
                    output->instance = new FfmpegEncoder(type, static_cast<PluginState*>(context)->host);
                    output->api = &musik::core::plugin::BlockingEncoderApi<FfmpegEncoder>();
                }
                return MC_OK;
            });
        },
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void** context, mc_error**) {
        return invoke([&]() -> mc_result {
            auto* state = new PluginState { *host, factoryTemplate };
            *context = state;
            state->factory.context = state;
            return musik::core::plugin::RegisterEncoder(*host, state->factory);
        });
    }
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "d4d13803-a285-4481-ad1e-106131e0d523", 36 };
    plugin->name = { "Stock Encoders (lame + ffmpeg)", sizeof("Stock Encoders (lame + ffmpeg)") - 1 };
    plugin->version = { MUSIKCUBE_VERSION_WITH_COMMIT_HASH, sizeof(MUSIKCUBE_VERSION_WITH_COMMIT_HASH) - 1 };
    plugin->author = { "clangen", 7 };
    plugin->initialize = initialize;
    plugin->shutdown = [](void* state) { delete static_cast<PluginState*>(state); };
    return MC_OK;
}
