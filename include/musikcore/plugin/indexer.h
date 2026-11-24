#ifndef MUSIKCORE_PLUGIN_INDEXER_H
#define MUSIKCORE_PLUGIN_INDEXER_H

#include <musikcore/plugin/metadata.h>
#include <musikcore/plugin/services.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_INDEXER_SOURCE "org.musikcube.indexer_source"

/* the largest progress increment accepted by one checkpoint. */
#define MC_INDEXER_PROGRESS_MAX INT32_MAX

/* a track assembled by an indexer source before it is saved to the library. */
typedef struct mc_metadata_record mc_metadata_record;

/* lets an indexer source request a later rescan when its external data changes.
the service remains valid until the source instance is destroyed. */
struct mc_indexer_source_host_api {
    /* available bytes. initialize with MC_INDEXER_SOURCE_HOST_API_DEFAULT. */
    uint32_t struct_size;

    /* host-owned source state, passed unchanged to request_rescan. */
    void* context;

    /* enqueue a scan of this source; safe to call from source worker threads.
    requests made during startup are queued; shutdown returns MC_ERROR_CANCELLED. */
    mc_result (MC_CALL* request_rescan)(void* context);
};

#define MC_INDEXER_SOURCE_HOST_API_DEFAULT { sizeof(mc_indexer_source_host_api), NULL, NULL }

/* writes tracks owned by one source during its current scan. records and
writers are valid only until the scan callback returns. i/o and internal errors
from these operations prevent later checkpoints and roll back the current batch,
even if the source returns success. */
typedef struct mc_indexer_writer {
    /* available bytes. initialize with MC_INDEXER_WRITER_DEFAULT. */
    uint32_t struct_size;

    /* host-owned scan state, passed unchanged to each operation. */
    void* context;

    /* create a fresh record; failure leaves the output null. */
    mc_result (MC_CALL* create_record)(void* context, mc_metadata_record** record);

    /* obtain a call-scoped writer for a record created by this scan. */
    mc_result (MC_CALL* get_writer)(void* context, mc_metadata_record* record, mc_metadata_writer* writer);

    /* release a record and clear the caller's pointer. */
    void (MC_CALL* destroy_record)(void* context, mc_metadata_record** record);

    /* save a record using a nonempty, source-specific external ID. */
    mc_result (MC_CALL* save)(void* context, const mc_metadata_record* record, mc_string_view external_id);

    /* remove this source's tracks with the specified URI. */
    mc_result (MC_CALL* remove_uri)(void* context, mc_string_view uri);

    /* remove this source's tracks with the specified external ID. */
    mc_result (MC_CALL* remove_external_id)(void* context, mc_string_view external_id);

    /* remove every track owned by this source. */
    mc_result (MC_CALL* remove_all)(void* context, uint64_t* removed);

    /* read the saved modification time, in the source's own units.
    MC_ERROR_NOT_FOUND means no matching track exists. */
    mc_result (MC_CALL* last_modified)(void* context, mc_string_view external_id, int64_t* modified);

    /* commit the current batch and report up to MC_INDEXER_PROGRESS_MAX updates.
    later rollback cannot undo this checkpoint, so scans must be resumable. */
    mc_result (MC_CALL* checkpoint)(void* context, uint64_t updated_tracks);
} mc_indexer_writer;

#define MC_INDEXER_WRITER_DEFAULT { sizeof(mc_indexer_writer), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

/* identifies a source across scans and describes how its tracks are maintained. */
typedef struct mc_indexer_source_info {
    /* available bytes. initialize with MC_INDEXER_SOURCE_INFO_DEFAULT. */
    uint32_t struct_size;

    /* stable UTF-8 key scoped to the plugin; the host copies it. */
    mc_string_view source_key;

    /* one if external IDs survive rescans and rebuilds, otherwise zero. */
    uint32_t stable_ids;

    /* one if the host should ask the source to validate existing tracks. */
    uint32_t validate_existing_tracks;

} mc_indexer_source_info;

#define MC_INDEXER_SOURCE_INFO_DEFAULT { sizeof(mc_indexer_source_info), { NULL, 0 }, 0, 0 }

/* a successful scan chooses whether to keep its uncheckpointed writes. */
typedef uint32_t mc_indexer_scan_decision;
enum { MC_INDEXER_SCAN_ROLLBACK = 0, MC_INDEXER_SCAN_COMMIT = 1 };

/* finds external tracks on the indexer worker. interrupt may run concurrently;
all other calls on one instance are serialized. */
typedef struct mc_indexer_source_vtable {
    /* available bytes. initialize with MC_INDEXER_SOURCE_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* destroy after scanning and interruption have both finished. */
    void (MC_CALL* destroy)(void* instance);

    /* describe the source; returned text remains valid until destroy. */
    mc_result (MC_CALL* get_info)(void* instance, mc_indexer_source_info* info);

    /* prepare for a scan before the host makes the instance interruptible. */
    mc_result (MC_CALL* begin_scan)(void* instance, mc_error** error);

    /* scan UTF-8 paths. paths and writer expire when this call returns. */
    mc_result (MC_CALL* scan)(void* instance, const mc_string_view* paths, size_t path_count,
        const mc_indexer_writer* writer, mc_indexer_scan_decision* decision, mc_error** error);

    /* inspect an existing track and remove it through writer when stale. */
    mc_result (MC_CALL* validate_track)(void* instance, mc_string_view external_id,
        const mc_indexer_writer* writer, mc_error** error);

    /* finish after begin_scan, including on cancellation or failure. */
    mc_result (MC_CALL* end_scan)(void* instance, mc_indexer_scan_decision decision, mc_error** error);

    /* request that an active scan return promptly; safe from another thread. */
    void (MC_CALL* interrupt)(void* instance);
} mc_indexer_source_vtable;

#define MC_INDEXER_SOURCE_VTABLE_DEFAULT { sizeof(mc_indexer_source_vtable), NULL, NULL, NULL, NULL, NULL, NULL, NULL }

/* creates independent indexer sources that remain valid until plugin shutdown. */
typedef struct mc_indexer_source_factory {
    /* available bytes. initialize with MC_INDEXER_SOURCE_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned factory state, passed unchanged to create. */
    void* context;

    /* immutable instance operations, valid until plugin shutdown. */
    const mc_indexer_source_vtable* instance_api;

    /* write the new source to instance; on failure set it to null. */
    mc_result (MC_CALL* create)(void* context, const mc_plugin_services* services,
        void** instance, mc_error** error);
} mc_indexer_source_factory;

#define MC_INDEXER_SOURCE_FACTORY_DEFAULT { sizeof(mc_indexer_source_factory), NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
