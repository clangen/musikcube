#ifndef MUSIKCORE_LIBRARY_H
#define MUSIKCORE_LIBRARY_H

#include <musikcore/context.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_track_page mc_track_page;
typedef struct mc_track_result mc_track_result;
typedef struct mc_track_ref mc_track_ref;

typedef enum mc_track_search_mode {
    MC_TRACK_SEARCH_SUBSTRING = 0,
    MC_TRACK_SEARCH_REGEX = 1
} mc_track_search_mode;

typedef enum mc_track_search_order {
    MC_TRACK_SEARCH_ALBUM = 0,
    MC_TRACK_SEARCH_TITLE = 1,
    MC_TRACK_SEARCH_ARTIST = 2,
    MC_TRACK_SEARCH_DATE_ADDED_ASC = 3,
    MC_TRACK_SEARCH_DATE_ADDED_DESC = 4,
    MC_TRACK_SEARCH_DATE_UPDATED_ASC = 5,
    MC_TRACK_SEARCH_DATE_UPDATED_DESC = 6,
    MC_TRACK_SEARCH_LAST_PLAYED_ASC = 7,
    MC_TRACK_SEARCH_LAST_PLAYED_DESC = 8,
    MC_TRACK_SEARCH_RATING_ASC = 9,
    MC_TRACK_SEARCH_RATING_DESC = 10,
    MC_TRACK_SEARCH_PLAY_COUNT_ASC = 11,
    MC_TRACK_SEARCH_PLAY_COUNT_DESC = 12,
    MC_TRACK_SEARCH_GENRE = 13
} mc_track_search_order;

typedef struct mc_track_search {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* UTF-8 title, album, artist, or genre search. Copied during submission. */
    mc_string_view text;
    /* A value from mc_track_search_mode. Empty text matches every track in the sort. */
    uint32_t mode;
    /* A value from mc_track_search_order. Zero sorts by album. */
    uint32_t order;
} mc_track_search;

#define MC_TRACK_SEARCH_DEFAULT { sizeof(mc_track_search), { NULL, 0 }, MC_TRACK_SEARCH_SUBSTRING, MC_TRACK_SEARCH_ALBUM }

typedef struct mc_track_range {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* First track position in the stable result. */
    size_t offset;
    /* Maximum tracks to read. Zero uses the context default. */
    size_t limit;
    /* Requested keys in field order. Copied during submission. */
    const mc_string_view *fields;
    /* Number of keys. Zero requests the title. */
    size_t field_count;
} mc_track_range;

#define MC_TRACK_RANGE_DEFAULT { sizeof(mc_track_range), 0, 0, NULL, 0 }

/* Success transfers the result handle. Errors are borrowed for the callback. */
typedef void (MC_CALL *mc_track_search_completion_fn)(
    mc_result result, mc_track_result *tracks, const mc_error *error, void *user_data);

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

/* Open a local search. Membership, sort order and album boundaries stay fixed. */
MC_API mc_result MC_CALL mc_library_open_track_search_async(
    mc_context *context, const mc_track_search *search, mc_track_search_completion_fn completion,
    void *user_data, mc_request_id *out_request);
MC_API size_t MC_CALL mc_track_result_get_count(const mc_track_result *tracks);
/* Zero for an invalid row. */
MC_API int64_t MC_CALL mc_track_result_get_id(const mc_track_result *tracks, size_t row);
/* True when a grouped sort starts a new album at this track position. */
MC_API bool MC_CALL mc_track_result_starts_album(const mc_track_result *tracks, size_t row);
/* Read current metadata. Removed tracks retain their IDs and have no values. */
/* The submitted request retains the result data after the caller releases the handle. */
MC_API mc_result MC_CALL mc_track_result_read_async(
    mc_context *context, const mc_track_result *tracks, const mc_track_range *range,
    mc_track_query_completion_fn completion, void *user_data, mc_request_id *out_request);
MC_API void MC_CALL mc_track_result_destroy(mc_track_result **tracks);

/* Create an owned identity for a result row. The output slot must be empty. */
MC_API mc_result MC_CALL mc_track_result_get_ref(
    const mc_track_result *tracks, size_t row, mc_track_ref **out_track);
/* The ID is scoped to the originating local library. Null returns zero. */
MC_API int64_t MC_CALL mc_track_ref_get_id(const mc_track_ref *track);
MC_API void MC_CALL mc_track_ref_destroy(mc_track_ref **track);
/* Resolve against the originating library. Hidden or removed tracks return MC_ERROR_NOT_FOUND. */
/* The accepted request copies the identity and all requested fields. */
MC_API mc_result MC_CALL mc_track_ref_read_async(mc_context *context, const mc_track_ref *track,
    const mc_string_view *fields, size_t field_count, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request);

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
