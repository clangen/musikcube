#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace musik { namespace core { namespace library { namespace query {

    /* a query-owned selection snapshot. copies own their source IDs; replacing
    the library's selection cannot change an already copied scope. */
    class LibraryQueryScope {
    public:
        /* the default scope includes every source, including newly added ones. */
        LibraryQueryScope() = default;

        /* an empty selection matches nothing. source zero selects ordinary files.
        negative source IDs are invalid; scan-source sentinels are not selections. */
        static LibraryQueryScope ForSources(std::vector<int32_t> ids) {
            if (std::any_of(ids.begin(), ids.end(),
                    [](int32_t id) {
                        return id < 0;
                    }))
            {
                throw std::invalid_argument("library scope source IDs must be nonnegative");
            }
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            return LibraryQueryScope(std::move(ids));
        }

        bool MatchesNoTracks() const { return this->sourceIds && this->sourceIds->empty(); }

        bool IsUnrestricted() const { return !this->sourceIds.has_value(); }

        const std::optional<std::vector<int32_t>>& SourceIds() const { return this->sourceIds; }

    private:
        explicit LibraryQueryScope(std::vector<int32_t> ids)
        : sourceIds(std::move(ids)) {}

        std::optional<std::vector<int32_t>> sourceIds;
    };

} } } }
