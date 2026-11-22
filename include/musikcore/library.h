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
typedef struct mc_remote_session mc_remote_session;

enum {
    MC_REMOTE_DEFAULT_WEBSOCKET_PORT = 7905,
    MC_REMOTE_DEFAULT_HTTP_PORT = 7906
};

typedef void (MC_CALL *mc_library_completion_fn)(mc_result status, const mc_error* error, void* user_data);
enum { MC_TRACK_RATING_MAX = 5 };

/* Required handles/options/callbacks/output slots are non-null; user_data may be null.
   Library requests that take a context accept a null library to use its selection.
   They capture that identity at submission and return MC_ERROR_NOT_FOUND if empty.
   A pointer with a count may be null only when count is zero.
   New handle output slots must be initialized to null. Occupied slots are rejected
   without modification. Destroy accepts null slots and clears owned handles.
   Track IDs are positive and library-scoped; zero is the invalid/missing track ID.
   Text inputs must describe readable UTF-8 without embedded NUL. Byte lengths
   above PTRDIFF_MAX are rejected before reading storage. No UTF-8 normalization
   is performed. The caller is responsible for encoding validity.
   Accessors on null handles return zero, false, or a null view as appropriate.
   Views do not own storage and are valid until their parent is destroyed.
   Copy their bytes if they must outlive that parent.
   Async callbacks use status, payload, callback-scoped error, user_data order.
   Error details may be null on allocation failure; status is authoritative.
   Error is null on success and otherwise valid only during the callback. */

typedef struct mc_library_options {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* A value from mc_library_kind. */
    uint32_t kind;
    /* UTF-8 local database directory or remote endpoint. Copied at creation. */
    mc_string_view locator;
} mc_library_options;

#define MC_LIBRARY_OPTIONS_DEFAULT { sizeof(mc_library_options), MC_LIBRARY_LOCAL, { NULL, 0 } }

/* Create a library identity. The output slot must be empty.
   Creating a remote handle by endpoint does not open a connection. Remote reads require a core-bound
   session. */
MC_API mc_result MC_CALL mc_library_create(const mc_library_options *options, mc_library **out_library);
/* Release the handle. Accepted requests and derived identities retain their library. */
MC_API void MC_CALL mc_library_destroy(mc_library **library);

/* Set a track's rating from zero through five in its originating library.
   Zero clears the rating. Completion runs on the context dispatcher. */
MC_API mc_result MC_CALL mc_track_set_rating_async(mc_context* context, const mc_track_ref* track, uint32_t rating,
    mc_library_completion_fn completion, void* user_data, mc_request_id* out_request);

typedef uint32_t mc_remote_state;
enum {
    MC_REMOTE_DISCONNECTED = 0,
    MC_REMOTE_CONNECTING = 1,
    MC_REMOTE_CONNECTED = 2,
    MC_REMOTE_AUTHENTICATION_FAILED = 3
};

typedef struct mc_remote_options {
    /* Size of this struct in bytes. */
    uint32_t struct_size;
    /* UTF-8 hostname or address. Copied. */
    mc_string_view host;
    /* UTF-8 server password. Copied. Empty means no password. */
    mc_string_view password;
    /* WebSocket port. Zero selects the default. */
    uint16_t websocket_port;
    /* Audio HTTP port. Zero selects the default. */
    uint16_t http_port;
    /* Enable TLS for the WebSocket connection. Defaults to false. */
    bool websocket_tls;
    /* Enable TLS for audio requests. Defaults to false. */
    bool http_tls;
    /* Allow HTTPS audio and artwork from an untrusted or expired certificate.
       Hostname verification remains enabled. Defaults to zero. */
    uint32_t http_tls_allow_untrusted_certificate;
} mc_remote_options;

#define MC_REMOTE_OPTIONS_DEFAULT { sizeof(mc_remote_options), { NULL, 0 }, { NULL, 0 }, 0, 0, false, false, 0 }

typedef void (MC_CALL *mc_remote_connect_fn)(mc_result status, mc_remote_state state,
    const mc_error *error, void *user_data);

/* Own a remote connection independently of app preferences. Use session operations on one
   host thread. The ABI-only build returns MC_ERROR_UNSUPPORTED. */
MC_API mc_result MC_CALL mc_remote_session_create(const mc_remote_options *options, mc_remote_session **out_session);
/* Disconnect and release the session. Existing library handles retain their original generation. */
MC_API void MC_CALL mc_remote_session_destroy(mc_remote_session **session);
/* Report the latest connection state. */
MC_API mc_remote_state MC_CALL mc_remote_session_state(const mc_remote_session *session);
/* Complete on context dispatch when the connection succeeds or fails. Cancelling
   delivery does not disconnect the session. Only one connection attempt may be active. */
