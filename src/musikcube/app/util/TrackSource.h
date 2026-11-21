#pragma once

#include <musikcore/library.hpp>

#include <variant>

namespace musik { namespace cube {
    struct CategoryTracks {
        std::string field;
        int64_t id;
        mc_track_search_order order;

        bool operator==(const CategoryTracks&) const = default;

        mc_request_id Open(musik::core::api::Context& context, const musik::core::api::Library& library,
            musik::core::api::TrackSearchCompletion completion) const
        {
            return musik::core::api::OpenCategory(context, library, this->field, this->id, this->order,
                std::move(completion));
        }
    };

    struct DirectoryTracks {
        std::string path;

        bool operator==(const DirectoryTracks&) const = default;

        mc_request_id Open(musik::core::api::Context& context, const musik::core::api::Library& library,
            musik::core::api::TrackSearchCompletion completion) const
        {
            return musik::core::api::OpenDirectory(context, library, this->path, std::move(completion));
        }
    };

    using TrackSource = std::variant<CategoryTracks, DirectoryTracks>;
} }
