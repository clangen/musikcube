#ifndef MUSIKCORE_MUSIKCORE_H
#define MUSIKCORE_MUSIKCORE_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_context mc_context;
typedef struct mc_track_page mc_track_page;
typedef uint64_t mc_request_id;
typedef void (MC_CALL *mc_wakeup_fn)(void *user_data);

typedef struct mc_context_options {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Requested ABI version. Zero selects the current version. */
    uint32_t requested_abi_version;
    /* UTF-8 data root, copied at creation. An empty view selects the default root. */
    mc_string_view data_directory;
    /* UTF-8 plugin root, copied at creation. An empty view selects the default root. */
    mc_string_view plugin_directory;
    /* Reserved for context options. Must be zero. */
    uint32_t flags;
    /* Optional hook that signals the host loop. May run on a worker thread. */
    mc_wakeup_fn wakeup;
    /* Borrowed hook argument. Keep it valid until shutdown returns. */
    void *wakeup_user_data;
    /* Default query row limit, capped by the maximum. Nonpositive uses MC_TRACK_QUERY_DEFAULT_LIMIT. */
    int32_t track_query_default_limit;
    /* Maximum query row limit. Nonpositive uses MC_TRACK_QUERY_MAX_LIMIT. */
    int32_t track_query_max_limit;
    /* Maximum metadata field count. Nonpositive uses MC_TRACK_QUERY_MAX_FIELDS. */
    int32_t track_query_max_fields;
} mc_context_options;

#define MC_CONTEXT_OPTIONS_DEFAULT \
    { sizeof(mc_context_options), 0, { NULL, 0 }, { NULL, 0 }, 0, NULL, NULL, 0, 0, 0 }

/* Null options use defaults. Callers own directory storage and may release it on return. */
/* Output slots must not already own handles. A second context returns MC_ERROR_BUSY. */
MC_API mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error);

/* The first dispatch binds its thread. Callbacks run inline for work already ready. */
MC_API mc_result MC_CALL mc_context_dispatch(mc_context *context);
/* Selects cancellation if the request still has no terminal result. */
MC_API mc_result MC_CALL mc_request_cancel(mc_context *context, mc_request_id request);
/* Settles requests on the dispatcher thread. Waits for work and wakeup hooks to stop. */
MC_API mc_result MC_CALL mc_context_shutdown(mc_context *context);

/* Consumes the context and clears the slot. Null is accepted. */
MC_API void MC_CALL mc_context_destroy(mc_context **context);

/* Built-in metadata keys. Queries also accept keys from the library tag tables. */
#define MC_TRACK_FIELD_ID "id"
#define MC_TRACK_FIELD_TITLE "title"
#define MC_TRACK_FIELD_TRACK "track"
#define MC_TRACK_FIELD_DISC "disc"
#define MC_TRACK_FIELD_BPM "bpm"
#define MC_TRACK_FIELD_DURATION "duration"
#define MC_TRACK_FIELD_FILESIZE "filesize"
#define MC_TRACK_FIELD_FILENAME "filename"
#define MC_TRACK_FIELD_FILETIME "filetime"
#define MC_TRACK_FIELD_THUMBNAIL_ID "thumbnail_id"
#define MC_TRACK_FIELD_VISUAL_GENRE_ID "visual_genre_id"
#define MC_TRACK_FIELD_VISUAL_ARTIST_ID "visual_artist_id"
#define MC_TRACK_FIELD_ALBUM_ARTIST_ID "album_artist_id"
#define MC_TRACK_FIELD_ALBUM_ID "album_id"
#define MC_TRACK_FIELD_PATH_ID "path_id"
#define MC_TRACK_FIELD_SOURCE_ID "source_id"
#define MC_TRACK_FIELD_EXTERNAL_ID "external_id"
#define MC_TRACK_FIELD_RATING "rating"
#define MC_TRACK_FIELD_LAST_PLAYED "last_played"
#define MC_TRACK_FIELD_PLAY_COUNT "play_count"
#define MC_TRACK_FIELD_DATE_ADDED "date_added"
#define MC_TRACK_FIELD_DATE_UPDATED "date_updated"
#define MC_TRACK_FIELD_ALBUM "album"
#define MC_TRACK_FIELD_ARTIST "artist"
#define MC_TRACK_FIELD_ALBUM_ARTIST "album_artist"
#define MC_TRACK_FIELD_GENRE "genre"
#define MC_TRACK_FIELD_DIRECTORY "directory"

enum {
    MC_TRACK_QUERY_DEFAULT_LIMIT = 100,
    MC_TRACK_QUERY_MAX_LIMIT = 1000,
    MC_TRACK_QUERY_MAX_FIELDS = 64
};

typedef struct mc_track_query {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* Literal title substring. An empty view matches all visible tracks. */
    mc_string_view search;
    /* Requested UTF-8 metadata keys in result order. Copied during submission. */
    const mc_string_view *fields;
    /* Number of keys, up to the context's field limit. Zero selects title. */
    size_t field_count;
    /* Number of matching rows to skip. */
    size_t offset;
    /* Maximum rows, up to the context's row limit. Zero selects its default. */
    size_t limit;
    /* Reserved. Must be zero. */
    uint32_t flags;
} mc_track_query;

#define MC_TRACK_QUERY_DEFAULT { sizeof(mc_track_query), { NULL, 0 }, NULL, 0, 0, 0, 0 }

/* Success transfers the page to the callback. Release it with mc_track_page_destroy. */
/* Error details and their message views are borrowed until the callback returns. */
typedef void (MC_CALL *mc_track_query_completion_fn)(
    mc_result result, mc_track_page *page, const mc_error *error, void *user_data);

/* Callers own query input storage and may release it on return. */
/* ABI0 reads musik.db in the explicit context data directory. */
MC_API mc_result MC_CALL mc_library_query_tracks_async(
    mc_context *context, const mc_track_query *query, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request);
MC_API size_t MC_CALL mc_track_page_get_count(const mc_track_page *page);
/* Returns zero for a missing row. */
MC_API int64_t MC_CALL mc_track_page_get_id(const mc_track_page *page, size_t row);
/* Borrows the first value until page destruction. Null data means missing. */
MC_API mc_string_view MC_CALL mc_track_page_get_string(const mc_track_page *page, size_t row, size_t field);
/* Counts fields in the original request order. Null pages return zero. */
MC_API size_t MC_CALL mc_track_page_get_field_count(const mc_track_page *page);
/* Borrows a field name until page destruction. An invalid index returns a null view. */
MC_API mc_string_view MC_CALL mc_track_page_get_field_name(const mc_track_page *page, size_t field);
/* Returns zero for missing values and invalid indices. */
MC_API size_t MC_CALL mc_track_page_get_value_count(const mc_track_page *page, size_t row, size_t field);
/* Borrows a value until page destruction. An invalid index returns a null view. */
MC_API mc_string_view MC_CALL mc_track_page_get_string_at(
    const mc_track_page *page, size_t row, size_t field, size_t value);
/* Consumes the page and clears the slot. Null is accepted. */
MC_API void MC_CALL mc_track_page_destroy(mc_track_page **page);

#ifdef __cplusplus
}
#endif

#endif
