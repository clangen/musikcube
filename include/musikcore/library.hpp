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

    /* Copyable and movable. Borrows a row and its strings while the page storage is owned. */
    class TrackView {
    public:
        int64_t Id() const noexcept { return mc_track_page_get_id(page, rowIndex); }
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
            return AsStringView(mc_track_page_get_string_at(page, rowIndex, FieldIndex(key), index));
        }

        /* The vector owns its elements. Each string view still borrows the page. */
        std::vector<std::string_view> Values(std::string_view key) const {
            const auto field = FieldIndex(key);
            const auto count = mc_track_page_get_value_count(page, rowIndex, field);
            std::vector<std::string_view> result;
            result.reserve(count);
            for (size_t index = 0; index < count; ++index) {
                result.push_back(AsStringView(mc_track_page_get_string_at(page, rowIndex, field, index)));
            }
            return result;
        }

    private:
        friend class TrackPage;
        TrackView(const mc_track_page *page, size_t row) noexcept : page(page), rowIndex(row) {}

        size_t FieldIndex(std::string_view key) const noexcept {
            const auto count = mc_track_page_get_field_count(page);
            for (size_t index = 0; index < count; ++index) {
                if (AsStringView(mc_track_page_get_field_name(page, index)) == key) return index;
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

        const mc_track_page *page;
        size_t rowIndex;
    };

    /* Move-only. Moving transfers ownership and leaves the source empty. */
    class TrackPage {
    public:
        TrackPage() = default;
        /* Takes ownership of a page returned by the C ABI. */
        explicit TrackPage(mc_track_page *page) noexcept : page(page) {}

        TrackPage(const TrackPage &) = delete;
        TrackPage &operator=(const TrackPage &) = delete;
        TrackPage(TrackPage &&) noexcept = default;
        TrackPage &operator=(TrackPage &&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(page); }
        size_t Size() const noexcept { return mc_track_page_get_count(page.get()); }
        int64_t Id(size_t row) const noexcept { return mc_track_page_get_id(page.get(), row); }
        size_t FieldCount() const noexcept { return mc_track_page_get_field_count(page.get()); }
        TrackView Track(size_t row) const & noexcept { return TrackView(page.get(), row); }
        TrackView Track(size_t row) const && = delete;
        size_t ValueCount(size_t row, size_t field) const noexcept {
            return mc_track_page_get_value_count(page.get(), row, field);
        }

        /* Views borrow from this page. Missing values have null data. */
        std::string_view FieldName(size_t field) const noexcept {
            return AsStringView(mc_track_page_get_field_name(page.get(), field));
        }
        std::string_view Value(size_t row, size_t field, size_t index = 0) const noexcept {
            return AsStringView(mc_track_page_get_string_at(page.get(), row, field, index));
        }

    private:
        struct Destroy {
            void operator()(mc_track_page *page) const noexcept { mc_track_page_destroy(&page); }
        };
        std::unique_ptr<mc_track_page, Destroy> page;
    };

    /* Copyable and movable. The search and field views borrow their strings. */
    struct TrackQuery {
        /* Title substring and field names are copied during submission. */
        std::string_view search;
        std::vector<std::string_view> fields;
        size_t offset = 0;
        size_t limit = 0;
    };

    /* The callback owns its page and borrows the error message for the call. */
    using TrackQueryCompletion = std::function<void(mc_result, TrackPage, std::string_view)>;

    /* Move-only. Owns a library-scoped identity independently of metadata pages. */
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
        struct Destroy {
            void operator()(mc_track_ref *track) const noexcept { mc_track_ref_destroy(&track); }
        };
        std::unique_ptr<mc_track_ref, Destroy> identity;
    };

    /* Move-only. Moving transfers ownership and leaves the source empty. */
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
        struct Destroy {
            void operator()(mc_track_result *tracks) const noexcept { mc_track_result_destroy(&tracks); }
        };
        std::unique_ptr<mc_track_result, Destroy> tracks;
    };

    /* Copyable and movable. The text view borrows its input until submission. */
    struct TrackSearch {
        std::string_view text;
        mc_track_search_mode mode = MC_TRACK_SEARCH_SUBSTRING;
        mc_track_search_order order = MC_TRACK_SEARCH_ALBUM;
    };

    /* Copyable and movable. The field views borrow their strings until submission. */
    struct TrackRange {
        size_t offset = 0;
        size_t limit = 0;
        std::vector<std::string_view> fields = {
            MC_TRACK_FIELD_TITLE, MC_TRACK_FIELD_ARTIST, MC_TRACK_FIELD_ALBUM, MC_TRACK_FIELD_DURATION
        };
    };

    using TrackSearchCompletion = std::function<void(mc_result, TrackResult, std::string_view)>;

    namespace detail {
        template<typename Handle, typename Result, typename Completion>
        inline void MC_CALL completeLibraryRequest(
            mc_result result, Handle *handle, const mc_error *error, void *data) {
            std::unique_ptr<Completion> completion(static_cast<Completion *>(data));
            (*completion)(result, Result(handle), AsStringView(mc_error_get_message(error)));
        }

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

        inline std::vector<mc_string_view> fieldViews(const std::vector<std::string_view> &names) {
            std::vector<mc_string_view> fields;
            fields.reserve(names.size());
            for (const auto name : names) fields.push_back({ name.data(), name.size() });
            return fields;
        }
    }

    /* Move-only owner of a library identity. Requests retain their own reference. */
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

        /* Borrows the handle for the lifetime of this wrapper. */
        const mc_library *Get() const noexcept { return library.get(); }

    private:
        /* Stateless deleter. Copying or moving it has no effect on ownership. */
        struct Deleter {
            void operator()(mc_library *handle) const noexcept { mc_library_destroy(&handle); }
        };
        std::unique_ptr<mc_library, Deleter> library;
    };

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

    inline mc_request_id OpenPlaylist(Context &context, const Library &library, int64_t playlistId,
        TrackSearchCompletion completion) {
        return detail::submitLibraryRequest(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_library_open_playlist_async(context.Get(), library.Get(), playlistId,
                detail::completeLibraryRequest<mc_track_result, TrackResult, TrackSearchCompletion>, state, request);
        });
    }

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

    inline mc_request_id ReadTrack(Context &context, const TrackRef &track,
        const std::vector<std::string_view> &names, TrackQueryCompletion completion) {
        const auto fields = detail::fieldViews(names);
        return detail::submitLibraryRequest(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_track_ref_read_async(context.Get(), track.Get(), fields.data(), fields.size(),
                detail::completeLibraryRequest<mc_track_page, TrackPage, TrackQueryCompletion>, state, request);
        });
    }

    inline mc_request_id ReadTrack(Context &context, const TrackRef &track, TrackQueryCompletion completion) {
        return ReadTrack(context, track, TrackRange{}.fields, std::move(completion));
    }

} } }

#endif
