#ifndef MUSIKCORE_LIBRARY_HPP
#define MUSIKCORE_LIBRARY_HPP

#include <musikcore/context.hpp>
#include <musikcore/library.h>

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace musik { namespace core { namespace client {

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

    namespace detail {
        inline void MC_CALL complete_track_query(
            mc_result result, mc_track_page *page, const mc_error *error, void *data) {
            std::unique_ptr<TrackQueryCompletion> completion(static_cast<TrackQueryCompletion *>(data));
            (*completion)(result, TrackPage(page), as_string_view(mc_error_get_message(error)));
        }
    }

    inline mc_request_id query_tracks(
        Context &context, const TrackQuery &query, TrackQueryCompletion completion) {
        if (!completion) throw Error(MC_ERROR_INVALID_ARGUMENT);

        std::vector<mc_string_view> fields;
        fields.reserve(query.fields.size());
        for (const auto field : query.fields) fields.push_back({ field.data(), field.size() });

        mc_track_query options = MC_TRACK_QUERY_DEFAULT;
        options.search = { query.search.data(), query.search.size() };
        options.fields = fields.data();
        options.field_count = fields.size();
        options.offset = query.offset;
        options.limit = query.limit;

        /* An accepted request owns the callback state until its terminal callback. */
        auto *state = new TrackQueryCompletion(std::move(completion));
        mc_request_id request = 0;
        const auto result = mc_library_query_tracks_async(
            context.get(), &options, detail::complete_track_query, state, &request);
        if (result != MC_OK) {
            delete state;
            throw Error(result);
        }
        return request;
    }

} } }

#endif
