#ifndef MUSIKCORE_SERVER_H
#define MUSIKCORE_SERVER_H

#include <musikcore/abi.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    MC_SERVER_DEFAULT_METADATA_PORT = 7905,
    MC_SERVER_DEFAULT_AUDIO_PORT = 7906,
    MC_SERVER_DEFAULT_CACHE_COUNT = 50,
    MC_SERVER_DEFAULT_ACTIVE_TRANSCODE_LIMIT = 4
};

typedef struct mc_server_configuration {
    /* Enable the metadata WebSocket listener. */
    bool metadata_enabled;
    /* Enable the HTTP audio listener. */
    bool audio_enabled;
    /* Bind the listeners on IPv6. */
    bool ipv6;
    /* Transcode audio synchronously. */
    bool synchronous_transcoding;
    /* Metadata WebSocket TCP port. Zero is invalid. */
    uint16_t metadata_port;
    /* HTTP audio TCP port. Zero is invalid. */
    uint16_t audio_port;
    /* Maximum number of cached transcodes. Zero disables caching. */
    int32_t cache_count;
    /* Maximum simultaneous transcodes. Zero disables transcoding. */
    int32_t active_transcode_limit;
    /* Authentication password. Valid through the call or read callback. */
    mc_string_view password;
} mc_server_configuration;

typedef void (MC_CALL *mc_server_configuration_fn)(void* user_data, const mc_server_configuration* configuration);

/* Use on the runtime owner thread. Returns MC_ERROR_UNSUPPORTED without the
   server plugin. Configuration is borrowed for the duration of read. */
MC_API mc_result MC_CALL mc_server_read_configuration(mc_server_configuration_fn read, void* user_data);
/* Copy and persist the configuration, then reload the server plugin. */
MC_API mc_result MC_CALL mc_server_configure(const mc_server_configuration* configuration);

#ifdef __cplusplus
}
#endif

#endif
