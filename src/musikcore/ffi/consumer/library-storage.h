#pragma once

#include <musikcore/library.h>
#include "library-scope.h"
#include <musikcore/library/query/TrackReadBackend.h>
#include <stop_token>

namespace musik { namespace core {
    class LibraryBackend;
} }
struct mc_track_page : musik::core::library::query::ProjectedTrackPage {};

namespace musik { namespace core { namespace library {
    /* one library owns its selection and executes its operations. handles,
    results and references share it, retaining the original catalog and remote
    connection across selection changes. native implementations schedule typed
    queries; catalog-only libraries read without starting the engine.
    noncopyable and nonmovable; construct through the library factories. */
    class Library : public std::enable_shared_from_this<Library> {
    public:
        using Shared = std::shared_ptr<const Library>;

        Library(const Library&) = delete;
        virtual ~Library() = default;
        Library& operator=(const Library&) = delete;

        const std::string& Locator() const { return this->locator; }

        mc_library_kind Kind() const { return this->kind; }

        const std::shared_ptr<LibraryScopeState>& GetScopeState() const { return this->scope; }

        virtual bool Matches(const musik::core::LibraryBackend*) const { return false; }

        virtual std::shared_ptr<musik::core::LibraryBackend> NativeBackend() const { return {}; }

        /* synchronous projection for plugin workers already outside the context loop. */
        virtual mc_result ReadPageNow(const musik::core::library::query::TrackPageQuery&,
            musik::core::library::query::ProjectedTrackPage&, const std::atomic_bool&) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result QueryTracks(mc_context*, const musik::core::library::query::TrackPageQuery&,
            mc_track_query_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result Search(mc_context*, const musik::core::library::query::TrackSearchQuery&,
            mc_track_search_completion_fn, void*, mc_request_id*) const = 0;

        virtual mc_result ReadPage(mc_context*, std::vector<int64_t>, std::vector<std::string>, bool,
            mc_track_query_completion_fn, void*, mc_request_id*, const void* owner = nullptr,
            size_t total = 0) const = 0;

        virtual mc_result OpenPlaylist(
            mc_context*, int64_t, mc_track_search_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result OpenSelection(mc_context*, std::string, std::string, int64_t,
            musik::core::library::query::TrackSortType, mc_track_search_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result ListSources(mc_context*, mc_library_sources_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result ListPlaylists(mc_context*, mc_playlist_list_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result SavePlaylist(mc_context*, int64_t, std::string, std::vector<int64_t>, bool,
            mc_playlist_write_completion_fn, void*, mc_request_id*, std::optional<int32_t> = {}) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result RenamePlaylist(
            mc_context*, int64_t, std::string, mc_playlist_write_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result DeletePlaylist(
            mc_context*, int64_t, mc_playlist_write_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result SetTrackRating(
            mc_context*, int64_t, uint32_t, mc_library_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result ListCategoryFields(
            mc_context*, mc_category_fields_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result QueryCategories(mc_context*, std::string, std::string, mc_track_search_mode, std::string,
            mc_track_search_mode, std::vector<std::pair<std::string, int64_t>>, int64_t, size_t, size_t,
            mc_category_page_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result ListAlbums(
            mc_context*, std::string, mc_album_list_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result ReadThumbnailAsync(
            mc_context*, int64_t, mc_thumbnail_completion_fn, void*, mc_request_id*) const {
            return MC_ERROR_UNSUPPORTED;
        }

        virtual mc_result ReadThumbnail(int64_t, std::vector<uint8_t>&, std::stop_token) const {
            return MC_ERROR_UNSUPPORTED;
        }

    protected:
        Library(std::string locator, mc_library_kind kind)
        : locator(std::move(locator))
        , kind(kind) {}

    private:
        const std::string locator;
        const mc_library_kind kind;
        const std::shared_ptr<LibraryScopeState> scope = std::make_shared<LibraryScopeState>();
    };

} } }
struct mc_library {
    std::shared_ptr<const musik::core::library::Library> instance;
};

struct TrackResultData : musik::core::library::query::TrackSearchRows {
    std::shared_ptr<const musik::core::library::Library> library;
};

struct mc_track_result {
    std::shared_ptr<const TrackResultData> data;
};

struct mc_track_ref {
    std::shared_ptr<const musik::core::library::Library> library;
    int64_t id;
};
