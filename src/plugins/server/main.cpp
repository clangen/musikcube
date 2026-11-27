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

#include "Constants.h"
#include "Context.h"
#include "HttpServer.h"
#include "WebSocketServer.h"

#include <musikcore/plugin.h>
#include <musikcore/plugin/playback-remote.hpp>
#include <common/version.h>

#include <cstring>

namespace {
    mc_string_view text(const char* value) {
        return { value, std::strlen(value) };
    }

    const mc_configuration_api& configuration() {
        static const mc_integer_setting integers[] {
            {
                .key = text(serverprefs::MetadataPort.c_str()),
                .default_value = serverprefs::DefaultMetadataPort,
                .minimum = 1,
                .maximum = 65535,
            },
            {
                .key = text(serverprefs::AudioPort.c_str()),
                .default_value = serverprefs::DefaultAudioPort,
                .minimum = 1,
                .maximum = 65535,
            },
            {
                .key = text(serverprefs::CacheCount.c_str()),
                .default_value = serverprefs::DefaultCacheCount,
                .minimum = 0,
                .maximum = 100000,
            },
            {
                .key = text(serverprefs::ActiveTranscodeLimit.c_str()),
                .default_value = serverprefs::DefaultActiveTranscodeLimit,
                .minimum = 0,
                .maximum = 100000,
            },
        };
        static const mc_boolean_setting booleans[] {
            { .key = text(serverprefs::MetadataEnabled.c_str()), .default_value = true },
            { .key = text(serverprefs::AudioEnabled.c_str()), .default_value = true },
            { .key = text(serverprefs::UseIpv6.c_str()), .default_value = defaults::use_ipv6 },
            { .key = text(serverprefs::SynchronousTranscoding.c_str()), .default_value = defaults::transcoder_synchronous },
            { .key = text(prefs::transcoder_synchronous_fallback.c_str()), .default_value = defaults::transcoder_synchronous_fallback },
        };
        static const mc_text_setting texts[] {
            { .key = text(serverprefs::Password.c_str()), .default_value = text("") },
        };
        static const mc_configuration_api value {
            .struct_size = sizeof(mc_configuration_api),
            .integers = integers,
            .integer_count = sizeof(integers) / sizeof(integers[0]),
            .booleans = booleans,
            .boolean_count = sizeof(booleans) / sizeof(booleans[0]),
            .numbers = nullptr,
            .number_count = 0,
            .texts = texts,
            .text_count = sizeof(texts) / sizeof(texts[0]),
        };

        return value;
    }

    /* owns both network servers for one playback session. */
    class ServerRemote {
    public:
        explicit ServerRemote(const mc_plugin_services& services)
        : httpServer(this->context)
        , webSocketServer(this->context)
        {
            this->context.settingsService = *services.settings;
            this->context.dataDirectory.assign(services.data_directory.data ? services.data_directory.data : "",
                services.data_directory.length);
            this->context.host = services.host;
            this->context.library = services.library;
            this->context.media = services.media;
            this->context.scratch = services.audio_scratch;
            this->context.playback = services.playback;
            this->context.indexer = services.indexer;
        }

        void Changed(const mc_plugin_playback_event&) noexcept {
            this->webSocketServer.PostPlaybackChanged(false);
        }

        void QueueChanged() noexcept {
            this->webSocketServer.PostPlaybackChanged(true);
        }

        mc_result Start() {
            this->context.stopping.store(false);
            if (this->context.settings.Boolean(serverprefs::AudioEnabled, true) && !this->httpServer.Start()) {
                return MC_ERROR_IO;
            }
            if (this->context.settings.Boolean(serverprefs::MetadataEnabled, true) && !this->webSocketServer.Start()) {
                return MC_ERROR_IO;
            }
            return MC_OK;
        }

        void RequestStop() noexcept {
            /* the owner cancels pending playback calls before joining workers. */
            this->context.stopping.store(true);
        }

        mc_result Join() {
            const bool httpStopped = this->httpServer.Stop();
            const bool websocketStopped = this->webSocketServer.Stop();
            return httpStopped && websocketStopped ? MC_OK : MC_ERROR_IO;
        }

    private:
        Context context;
        HttpServer httpServer;
        WebSocketServer webSocketServer;
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void**, mc_error**) {
        const auto configured = musik::core::plugin::RegisterConfiguration(*host, configuration());
        if (configured != MC_OK) {
            return configured;
        }
        return musik::core::plugin::RegisterPlaybackRemote<ServerRemote>(*host);
    }

    void MC_CALL shutdown(void*) {}
}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto status = mc_plugin_validate_host_api(version, host, plugin, error);
    if (status != MC_OK) {
        return status;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = text(serverprefs::PluginGuid);
    plugin->name = text("musikcube Server (wss, http)");
    plugin->version = text(MUSIKCUBE_VERSION_WITH_COMMIT_HASH);
    plugin->author = text("clangen");
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}
