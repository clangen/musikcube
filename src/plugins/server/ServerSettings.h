#pragma once

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

    inline constexpr int DefaultMetadataPort = 7905;
    inline constexpr int DefaultAudioPort = 7906;
    inline constexpr int DefaultCacheCount = 50;
    inline constexpr int DefaultActiveTranscodeLimit = 4;
} } }