MC_API mc_result MC_CALL mc_remote_session_connect_async(mc_context *context, mc_remote_session *session,
    mc_remote_connect_fn completion, void *user_data, mc_request_id *out_request);
/* Disconnect and invalidate previously opened library handles for future requests. */
MC_API mc_result MC_CALL mc_remote_session_disconnect(mc_remote_session *session);
/* Open the currently connected library. Use the same library APIs as for a local catalog. */
MC_API mc_result MC_CALL mc_remote_session_open_library(const mc_remote_session *session, mc_library **out_library);

typedef uint32_t mc_track_search_mode;
enum {
    MC_TRACK_SEARCH_SUBSTRING = 0,
    MC_TRACK_SEARCH_REGEX = 1
};

typedef uint32_t mc_track_search_order;
enum {
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
};

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

/* Success transfers the result handle. Errors are valid only during the callback. */
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
#define MC_TRACK_FIELD_LYRICS "lyrics"
#define MC_TRACK_FIELD_LAST_PLAYED "last_played"
#define MC_TRACK_FIELD_PLAY_COUNT "play_count"
#define MC_TRACK_FIELD_DATE_ADDED "date_added"
#define MC_TRACK_FIELD_DATE_UPDATED "date_updated"
#define MC_TRACK_FIELD_ALBUM "album"
#define MC_TRACK_FIELD_ARTIST "artist"
#define MC_TRACK_FIELD_ALBUM_ARTIST "album_artist"
#define MC_TRACK_FIELD_GENRE "genre"
#define MC_TRACK_FIELD_DIRECTORY "directory"

/* Category key for browsing saved playlists. */
#define MC_LIBRARY_CATEGORY_PLAYLISTS "playlists"

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

/* Success transfers the page to the callback. Release it with mc_track_page_destroy.
   Core owns error details and message bytes; they are valid until the callback returns. */
typedef void (MC_CALL *mc_track_query_completion_fn)(
    mc_result result, mc_track_page *page, const mc_error *error, void *user_data);

/* Open a search. Membership, sort order and album boundaries stay fixed. */
MC_API mc_result MC_CALL mc_library_open_track_search_async(
    mc_context *context, const mc_library *library, const mc_track_search *search, mc_track_search_completion_fn completion,
    void *user_data, mc_request_id *out_request);

/* Read the saved playlist in queue order, preserving duplicates. Uses the track-result completion contract.
   Returns MC_ERROR_UNSUPPORTED when the library has no playlist backend. */
MC_API mc_result MC_CALL mc_library_open_playlist_async(
    mc_context *context, const mc_library *library, int64_t playlist_id,
    mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request);

/* Open a Browse category, including categories defined by metadata keys.
   id is a signed backend catalog ID; zero can identify an uncategorized group. */
MC_API mc_result MC_CALL mc_library_open_category_async(
    mc_context *context, const mc_library *library, mc_string_view category, int64_t id,
    mc_track_search_order order, mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request);
/* Open the ordered tracks below a directory using the library's directory query. */
MC_API mc_result MC_CALL mc_library_open_directory_async(
    mc_context *context, const mc_library *library, mc_string_view directory,
    mc_track_search_completion_fn completion, void *user_data, mc_request_id *out_request);
/* Copy library-scoped IDs into a result. The caller owns the returned result. */
MC_API mc_result MC_CALL mc_library_make_track_result(
    const mc_library *library, const int64_t *ids, size_t count, mc_track_result **out_result);

typedef struct mc_playlist_entry {
    /* ID scoped to the library used for the request. */
    int64_t id;
    /* UTF-8 name owned by core, valid only during the completion callback. */
    mc_string_view name;
} mc_playlist_entry;

typedef uint32_t mc_playlist_event;
enum {
    MC_PLAYLIST_CONTENTS_CHANGED,
    MC_PLAYLIST_CREATED,
    MC_PLAYLIST_RENAMED,
    MC_PLAYLIST_DELETED
};

typedef void (MC_CALL *mc_playlist_changed_fn)(void* user_data, mc_playlist_event event, int64_t playlist_id);

/* Observe local playlist changes while the managed local library is selected.
   Register on the dispatcher thread. Dispatch delivers changes in order.
   Pass NULL to unsubscribe and discard queued changes. */
MC_API mc_result MC_CALL mc_context_set_playlist_changed_callback(
    mc_context* context, mc_playlist_changed_fn callback, void* user_data);

typedef struct mc_category_entry {
    /* ID scoped to the library and category field. */
    int64_t id;
    /* UTF-8 category name, valid only during the completion callback. */
    mc_string_view value;
} mc_category_entry;

/* Entries are borrowed for the duration of the callback. */
typedef void (MC_CALL *mc_category_list_completion_fn)(mc_result status, const mc_category_entry* entries,
    size_t count, const mc_error* error, void* user_data);

