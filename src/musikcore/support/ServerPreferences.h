#pragma once

#include <musikcore/server.h>
#include <string>

namespace musik { namespace core { namespace serverprefs {
    inline constexpr char PluginGuid[] = "9fc897a3-dfd5-4524-a0fc-b02f46aea4a9";

    inline const std::string MetadataEnabled = "websocket_server_enabled";
    inline const std::string MetadataPort = "websocket_server_port";
    inline const std::string AudioEnabled = "http_server_enabled";
    inline const std::string AudioPort = "http_server_port";
    inline const std::string UseIpv6 = "use_ipv6";
    inline const std::string CacheCount = "transcoder_cache_count";
    inline const std::string ActiveTranscodeLimit = "transcoder_max_active_count";
    inline const std::string SynchronousTranscoding = "transcoder_synchronous";
    inline const std::string Password = "password";

    inline constexpr int DefaultMetadataPort = MC_SERVER_DEFAULT_METADATA_PORT;
    inline constexpr int DefaultAudioPort = MC_SERVER_DEFAULT_AUDIO_PORT;
    inline constexpr int DefaultCacheCount = MC_SERVER_DEFAULT_CACHE_COUNT;
    inline constexpr int DefaultActiveTranscodeLimit = MC_SERVER_DEFAULT_ACTIVE_TRANSCODE_LIMIT;
} } }
