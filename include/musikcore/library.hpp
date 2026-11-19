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

namespace musik { namespace core { namespace client {

    /* Copyable and movable. Borrows a row and its strings while the page storage is owned. */
    class TrackView {
    public:
        int64_t id() const noexcept { return mc_track_page_get_id(page_, row_); }
        explicit operator bool() const noexcept { return id() != 0; }

        /* Missing fields have null data. An explicit empty value has non-null data. */
        std::string_view title() const noexcept { return value(MC_TRACK_FIELD_TITLE); }
        std::string_view artist() const noexcept { return value(MC_TRACK_FIELD_ARTIST); }
        std::string_view album() const noexcept { return value(MC_TRACK_FIELD_ALBUM); }
        std::string_view album_artist() const noexcept { return value(MC_TRACK_FIELD_ALBUM_ARTIST); }
        std::string_view genre() const noexcept { return value(MC_TRACK_FIELD_GENRE); }
        std::string_view filename() const noexcept { return value(MC_TRACK_FIELD_FILENAME); }
        std::optional<int64_t> track_number() const noexcept { return number(MC_TRACK_FIELD_TRACK); }
        std::optional<int64_t> disc_number() const noexcept { return number(MC_TRACK_FIELD_DISC); }
        std::optional<std::chrono::seconds> duration() const noexcept {
            const auto seconds = number(MC_TRACK_FIELD_DURATION);
            if (!seconds || *seconds < 0) return std::nullopt;
            return std::chrono::seconds(*seconds);
        }

        std::string_view value(std::string_view key, size_t index = 0) const noexcept {
            return as_string_view(mc_track_page_get_string_at(page_, row_, field_index(key), index));
        }

        /* The vector owns its elements. Each string view still borrows the page. */
        std::vector<std::string_view> values(std::string_view key) const {
            const auto field = field_index(key);
            const auto count = mc_track_page_get_value_count(page_, row_, field);
            std::vector<std::string_view> result;
            result.reserve(count);
            for (size_t index = 0; index < count; ++index) {
                result.push_back(as_string_view(mc_track_page_get_string_at(page_, row_, field, index)));
            }
            return result;
        }

    private:
        friend class TrackPage;
        TrackView(const mc_track_page *page, size_t row) noexcept : page_(page), row_(row) {}

        size_t field_index(std::string_view key) const noexcept {
            const auto count = mc_track_page_get_field_count(page_);
            for (size_t index = 0; index < count; ++index) {
                if (as_string_view(mc_track_page_get_field_name(page_, index)) == key) return index;
            }
            return count;
        }

        std::optional<int64_t> number(std::string_view key) const noexcept {
            const auto text = value(key);
            if (text.empty()) return std::nullopt;
            int64_t result = 0;
            const auto end = text.data() + text.size();
            const auto parsed = std::from_chars(text.data(), end, result);
            if (parsed.ec != std::errc{} || parsed.ptr != end) return std::nullopt;
            return result;
        }

        const mc_track_page *page_;
        size_t row_;
    };

    /* Move-only. Moving transfers ownership and leaves the source empty. */
    class TrackPage {
    public:
        TrackPage() = default;
        /* Takes ownership of a page returned by the C ABI. */
        explicit TrackPage(mc_track_page *page) noexcept : page_(page) {}

        TrackPage(const TrackPage &) = delete;
        TrackPage &operator=(const TrackPage &) = delete;
        TrackPage(TrackPage &&) noexcept = default;
        TrackPage &operator=(TrackPage &&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(page_); }
        size_t size() const noexcept { return mc_track_page_get_count(page_.get()); }
        int64_t id(size_t row) const noexcept { return mc_track_page_get_id(page_.get(), row); }
        size_t field_count() const noexcept { return mc_track_page_get_field_count(page_.get()); }
        TrackView track(size_t row) const & noexcept { return TrackView(page_.get(), row); }
        TrackView track(size_t row) const && = delete;
        size_t value_count(size_t row, size_t field) const noexcept {
            return mc_track_page_get_value_count(page_.get(), row, field);
        }

        /* Views borrow from this page. Missing values have null data. */
        std::string_view field_name(size_t field) const noexcept {
            return as_string_view(mc_track_page_get_field_name(page_.get(), field));
        }
        std::string_view value(size_t row, size_t field, size_t index = 0) const noexcept {
            return as_string_view(mc_track_page_get_string_at(page_.get(), row, field, index));
        }

    private:
        struct Destroy {
            void operator()(mc_track_page *page) const noexcept { mc_track_page_destroy(&page); }
        };
        std::unique_ptr<mc_track_page, Destroy> page_;
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
        explicit TrackRef(mc_track_ref *track) noexcept : track_(track) {}
        TrackRef(const TrackRef &) = delete;
        TrackRef &operator=(const TrackRef &) = delete;
        TrackRef(TrackRef &&) noexcept = default;
        TrackRef &operator=(TrackRef &&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(track_); }
        int64_t id() const noexcept { return mc_track_ref_get_id(get()); }
        const mc_track_ref *get() const noexcept { return track_.get(); }