typedef void (MC_CALL *mc_category_fields_completion_fn)(mc_result status, const mc_string_view* fields,
    size_t count, const mc_error* error, void* user_data);
/* List available metadata fields. Entries live through the completion callback. */
MC_API mc_result MC_CALL mc_library_list_category_fields_async(mc_context* context, const mc_library* library,
    mc_category_fields_completion_fn completion, void* user_data, mc_request_id* out_request);

/* List values for a metadata field, filtered by substring or regular expression. */
typedef uint32_t mc_facet_field;
enum { MC_FACET_GENRE = 1, MC_FACET_ARTIST = 2, MC_FACET_ALBUM = 3 };

typedef struct mc_facet_selection {
    /* Category column to constrain. */
    mc_facet_field field;
    /* Positive category ID from a facet or category result in this library. */
    int64_t id;
} mc_facet_selection;

/* List values that occur in matching tracks. Text uses the same title, artist,
   album and genre substring matching as track search. Constraints are intersected.
   Pass the other columns' selections to keep this facet's alternatives visible.
   Each field may be constrained once. Callback rows are borrowed during delivery. */
MC_API mc_result MC_CALL mc_library_list_facets_async(mc_context* context, const mc_library* library,
    mc_facet_field field, mc_string_view search, const mc_facet_selection* selections, size_t selection_count,
    mc_category_list_completion_fn completion, void* user_data, mc_request_id* out_request);

MC_API mc_result MC_CALL mc_library_list_categories_async(mc_context* context, const mc_library* library,
    mc_string_view field, mc_string_view filter, mc_track_search_mode mode,
    mc_category_list_completion_fn completion, void* user_data, mc_request_id* out_request);

typedef struct mc_album_entry {
    /* ID scoped to the catalog. */
    int64_t id;
    /* Album name borrowed until the callback returns. */
    mc_string_view title;
    /* Album artist borrowed until the callback returns. */
    mc_string_view artist;
    /* Catalog-scoped cover ID. Zero means no artwork. */
    int64_t thumbnail_id;
} mc_album_entry;

typedef void (MC_CALL *mc_album_list_completion_fn)(mc_result status, const mc_album_entry* albums,
    size_t count, const mc_error* error, void* user_data);
typedef void (MC_CALL *mc_thumbnail_completion_fn)(mc_result status, mc_byte_view image,
    const mc_error* error, void* user_data);

/* List albums and their cover IDs. The catalog and substring filter are captured at submission. */
MC_API mc_result MC_CALL mc_library_list_albums_async(mc_context* context, const mc_library* library,
    mc_string_view filter, mc_album_list_completion_fn completion, void* user_data, mc_request_id* out_request);
/* Read encoded artwork by positive cover ID from the supplied catalog. Bytes
   are valid only during completion. Missing artwork returns MC_ERROR_NOT_FOUND. */
MC_API mc_result MC_CALL mc_library_read_thumbnail_async(mc_context* context, const mc_library* library,
    int64_t thumbnail_id, mc_thumbnail_completion_fn completion, void* user_data, mc_request_id* out_request);

/* Core owns entries, names, and error; valid only during the callback. Failure yields null
   entries and zero count. */
typedef void (MC_CALL *mc_playlist_list_completion_fn)(
    mc_result result, const mc_playlist_entry *entries, size_t count, const mc_error *error, void *user_data);
/* On success, playlist_id identifies the playlist. Older servers may return zero for creation.
   Failure yields zero. Error details are valid only during the callback. */
typedef void (MC_CALL *mc_playlist_write_completion_fn)(
    mc_result result, int64_t playlist_id, const mc_error *error, void *user_data);

MC_API mc_result MC_CALL mc_library_list_playlists_async(
    mc_context *context, const mc_library *library, mc_playlist_list_completion_fn completion,
    void *user_data, mc_request_id *out_request);
/* Playlist mutations require an explicit library handle. If a remote write
   returns MC_ERROR_OUTCOME_UNKNOWN, reread the playlist before retrying. */
/* Zero playlist_id creates a playlist using name. A positive ID overwrites its tracks.
   IDs belong to library. Core copies their order, including duplicates, before returning. */
MC_API mc_result MC_CALL mc_library_save_playlist_async(
    mc_context *context, const mc_library *library, int64_t playlist_id, mc_string_view name,
    const int64_t *track_ids, size_t track_count, mc_playlist_write_completion_fn completion,
    void *user_data, mc_request_id *out_request);
/* Rename or delete a playlist in the supplied library. */
MC_API mc_result MC_CALL mc_library_rename_playlist_async(
    mc_context *context, const mc_library *library, int64_t playlist_id, mc_string_view name,
    mc_playlist_write_completion_fn completion, void *user_data, mc_request_id *out_request);
