#ifndef MUSIKCORE_LIBRARY_HPP
#define MUSIKCORE_LIBRARY_HPP

#include <musikcore/context.hpp>
#include <musikcore/library.h>

#include <charconv>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace musik { namespace core { namespace api {

    /* Reads one row's metadata by field name. Use when displaying a track or
       keeping a row after its page leaves a cache. Copyable and movable;
       retains the page without copying strings. Returned string views do not
       retain storage, so keep a page or TrackView alive while using them. */
    class TrackView {
    public:
        TrackView(const TrackView&) = default;
        TrackView& operator=(const TrackView&) = default;
        TrackView(TrackView&&) noexcept = default;
        TrackView& operator=(TrackView&&) noexcept = default;
        int64_t Id() const noexcept { return mc_track_page_get_id(page.get(), rowIndex); }
        explicit operator bool() const noexcept { return Id() != 0; }

        /* Missing fields have null data. An explicit empty value has non-null data. */
        std::string_view Title() const noexcept { return Value(MC_TRACK_FIELD_TITLE); }
        std::string_view Artist() const noexcept { return Value(MC_TRACK_FIELD_ARTIST); }
        std::string_view Album() const noexcept { return Value(MC_TRACK_FIELD_ALBUM); }
        std::string_view AlbumArtist() const noexcept { return Value(MC_TRACK_FIELD_ALBUM_ARTIST); }
        std::string_view Genre() const noexcept { return Value(MC_TRACK_FIELD_GENRE); }
        std::string_view Filename() const noexcept { return Value(MC_TRACK_FIELD_FILENAME); }
        std::optional<int64_t> TrackNumber() const noexcept { return Number(MC_TRACK_FIELD_TRACK); }
        std::optional<int64_t> DiscNumber() const noexcept { return Number(MC_TRACK_FIELD_DISC); }
        std::optional<std::chrono::seconds> Duration() const noexcept {
            const auto seconds = Number(MC_TRACK_FIELD_DURATION);
            if (!seconds || *seconds < 0) return std::nullopt;
            return std::chrono::seconds(*seconds);
        }

        std::string_view Value(std::string_view key, size_t index = 0) const noexcept {
            return AsStringView(mc_track_page_get_string_at(page.get(), rowIndex, FieldIndex(key), index));
        }

        /* The vector owns its elements, not the string bytes. Keep the page or TrackView alive. */
        std::vector<std::string_view> Values(std::string_view key) const {
            const auto field = FieldIndex(key);
            const auto count = mc_track_page_get_value_count(page.get(), rowIndex, field);
            std::vector<std::string_view> result;
            result.reserve(count);
            for (size_t index = 0; index < count; ++index) {
                result.push_back(AsStringView(mc_track_page_get_string_at(page.get(), rowIndex, field, index)));
            }
            return result;
        }

    private:
        friend class TrackPage;
        TrackView(std::shared_ptr<mc_track_page> page, size_t row) noexcept
            : page(std::move(page)), rowIndex(row) {}

        size_t FieldIndex(std::string_view key) const noexcept {
            const auto count = mc_track_page_get_field_count(page.get());
            for (size_t index = 0; index < count; ++index) {
                if (AsStringView(mc_track_page_get_field_name(page.get(), index)) == key) return index;
            }
            return count;
        }

        std::optional<int64_t> Number(std::string_view key) const noexcept {
            const auto text = Value(key);
            if (text.empty()) return std::nullopt;
            int64_t result = 0;
            const auto end = text.data() + text.size();
            const auto parsed = std::from_chars(text.data(), end, result);
            if (parsed.ec != std::errc{} || parsed.ptr != end) return std::nullopt;
            return result;
        }

        std::shared_ptr<mc_track_page> page;
        size_t rowIndex;
    };

    /* Owns a batch of metadata returned by QueryTracks, ReadTracks, or ReadTrack.
       Use row/field accessors for indexed reads, or Track() for a named-field
       TrackView. Move-only; moving leaves the source empty. TrackViews share
       the page's storage and can keep it alive after this wrapper is released. */
    class TrackPage {
    public:
        TrackPage() = default;
        /* Takes ownership of a page returned by the C ABI. */
        explicit TrackPage(mc_track_page *page) {
            if (page) this->page = std::shared_ptr<mc_track_page>(page, Destroy{});
        }

        TrackPage(const TrackPage &) = delete;
        TrackPage &operator=(const TrackPage &) = delete;
        TrackPage(TrackPage &&) noexcept = default;
        TrackPage &operator=(TrackPage &&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(page); }
        size_t Size() const noexcept { return mc_track_page_get_count(page.get()); }
        int64_t Id(size_t row) const noexcept { return mc_track_page_get_id(page.get(), row); }
        size_t FieldCount() const noexcept { return mc_track_page_get_field_count(page.get()); }
        TrackView Track(size_t row) const & noexcept { return TrackView(page, row); }
        TrackView Track(size_t row) const && = delete;
        size_t ValueCount(size_t row, size_t field) const noexcept {
            return mc_track_page_get_value_count(page.get(), row, field);
        }

        /* Views do not retain storage; keep a page or TrackView owner alive. Missing values have null data. */
        std::string_view FieldName(size_t field) const & noexcept {
            return AsStringView(mc_track_page_get_field_name(page.get(), field));
        }
        std::string_view FieldName(size_t field) const && = delete;
        std::string_view Value(size_t row, size_t field, size_t index = 0) const & noexcept {
            return AsStringView(mc_track_page_get_string_at(page.get(), row, field, index));
        }
        std::string_view Value(size_t row, size_t field, size_t index = 0) const && = delete;

    private:
        /* Releases C page storage when the last shared page/view owner goes
           away. Stateless, copyable, and movable. */
        struct Destroy {
            void operator()(mc_track_page *page) const noexcept { mc_track_page_destroy(&page); }
        };
        std::shared_ptr<mc_track_page> page;
    };

    /* Describes one page of a literal title-substring query. Use with
       QueryTracks when the caller needs metadata immediately, rather than a
       stable full result for later paging. Copyable and movable; caller-owned
       search/field strings must remain alive through submission. */
    struct TrackQuery {
        /* Title substring and field names are copied during submission. */
        std::string_view search;
        std::vector<std::string_view> fields;
        size_t offset = 0;
        size_t limit = 0;
    };

    /* The callback owns its page; the error message is valid only until it returns. */
    using TrackQueryCompletion = std::function<void(mc_result, TrackPage, std::string_view)>;

    /* Identifies one track in its original library. Keep a TrackRef for later
       metadata reads or playback actions after releasing a result/page; it does
       not cache metadata. Move-only, retaining its library identity separately
       from metadata storage. */
    class TrackRef {
    public:
        TrackRef() = default;
        explicit TrackRef(mc_track_ref *track) noexcept : identity(track) {}
        TrackRef(const TrackRef &) = delete;
        TrackRef &operator=(const TrackRef &) = delete;
        TrackRef(TrackRef &&) noexcept = default;
        TrackRef &operator=(TrackRef &&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(identity); }
        int64_t Id() const noexcept { return mc_track_ref_get_id(Get()); }
        const mc_track_ref *Get() const noexcept { return identity.get(); }

    private:
        /* Releases the C identity when its unique wrapper owner is released.
           Stateless, copyable, and movable. */
        struct Destroy {
            void operator()(mc_track_ref *track) const noexcept { mc_track_ref_destroy(&track); }
        };
        std::unique_ptr<mc_track_ref, Destroy> identity;
    };

    /* Owns a stable ordered track selection and album grouping. Use search,
       browse, or playlist results to page metadata with ReadTracks or build a
       playback queue. Membership is fixed; later reads fetch current metadata.
       Move-only; moving transfers the handle and leaves the source empty. */
    class TrackResult {
    public:
        TrackResult() = default;
        explicit TrackResult(mc_track_result *tracks) noexcept : tracks(tracks) {}

        TrackResult(const TrackResult &) = delete;
        TrackResult &operator=(const TrackResult &) = delete;
        TrackResult(TrackResult &&) noexcept = default;
        TrackResult &operator=(TrackResult &&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(tracks); }
        const mc_track_result *Get() const noexcept { return tracks.get(); }
        size_t Size() const noexcept { return mc_track_result_get_count(Get()); }
        int64_t Id(size_t row) const noexcept { return mc_track_result_get_id(Get(), row); }
        bool StartsAlbum(size_t row) const noexcept { return mc_track_result_starts_album(Get(), row); }
        uint64_t AlbumDurationSeconds(size_t row) const noexcept {
            return mc_track_result_get_album_duration(Get(), row);
        }
        TrackRef Track(size_t row) const {
            mc_track_ref *track = nullptr;
            const auto status = mc_track_result_get_ref(Get(), row, &track);
            if (status != MC_OK) throw Error(status);
            return TrackRef(track);
        }

    private:
        /* Releases the C result handle, leaving separately retained references
           and accepted reads intact. Stateless, copyable, and movable. */
        struct Destroy {
            void operator()(mc_track_result *tracks) const noexcept { mc_track_result_destroy(&tracks); }
        };
        std::unique_ptr<mc_track_result, Destroy> tracks;
    };

    /* Selects text matching and sort order for OpenTrackSearch. Use it to open
       a stable result across titles, albums, artists, or genres before paging
       metadata. Copyable and movable; caller-owned text must remain alive
       through submission. */
    struct TrackSearch {
        std::string_view text;
        mc_track_search_mode mode = MC_TRACK_SEARCH_SUBSTRING;
        mc_track_search_order order = MC_TRACK_SEARCH_ALBUM;
    };

    /* Chooses result positions and metadata fields for ReadTracks. Use a range
       to load only the rows a list needs; zero limit uses the context default.
       Copyable and movable; caller-owned field strings must remain alive
       through submission. */
    struct TrackRange {
        size_t offset = 0;
        size_t limit = 0;
        std::vector<std::string_view> fields = {
            MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_DURATION
        };
    };

    /* Receives ownership of the stable result. The diagnostic string is valid
       only during this callback, which runs when the Context dispatches. */
    using TrackSearchCompletion = std::function<void(mc_result, TrackResult, std::string_view)>;

    namespace detail {
        /* Shared C-to-C++ callback adapter. Adopt the transferred handle before
           invoking user code so normal return and exceptions release it safely. */
        template<typename Handle, typename Result, typename Completion>
        inline void MC_CALL completeLibraryRequest(
            mc_result result, Handle *handle, const mc_error *error, void *data) {
            std::unique_ptr<Completion> completion(static_cast<Completion *>(data));
            Result owned;
            try {
                /* Adopting page storage may allocate its shared lifetime control block. */
                if (handle) owned = Result(handle);
            }
            catch (...) {
                (*completion)(MC_ERROR_INTERNAL, Result{}, "Could not retain result storage");
                return;
            }
            (*completion)(result, std::move(owned), AsStringView(mc_error_get_message(error)));
        }

        /* Shared submission adapter. Keep callback state owned locally until
           C admission succeeds, then release it to the terminal callback. */
        template<typename Completion, typename Start>
        mc_request_id submitLibraryRequest(Completion completion, Start start) {
            if (!completion) throw Error(MC_ERROR_INVALID_ARGUMENT);

            /* An accepted request owns the callback state until its terminal callback. */
            auto state = std::make_unique<Completion>(std::move(completion));
            mc_request_id request = 0;
            const auto result = start(state.get(), &request);
            if (result != MC_OK) throw Error(result);
            state.release();
            return request;
        }

        /* Build the C field array for a submission. It does not copy string
           bytes; the C API copies them before the submitting call returns. */
        inline std::vector<mc_string_view> fieldViews(const std::vector<std::string_view> &names) {
            std::vector<mc_string_view> fields;
            fields.reserve(names.size());
            for (const auto name : names) fields.push_back({ name.data(), name.size() });
            return fields;
        }
    }

    /* Identifies the source of queries, playlists, and track IDs. Create from a
       local database directory or adopt a handle bound by the app. An endpoint
       alone does not connect a remote session. Move-only; accepted requests
       and derived track identities retain the state they need. */
    class Library {
    public:
        explicit Library(std::string_view locator, mc_library_kind kind = MC_LIBRARY_LOCAL) {
            mc_library_options options = MC_LIBRARY_OPTIONS_DEFAULT;
            options.kind = kind;
            options.locator = { locator.data(), locator.size() };
            mc_library *handle = nullptr;
            const auto result = mc_library_create(&options, &handle);
            if (result != MC_OK) throw Error(result);
            library.reset(handle);
        }

        /* Takes ownership of a handle created by the C API. */
        explicit Library(mc_library *handle) noexcept : library(handle) {}

        Library(const Library &) = delete;
        Library &operator=(const Library &) = delete;
        Library(Library &&) noexcept = default;
        Library &operator=(Library &&) noexcept = default;

        /* Wrapper-owned handle. Do not destroy it; valid until this wrapper releases it. */
        const mc_library *Get() const noexcept { return library.get(); }

    private:
        /* Releases the C library handle when this wrapper releases ownership.
           Stateless; copying or moving the deleter has no effect on ownership. */
        struct Deleter {
            void operator()(mc_library *handle) const noexcept { mc_library_destroy(&handle); }
        };
        std::unique_ptr<mc_library, Deleter> library;
    };

    /* Owns one independently configured remote connection. Move-only.
       OpenLibrary returns the same wrapper used for a local catalog. */
    class RemoteSession {
    public:
        using ConnectCompletion = std::function<void(mc_result, mc_remote_state, std::string_view)>;

        explicit RemoteSession(const mc_remote_options& options) {
            mc_remote_session* handle = nullptr;
            const auto result = mc_remote_session_create(&options, &handle);
            if (result != MC_OK) {
                throw Error(result);
            }
            this->session.reset(handle);
        }

        RemoteSession(const RemoteSession&) = delete;
        RemoteSession(RemoteSession&&) noexcept = default;
        RemoteSession& operator=(const RemoteSession&) = delete;
        RemoteSession& operator=(RemoteSession&&) noexcept = default;

        mc_request_id Connect(Context& context, ConnectCompletion completion) {
            return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
                return mc_remote_session_connect_async(context.Get(), this->session.get(),
                    CompleteConnect, state, request);
            });
        }

        mc_remote_state State() const noexcept { return mc_remote_session_state(this->session.get()); }
        mc_result Disconnect() { return mc_remote_session_disconnect(this->session.get()); }

        Library OpenLibrary() const {
            mc_library* library = nullptr;
            const auto result = mc_remote_session_open_library(this->session.get(), &library);
            if (result != MC_OK) {
                throw Error(result);
            }
            return Library(library);
        }

    private:
        /* Stateless. Copying and moving the deleter leave ownership with unique_ptr. */
        struct Deleter {
            void operator()(mc_remote_session* handle) const noexcept { mc_remote_session_destroy(&handle); }
        };

        static void MC_CALL CompleteConnect(mc_result status, mc_remote_state state,
            const mc_error* error, void* data)
        {
            const std::unique_ptr<ConnectCompletion> completion(static_cast<ConnectCompletion*>(data));
            (*completion)(status, state, AsStringView(mc_error_get_message(error)));
        }

        std::unique_ptr<mc_remote_session, Deleter> session;
    };

    /* Async helpers below throw Error if submission is rejected. Accepted
       requests return an ID for Context::Cancel and deliver status/value during
       Context::Dispatch or shutdown. Input strings/arrays are copied during
       submission; callback error strings are valid only until the callback returns. */

    /* Fetch one metadata page for a literal title filter. Use OpenTrackSearch
       instead when ordering and membership must stay fixed across later reads. */
    inline mc_request_id QueryTracks(
        Context &context, const Library &library, const TrackQuery &query, TrackQueryCompletion completion) {
        const auto fields = detail::fieldViews(query.fields);

        mc_track_query options = MC_TRACK_QUERY_DEFAULT;
        options.search = { query.search.data(), query.search.size() };
        options.fields = fields.data();
        options.field_count = fields.size();
        options.offset = query.offset;
        options.limit = query.limit;

        return detail::submitLibraryRequest(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_library_query_tracks_async(context.Get(), library.Get(), &options,
                detail::completeLibraryRequest<mc_track_page, TrackPage, TrackQueryCompletion>, state, request);
        });
    }

    /* Open an ordered ID selection for repeated paging or playback. Metadata
       is loaded separately, so large searches need not copy every track's tags. */
    inline mc_request_id OpenTrackSearch(Context &context, const Library &library, const TrackSearch &search,
        TrackSearchCompletion completion) {
        mc_track_search options = MC_TRACK_SEARCH_DEFAULT;
        options.text = { search.text.data(), search.text.size() };
        options.mode = search.mode;
        options.order = search.order;
        return detail::submitLibraryRequest(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_library_open_track_search_async(context.Get(), library.Get(), &options,
                detail::completeLibraryRequest<mc_track_result, TrackResult, TrackSearchCompletion>, state, request);
        });
    }

    /* Open a saved playlist as a stable result in playlist order, including
       repeated tracks. Use for display, metadata paging, or queue replacement. */
    inline mc_request_id OpenPlaylist(Context &context, const Library &library, int64_t playlistId,
        TrackSearchCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_library_open_playlist_async(context.Get(), library.Get(), playlistId,
                detail::completeLibraryRequest<mc_track_result, TrackResult, TrackSearchCompletion>, state, request);
        });
    }

    /* Open a browse category, such as an album or artist, using its library
       catalog ID. Use the same ordered-result flow as search and playlists. */
    inline mc_request_id OpenCategory(Context& context, const Library& library, std::string_view category,
        int64_t id, mc_track_search_order order, TrackSearchCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
            return mc_library_open_category_async(context.Get(), library.Get(), {category.data(), category.size()}, id,
                order, detail::completeLibraryRequest<mc_track_result, TrackResult, TrackSearchCompletion>, state, request);
        });
    }

    /* Open the library's ordered selection below a directory for a file-browse
       view. The backend resolves the path; the wrapper does not scan files. */
    inline mc_request_id OpenDirectory(Context& context, const Library& library, std::string_view directory,
        TrackSearchCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
            return mc_library_open_directory_async(context.Get(), library.Get(), {directory.data(), directory.size()},
                detail::completeLibraryRequest<mc_track_result, TrackResult, TrackSearchCompletion>, state, request);
        });
    }

    /* Copy known library-scoped IDs into a stable result for reads or playback,
       preserving order and duplicates. This does not load metadata or check
       whether every positive ID still exists; invalid arguments throw Error. */
    inline TrackResult MakeTrackResult(const Library& library, const int64_t* ids, size_t count) {
        mc_track_result* result = nullptr;
        const auto status = mc_library_make_track_result(library.Get(), ids, count, &result);
        if (status != MC_OK) throw Error(status);
        return TrackResult(result);
    }

    /* Playlist ID and name returned by ListPlaylists. Keep entries for menus
       or later operations after the callback returns; IDs belong to the source
       library. Copyable and movable, owning the UTF-8 name. */
    struct PlaylistEntry {
        int64_t id;
        std::string name;
    };

    /* List callbacks receive owned names/entries; write callbacks receive an
       ID. Both report status and a diagnostic valid only during the callback. */
    using PlaylistListCompletion = std::function<void(mc_result, std::vector<PlaylistEntry>, std::string_view)>;
    using PlaylistWriteCompletion = std::function<void(mc_result, int64_t, std::string_view)>;

    namespace detail {
        /* Reuse one terminal adapter for playlist writes so callback state is
           released once while the C error remains valid during user delivery. */
        inline void MC_CALL completePlaylistWrite(mc_result status, int64_t id, const mc_error* error, void* data) {
            const std::unique_ptr<PlaylistWriteCompletion> callback(static_cast<PlaylistWriteCompletion*>(data));
            (*callback)(status, id, AsStringView(mc_error_get_message(error)));
        }
    }

    /* List saved playlists for a picker or later edits. Copies the C callback's
       temporary names into PlaylistEntry values that can outlive the callback. */
    inline mc_request_id ListPlaylists(Context& context, const Library& library, PlaylistListCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
            return mc_library_list_playlists_async(context.Get(), library.Get(),
                [](mc_result status, const mc_playlist_entry* entries, size_t count, const mc_error* error, void* data) {
                    std::unique_ptr<PlaylistListCompletion> callback(static_cast<PlaylistListCompletion*>(data));
                    std::vector<PlaylistEntry> playlists;
                    playlists.reserve(count);
                    for (size_t i = 0; i < count; ++i) {
                        playlists.push_back({entries[i].id, std::string(AsStringView(entries[i].name))});
                    }
                    (*callback)(status, std::move(playlists), AsStringView(mc_error_get_message(error)));
                }, state, request);
        });
    }

    /* Save ordered IDs, including duplicates. Zero creates a playlist using
       name; a positive ID replaces its tracks. Use RenamePlaylist to rename. */
    inline mc_request_id SavePlaylist(Context& context, const Library& library, int64_t id,
        std::string_view name, const std::vector<int64_t>& tracks, PlaylistWriteCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
            return mc_library_save_playlist_async(context.Get(), library.Get(), id, {name.data(), name.size()},
                tracks.data(), tracks.size(),
                detail::completePlaylistWrite, state, request);
        });
    }

    /* Add IDs to the end of an existing playlist without replacing its saved
       tracks. Use for an Add to Playlist action; order/duplicates are preserved. */
    inline mc_request_id AppendPlaylist(Context& context, const Library& library, int64_t id,
        const std::vector<int64_t>& tracks, PlaylistWriteCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
            return mc_library_append_playlist_async(context.Get(), library.Get(), id, tracks.data(), tracks.size(),
                detail::completePlaylistWrite, state, request);
        });
    }

    /* Change a saved playlist's name while keeping its track sequence intact. */
    inline mc_request_id RenamePlaylist(Context& context, const Library& library, int64_t id,
        std::string_view name, PlaylistWriteCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
            return mc_library_rename_playlist_async(context.Get(), library.Get(), id, {name.data(), name.size()},
                detail::completePlaylistWrite, state, request);
        });
    }

    /* Remove a saved playlist from its library; this does not remove its music
       files. Use after any confirmation required by the caller's UI. */
    inline mc_request_id DeletePlaylist(Context& context, const Library& library, int64_t id,
        PlaylistWriteCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void* state, mc_request_id* request) {
            return mc_library_delete_playlist_async(context.Get(), library.Get(), id,
                detail::completePlaylistWrite, state, request);
        });
    }

    /* Load current metadata for positions in an existing stable result. Use for
       visible list rows; removed tracks keep their positions with no values. */
    inline mc_request_id ReadTracks(Context &context, const TrackResult &tracks,
        const TrackRange &range, TrackQueryCompletion completion) {
        const auto fields = detail::fieldViews(range.fields);
        mc_track_range options = MC_TRACK_RANGE_DEFAULT;
        options.offset = range.offset;
        options.limit = range.limit;
        options.fields = fields.data();
        options.field_count = fields.size();
        return detail::submitLibraryRequest(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_track_result_read_async(context.Get(), tracks.Get(), &options,
                detail::completeLibraryRequest<mc_track_page, TrackPage, TrackQueryCompletion>, state, request);
        });
    }

    /* Resolve one retained identity against its original library. Use after
       releasing the source result/page; missing tracks report NOT_FOUND. */
    inline mc_request_id ReadTrack(Context &context, const TrackRef &track,
        const std::vector<std::string_view> &names, TrackQueryCompletion completion) {
        const auto fields = detail::fieldViews(names);
        return detail::submitLibraryRequest(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_track_ref_read_async(context.Get(), track.Get(), fields.data(), fields.size(),
                detail::completeLibraryRequest<mc_track_page, TrackPage, TrackQueryCompletion>, state, request);
        });
    }

    /* Read one identity using the wrapper's default display fields: title,
       artist, album, and duration. Use the overload above for other fields. */
    inline mc_request_id ReadTrack(Context &context, const TrackRef &track, TrackQueryCompletion completion) {
        return ReadTrack(context, track, TrackRange{}.fields, std::move(completion));
    }

} } }

#endif
