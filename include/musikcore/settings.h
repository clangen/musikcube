#ifndef MUSIKCORE_SETTINGS_H
#define MUSIKCORE_SETTINGS_H

#include <musikcore/setting_types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mc_settings_scope;
enum {
    MC_SETTINGS_APPLICATION,
    MC_SETTINGS_SESSION,
    MC_SETTINGS_HOTKEYS
};

/* Persisted settings shared by the core and its consumers. */
#define MC_SETTING_KEY_ASYNC_TRACK_LIST_QUERIES "AsyncTrackListQueries"
#define MC_SETTING_KEY_AUDDIO_API_TOKEN "AuddioApiToken"
#define MC_SETTING_KEY_INDEXER_LOG_ENABLED "IndexerLogEnabled"
#define MC_SETTING_KEY_INDEXER_THREAD_COUNT "IndexerThreadCount"
#define MC_SETTING_KEY_INDEXER_TRANSACTION_INTERVAL "IndexerTransactionInterval"
#define MC_SETTING_KEY_PLAYBACK_TRACK_QUERY_TIMEOUT_MS "PlaybackTrackQueryTimeoutMs"
#define MC_SETTING_KEY_REMOTE_LIBRARY_HOSTNAME "RemoteLibraryHostname"
#define MC_SETTING_KEY_REMOTE_LIBRARY_HTTP_PORT "RemoteLibraryHttpPort"
#define MC_SETTING_KEY_REMOTE_LIBRARY_HTTP_TLS "RemoteLibraryHttpTls"
#define MC_SETTING_KEY_REMOTE_LIBRARY_HTTP_TLS_ALLOW_UNTRUSTED_CERTIFICATE "RemoteLibraryHttpTlsAllowUntrustedCertificate"
#define MC_SETTING_KEY_REMOTE_LIBRARY_IGNORE_VERSION_MISMATCH "RemoteLibraryIgnoreVersionMismatch"
#define MC_SETTING_KEY_REMOTE_LIBRARY_LATENCY_TIMEOUT_MS "RemoteLibraryLatencyTimeoutMs"
#define MC_SETTING_KEY_REMOTE_LIBRARY_PASSWORD "RemoteLibraryPassword"
#define MC_SETTING_KEY_REMOTE_LIBRARY_TLS_WARNING_SUPPRESSED "RemoteLibraryTlsWarningSuppressed"
#define MC_SETTING_KEY_REMOTE_LIBRARY_TRANSCODER_BITRATE "RemoteLibraryTranscoderBitrate"
#define MC_SETTING_KEY_REMOTE_LIBRARY_TRANSCODER_ENABLED "RemoteLibraryTranscoderEnabled"
#define MC_SETTING_KEY_REMOTE_LIBRARY_TRANSCODER_FORMAT "RemoteLibraryTranscoderFormat"
#define MC_SETTING_KEY_REMOTE_LIBRARY_VIEWED "RemoteLibraryViewed"
#define MC_SETTING_KEY_REMOTE_LIBRARY_WSS_PORT "RemoteLibraryWssPort"
#define MC_SETTING_KEY_REMOTE_LIBRARY_WSS_TLS "RemoteLibraryWssTls"
#define MC_SETTING_KEY_REMOVE_MISSING_FILES "RemoveMissingFiles"
#define MC_SETTING_KEY_RESUME_PLAYBACK_ON_STARTUP "ResumePlaybackOnStartup"
#define MC_SETTING_KEY_SAVE_SESSION_ON_EXIT "SaveSessionOnExit"
#define MC_SETTING_KEY_SYNC_ON_STARTUP "SyncOnStartup"

typedef void (MC_CALL *mc_setting_read_fn)(void* user_data, const mc_setting_value* value);
typedef void (MC_CALL *mc_setting_key_fn)(void* user_data, mc_string_view key);

/* Read a value of the fallback's type. Missing or incompatible values use the
   fallback. Empty stored text uses a nonempty fallback. The callback runs
   inline with a borrowed value. These operations are thread-safe. */
MC_API mc_result MC_CALL mc_settings_read(mc_settings_scope scope, mc_string_view key,
    const mc_setting_value* fallback, mc_setting_read_fn read, void* user_data);
/* Copy a batch into memory as one update. Later duplicate keys take precedence.
   A zero count is valid. Invalid entries leave the store unchanged. */
MC_API mc_result MC_CALL mc_settings_write(mc_settings_scope scope,
    const mc_setting_entry* entries, size_t count);
/* Persist the current store. Returns MC_ERROR_IO on a write failure. */
MC_API mc_result MC_CALL mc_settings_save(mc_settings_scope scope);
/* Visit a snapshot of the store's keys. Each key is valid during its callback. */
MC_API mc_result MC_CALL mc_settings_list_keys(mc_settings_scope scope, mc_setting_key_fn read, void* user_data);

#ifdef __cplusplus
}
#endif

#endif