MC_API mc_result MC_CALL mc_library_delete_playlist_async(
    mc_context *context, const mc_library *library, int64_t playlist_id,
    mc_playlist_write_completion_fn completion, void *user_data, mc_request_id *out_request);
/* Append track IDs in order to a saved playlist. IDs must belong to the supplied library. */
MC_API mc_result MC_CALL mc_library_append_playlist_async(
    mc_context *context, const mc_library *library, int64_t playlist_id,
    const int64_t *track_ids, size_t track_count, mc_playlist_write_completion_fn completion,
    void *user_data, mc_request_id *out_request);
MC_API size_t MC_CALL mc_track_result_get_count(const mc_track_result *tracks);
/* Zero for an invalid row. */
MC_API int64_t MC_CALL mc_track_result_get_id(const mc_track_result *tracks, size_t row);
/* True when a grouped sort starts a new album at this track position. */
MC_API bool MC_CALL mc_track_result_starts_album(const mc_track_result *tracks, size_t row);
/* Total seconds for the album group starting at row. Other positions return zero. */
MC_API uint64_t MC_CALL mc_track_result_get_album_duration(const mc_track_result *tracks, size_t row);
/* Read current metadata. Removed tracks retain their IDs and have no values.
   Remote reads support ID, title, artist, album, duration, track number, rating, and lyrics.
   A remote read that requests any other field returns MC_ERROR_UNSUPPORTED.
   Current servers preserve missing/empty values and hidden rows through a typed
   projection. Legacy servers can return their older, less precise metadata map.
   The submitted request retains the result data after the caller releases the handle. */
MC_API mc_result MC_CALL mc_track_result_read_async(
    mc_context *context, const mc_track_result *tracks, const mc_track_range *range,
    mc_track_query_completion_fn completion, void *user_data, mc_request_id *out_request);
MC_API void MC_CALL mc_track_result_destroy(mc_track_result **tracks);

/* Create an owned identity for a result row. The output slot must be empty. */
MC_API mc_result MC_CALL mc_track_result_get_ref(
    const mc_track_result *tracks, size_t row, mc_track_ref **out_track);
/* The ID is scoped to the originating library. Null returns zero. */
MC_API int64_t MC_CALL mc_track_ref_get_id(const mc_track_ref *track);
/* Return an owned handle to the track's originating library in an empty output slot. */
MC_API mc_result MC_CALL mc_track_ref_get_library(const mc_track_ref* track, mc_library** out_library);
MC_API void MC_CALL mc_track_ref_destroy(mc_track_ref **track);
/* Resolve against the originating library. Missing tracks return MC_ERROR_NOT_FOUND.
   Local reads also treat hidden tracks as missing. Remote reads use the range projection above.
   The accepted request copies the identity and all requested fields. */
MC_API mc_result MC_CALL mc_track_ref_read_async(mc_context *context, const mc_track_ref *track,
    const mc_string_view *fields, size_t field_count, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request);

/* Callers own query input storage and may release it on return.
   Local libraries read musik.db in their locator directory. */
MC_API mc_result MC_CALL mc_library_query_tracks_async(
    mc_context *context, const mc_library *library, const mc_track_query *query, mc_track_query_completion_fn completion,
    void *user_data, mc_request_id *out_request);
MC_API size_t MC_CALL mc_track_page_get_count(const mc_track_page *page);
/* Returns zero for a missing row. */
MC_API int64_t MC_CALL mc_track_page_get_id(const mc_track_page *page, size_t row);
/* Returns a view into page-owned storage, valid until page destruction. Null data means missing. */
MC_API mc_string_view MC_CALL mc_track_page_get_string(const mc_track_page *page, size_t row, size_t field);
/* Counts fields in the original request order. Null pages return zero. */
MC_API size_t MC_CALL mc_track_page_get_field_count(const mc_track_page *page);
/* Returns a page-owned field name, valid until page destruction. Invalid indices return a null view. */
MC_API mc_string_view MC_CALL mc_track_page_get_field_name(const mc_track_page *page, size_t field);
/* Returns zero for missing values and invalid indices. */
MC_API size_t MC_CALL mc_track_page_get_value_count(const mc_track_page *page, size_t row, size_t field);
/* Returns a page-owned value, valid until page destruction. Invalid indices return a null view. */
MC_API mc_string_view MC_CALL mc_track_page_get_string_at(
    const mc_track_page *page, size_t row, size_t field, size_t value);
/* Consumes the page and clears the slot. Null is accepted. */
MC_API void MC_CALL mc_track_page_destroy(mc_track_page **page);

#ifdef __cplusplus
}
#endif

#endif
