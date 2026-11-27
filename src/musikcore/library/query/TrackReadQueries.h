#ifndef MUSIKCORE_TRACK_READ_QUERIES_H
#define MUSIKCORE_TRACK_READ_QUERIES_H

#include "util/TrackSort.h"
#include "LibraryQueryScope.h"

#include <cstddef>
#include <cstdint>
#include <vector>

struct sqlite3;

namespace musik { namespace core { namespace library { namespace query {

    struct TrackSearchRows {
        std::vector<int64_t> ids;
        std::vector<bool> albumStarts;
        std::map<size_t, uint64_t> albumDurations;
    };

    enum class TrackMatchMode { Substring, Regex };

    struct CategorySelection {
        std::string field;
        int64_t key;
    };

    inline std::vector<std::pair<std::string, int64_t>> EncodeSelections(
        const std::vector<CategorySelection>& selections) {
        std::vector<std::pair<std::string, int64_t>> result;
        for (const auto& selection : selections) {
            result.emplace_back(selection.field, selection.key);
        }
        return result;
    }
    inline std::vector<CategorySelection> DecodeSelections(
        const std::vector<std::pair<std::string, int64_t>>& selections) {
        std::vector<CategorySelection> result;
        for (const auto& selection : selections) {
            result.push_back({ selection.first, selection.second });
        }
        return result;
    }

    struct TrackSearchQuery {
        TrackSearchQuery(
            const std::string& filter, TrackMatchMode matchMode, TrackSortType order, int limit = -1, int offset = 0)
        : filter(filter)
        , matchMode(matchMode)
        , order(order)
        , limit(limit)
        , offset(offset) {}

        std::string filter;
        TrackMatchMode matchMode;
        TrackSortType order;
        int limit = -1;
        int offset = 0;

        LibraryQueryScope scope;
        std::vector<CategorySelection> selections;

        bool Run(sqlite3 *database, TrackSearchRows &result) const;
    };

    struct ProjectedTrackPage {
        struct Row {
            int64_t id;
            std::vector<std::vector<std::string>> fields;
        };
        std::vector<std::string> names;
        std::vector<Row> rows;
        size_t total = 0;
    };

    enum class TrackVisibility {
        VisibleOnly,
        IncludeHidden
    };

    /* When IDs are supplied, count rows from that ordered range rather than a title search. */
    struct TrackPageQuery {
        TrackPageQuery(const std::string &search, const std::vector<std::string> &fields,
            size_t offset, size_t limit, const int64_t *ids = nullptr, bool keepMissing = true,
            TrackVisibility visibility = TrackVisibility::VisibleOnly)
            : search(search), fields(fields), offset(offset), limit(limit), ids(ids), keepMissing(keepMissing),
              visibility(visibility) {}

        const std::string &search;
        const std::vector<std::string> &fields;
        size_t offset;
        size_t limit;
        const int64_t *ids = nullptr;
        bool keepMissing = true;
        TrackVisibility visibility = TrackVisibility::VisibleOnly;
        LibraryQueryScope scope;
        bool includeTotal = false;
        TrackMatchMode matchMode = TrackMatchMode::Substring;
        std::vector<CategorySelection> selections;

        bool Run(sqlite3 *database, ProjectedTrackPage &page) const;
    };

} } } }

#endif
