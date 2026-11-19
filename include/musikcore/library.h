#ifndef MUSIKCORE_LIBRARY_H
#define MUSIKCORE_LIBRARY_H

#include <musikcore/context.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_track_page mc_track_page;

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
