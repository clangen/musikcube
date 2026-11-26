#ifndef MUSIKCORE_PLUGIN_LIBRARY_H
#define MUSIKCORE_PLUGIN_LIBRARY_H

#include <musikcore/library.h>
#include <musikcore/plugin/metadata.h>

#ifdef __cplusplus
extern "C" {
#endif

/* identifies one category value used to narrow a library query. */
typedef struct mc_library_category_selection {
    /* category field, including custom metadata fields. */
    mc_string_view field;
    /* local ID of the selected value. */
    int64_t id;
} mc_library_category_selection;

/* identifies an occurrence in a playlist, including duplicate tracks. */
typedef struct mc_library_playlist_entry_key {
    /* track external ID, valid for the duration of the call. */
    mc_string_view external_id;
    /* current position in the playlist. */
    int32_t position;
} mc_library_playlist_entry_key;

/* describes an album and the library identifiers needed to browse its artist. */
typedef struct mc_library_album {
    /* local album ID. */
    int64_t id;
    /* album title, valid during the callback. */
    mc_string_view title;
    /* local album artist ID. */
    int64_t artist_id;
    /* false when no artist ID is stored. */
    bool has_artist_id;
    /* album artist name, valid during the callback. */
    mc_string_view artist;
    /* local thumbnail ID, or zero when no artwork is available. */
    int64_t thumbnail_id;
    /* false when no thumbnail ID is stored. */
    bool has_thumbnail_id;
} mc_library_album;

/* receives albums and their callback-scoped text on the calling thread. */
typedef void (MC_CALL* mc_library_album_list_fn)(void* user_data,
    const mc_library_album* albums, size_t count);

/* receives text valid until this callback returns. */
typedef void (MC_CALL* mc_library_text_fn)(void* user_data, mc_string_view text);

/* lets a plugin read the local library using specific operations. calls may
block on the database and invoke their callbacks before returning. */
typedef struct mc_plugin_library_service {
    /* available bytes. initialize with MC_PLUGIN_LIBRARY_SERVICE_DEFAULT. */
    uint32_t struct_size;

    /* host-owned state; pass unchanged to each operation. */
    void* context;

    /* search visible tracks; limit -1 returns all matches. the caller owns the
    result and releases it with mc_track_result_destroy. */
    mc_result (MC_CALL* search_tracks)(void* context, const mc_track_search* search,
        int32_t limit, int32_t offset, mc_track_result** result);

    /* resolve external IDs in input order, omitting IDs absent from the library. */
    mc_result (MC_CALL* tracks_by_external_ids)(void* context, const mc_string_view* ids,
        size_t count, mc_track_result** result);

    /* create a result from local track IDs in the supplied order. */
    mc_result (MC_CALL* tracks_by_ids)(void* context, const int64_t* ids,
        size_t count, mc_track_result** result);

    /* search within one category or a playlist. a zero ID selects all tracks. */
    mc_result (MC_CALL* tracks_by_category)(void* context, mc_string_view field, int64_t id,
        mc_string_view filter, int32_t limit, int32_t offset, mc_track_result** result);

    /* find tracks matching every category selection. */
    mc_result (MC_CALL* tracks_by_categories)(void* context,
        const mc_library_category_selection* selections, size_t selection_count,
        mc_string_view filter, mc_track_search_order order, int32_t limit, int32_t offset,
        mc_track_result** result);

    /* read a page from this host's local library. the caller owns the page and
    releases it with mc_track_page_destroy. */
    mc_result (MC_CALL* read_tracks)(void* context, const mc_track_result* tracks,
        const mc_track_range* range, mc_track_page** page);

    /* read a page from the result's original library. inputs are copied before
    return. accepted requests complete once on the runtime owner thread, even
    during shutdown; rejected requests have no callback. a worker submission
    may complete before this function returns. success transfers the page. */
    mc_result (MC_CALL* read_tracks_async)(void* context, const mc_track_result* tracks,
        const mc_track_range* range, mc_track_query_completion_fn completion, void* user_data);

    /* run a registered named JSON query against the local library. arbitrary
    SQL is not accepted. on success completion runs once on the calling thread;
    the result JSON is valid only until completion returns. unknown names return
    MC_ERROR_UNSUPPORTED and malformed inputs return MC_ERROR_INVALID_ARGUMENT. */
    mc_result (MC_CALL* run_query)(void* context, mc_string_view query_json,
        mc_library_text_fn completion, void* user_data);

    /* visit the available category fields. a successful call invokes the
    callback once; a failed call returns an error without invoking it. the
    result views expire when the callback returns. do not call from a real-time
    audio thread. */
    mc_result (MC_CALL* list_category_fields)(void* context,
        mc_category_fields_completion_fn completion, void* user_data);

    /* list values for a category field, optionally filtered by text. result
    views remain valid until the callback returns. */
    mc_result (MC_CALL* list_categories)(void* context, mc_string_view field,
        mc_string_view filter, mc_track_search_mode mode,
        mc_category_list_completion_fn completion, void* user_data);

    /* list category values constrained by the supplied selections. */
    mc_result (MC_CALL* list_filtered_categories)(void* context, mc_string_view field,
        const mc_library_category_selection* selections, size_t selection_count,
        mc_string_view filter, mc_track_search_mode mode, bool facet, mc_string_view track_search,
        mc_category_list_completion_fn completion, void* user_data);

    /* list albums, optionally restricted by category selections. */
    mc_result (MC_CALL* list_albums)(void* context, mc_string_view filter,
        const mc_library_category_selection* selections, size_t selection_count,
        mc_library_album_list_fn completion, void* user_data);

    /* read encoded thumbnail bytes, valid during the callback. */
    mc_result (MC_CALL* read_thumbnail)(void* context, int64_t thumbnail_id,
        mc_thumbnail_completion_fn completion, void* user_data);

    /* create or replace a playlist from local track IDs; zero creates it. */
    mc_result (MC_CALL* save_playlist)(void* context, int64_t playlist_id, mc_string_view name,
        const int64_t* track_ids, size_t count, int64_t* saved_id);

    /* rename a playlist or append local track IDs at an offset; -1 appends. */
    mc_result (MC_CALL* rename_playlist)(void* context, int64_t playlist_id, mc_string_view name);
    mc_result (MC_CALL* append_playlist)(void* context, int64_t playlist_id,
        const int64_t* track_ids, size_t count, int32_t offset);

    /* remove specific occurrences and return the number removed. */
    mc_result (MC_CALL* remove_playlist_entries)(void* context, int64_t playlist_id,
        const mc_library_playlist_entry_key* entries, size_t count, size_t* removed);

    /* delete the playlist identified by a positive local library ID. */
    mc_result (MC_CALL* delete_playlist)(void* context, int64_t playlist_id);

    /* read artwork from the supplied library identity, including a remote
    origin. this blocking worker call is cancelled when the host stops. the
    callback receives borrowed bytes only on success; keep library alive until
    return. IDs are scoped to that library. */
    mc_result (MC_CALL* read_library_thumbnail)(void* context, const mc_library* library,
        int64_t thumbnail_id, mc_thumbnail_completion_fn completion, void* user_data);
} mc_plugin_library_service;

#define MC_PLUGIN_LIBRARY_SERVICE_DEFAULT                                                                              \
    { sizeof(mc_plugin_library_service), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, \
        NULL, NULL, NULL, NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
