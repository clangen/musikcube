#ifndef MUSIKCORE_TRACK_READ_QUERIES_H
#define MUSIKCORE_TRACK_READ_QUERIES_H

#include "util/TrackSort.h"

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

    struct TrackSearchQuery {
        TrackSearchQuery(const std::string &filter, bool regex, TrackSortType order,
            int limit = -1, int offset = 0, const std::string &unknownAlbum = {})
            : filter(filter), regex(regex), order(order), limit(limit), offset(offset), unknownAlbum(unknownAlbum) {}

        std::string filter;
        bool regex;
        TrackSortType order;
        int limit = -1;
        int offset = 0;
        std::string unknownAlbum;

        bool Run(sqlite3 *database, TrackSearchRows &result) const;
    };

    struct ProjectedTrackPage {
        struct Row {
            int64_t id;
            std::vector<std::vector<std::string>> fields;
        };
        std::vector<std::string> names;
        std::vector<Row> rows;
    };

    /* When IDs are supplied, count rows from that ordered range rather than a title search. */
    struct TrackPageQuery {
        TrackPageQuery(const std::string &search, const std::vector<std::string> &fields,
            size_t offset, size_t limit, const int64_t *ids = nullptr, bool keepMissing = true)
            : search(search), fields(fields), offset(offset), limit(limit), ids(ids), keepMissing(keepMissing) {}

        const std::string &search;
        const std::vector<std::string> &fields;
        size_t offset;
        size_t limit;
        const int64_t *ids = nullptr;
        bool keepMissing = true;

        bool Run(sqlite3 *database, ProjectedTrackPage &page) const;
    };

} } } }

#endif
