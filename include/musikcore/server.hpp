#ifndef MUSIKCORE_SERVER_HPP
#define MUSIKCORE_SERVER_HPP

#include <musikcore/context.hpp>
#include <musikcore/server.h>

namespace musik { namespace core { namespace api {
    /* Owns a copy of the server settings and password. Copyable and movable. */
    struct ServerConfiguration {
        bool metadataEnabled;
        bool audioEnabled;
        bool ipv6;
        bool synchronousTranscoding;
        uint16_t metadataPort;
        uint16_t audioPort;
        int32_t cacheCount;
        int32_t activeTranscodeLimit;
        std::string password;
    };

    inline std::optional<ServerConfiguration> ReadServerConfiguration() {
        std::optional<ServerConfiguration> configuration;
        const auto status = mc_server_read_configuration([](void* data, const mc_server_configuration* source) {
            auto& result = *static_cast<std::optional<ServerConfiguration>*>(data);
            result = ServerConfiguration {
                source->metadata_enabled, source->audio_enabled, source->ipv6, source->synchronous_transcoding,
                source->metadata_port, source->audio_port, source->cache_count, source->active_transcode_limit,
                std::string(AsStringView(source->password))
            };
        }, &configuration);
        if (status != MC_OK && status != MC_ERROR_UNSUPPORTED) {
            throw Error(status);
        }
        return configuration;
    }

    inline mc_result ConfigureServer(const ServerConfiguration& configuration) {
        const mc_server_configuration options {
            configuration.metadataEnabled, configuration.audioEnabled, configuration.ipv6,
            configuration.synchronousTranscoding, configuration.metadataPort, configuration.audioPort,
            configuration.cacheCount, configuration.activeTranscodeLimit,
            { configuration.password.data(), configuration.password.size() }
        };
        return mc_server_configure(&options);
    }
} } }

#endif