    private:
        struct Destroy {
            void operator()(mc_track_ref *track) const noexcept { mc_track_ref_destroy(&track); }
        };
        std::unique_ptr<mc_track_ref, Destroy> track_;
    };

    /* Move-only. Moving transfers ownership and leaves the source empty. */
    class TrackResult {
    public:
        TrackResult() = default;
        explicit TrackResult(mc_track_result *tracks) noexcept : tracks_(tracks) {}

        TrackResult(const TrackResult &) = delete;
        TrackResult &operator=(const TrackResult &) = delete;
        TrackResult(TrackResult &&) noexcept = default;
        TrackResult &operator=(TrackResult &&) noexcept = default;

        explicit operator bool() const noexcept { return static_cast<bool>(tracks_); }
        const mc_track_result *get() const noexcept { return tracks_.get(); }
        size_t size() const noexcept { return mc_track_result_get_count(get()); }
        int64_t id(size_t row) const noexcept { return mc_track_result_get_id(get(), row); }
        bool starts_album(size_t row) const noexcept { return mc_track_result_starts_album(get(), row); }
        TrackRef track(size_t row) const {
            mc_track_ref *track = nullptr;
            const auto status = mc_track_result_get_ref(get(), row, &track);
            if (status != MC_OK) throw Error(status);
            return TrackRef(track);
        }

    private:
        struct Destroy {
            void operator()(mc_track_result *tracks) const noexcept { mc_track_result_destroy(&tracks); }
        };
        std::unique_ptr<mc_track_result, Destroy> tracks_;
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
        inline void MC_CALL complete_library_request(
            mc_result result, Handle *handle, const mc_error *error, void *data) {
            std::unique_ptr<Completion> completion(static_cast<Completion *>(data));
            (*completion)(result, Result(handle), as_string_view(mc_error_get_message(error)));
        }

        template<typename Completion, typename Start>
        mc_request_id submit_library_request(Completion completion, Start start) {
            if (!completion) throw Error(MC_ERROR_INVALID_ARGUMENT);

            /* An accepted request owns the callback state until its terminal callback. */
            auto state = std::make_unique<Completion>(std::move(completion));
            mc_request_id request = 0;
            const auto result = start(state.get(), &request);
            if (result != MC_OK) throw Error(result);
            state.release();
            return request;
        }

        inline std::vector<mc_string_view> field_views(const std::vector<std::string_view> &names) {
            std::vector<mc_string_view> fields;
            fields.reserve(names.size());
            for (const auto name : names) fields.push_back({ name.data(), name.size() });
            return fields;
        }
    }

    inline mc_request_id query_tracks(
        Context &context, const TrackQuery &query, TrackQueryCompletion completion) {
        const auto fields = detail::field_views(query.fields);

        mc_track_query options = MC_TRACK_QUERY_DEFAULT;
        options.search = { query.search.data(), query.search.size() };
        options.fields = fields.data();
        options.field_count = fields.size();
        options.offset = query.offset;
        options.limit = query.limit;

        return detail::submit_library_request(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_library_query_tracks_async(context.get(), &options,
                detail::complete_library_request<mc_track_page, TrackPage, TrackQueryCompletion>, state, request);
        });
    }

    inline mc_request_id open_track_search(Context &context, const TrackSearch &search,
        TrackSearchCompletion completion) {
        mc_track_search options = MC_TRACK_SEARCH_DEFAULT;
        options.text = { search.text.data(), search.text.size() };
        options.mode = search.mode;
        options.order = search.order;
        return detail::submit_library_request(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_library_open_track_search_async(context.get(), &options,
                detail::complete_library_request<mc_track_result, TrackResult, TrackSearchCompletion>, state, request);
        });
    }

    inline mc_request_id read_tracks(Context &context, const TrackResult &tracks,
        const TrackRange &range, TrackQueryCompletion completion) {
        const auto fields = detail::field_views(range.fields);
        mc_track_range options = MC_TRACK_RANGE_DEFAULT;
        options.offset = range.offset;
        options.limit = range.limit;
        options.fields = fields.data();
        options.field_count = fields.size();
        return detail::submit_library_request(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_track_result_read_async(context.get(), tracks.get(), &options,
                detail::complete_library_request<mc_track_page, TrackPage, TrackQueryCompletion>, state, request);
        });
    }

    inline mc_request_id read_track(Context &context, const TrackRef &track,
        const std::vector<std::string_view> &names, TrackQueryCompletion completion) {
        const auto fields = detail::field_views(names);
        return detail::submit_library_request(std::move(completion), [&](void *state, mc_request_id *request) {
            return mc_track_ref_read_async(context.get(), track.get(), fields.data(), fields.size(),
                detail::complete_library_request<mc_track_page, TrackPage, TrackQueryCompletion>, state, request);
        });
    }

    inline mc_request_id read_track(Context &context, const TrackRef &track, TrackQueryCompletion completion) {
        return read_track(context, track, TrackRange{}.fields, std::move(completion));
    }

} } }

#endif
