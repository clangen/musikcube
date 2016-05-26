#include "stdafx.h"
#include "CategoryListViewQuery.h"

#include <core/library/LocalLibraryConstants.h>
#include <core/db/Statement.h>

#include <boost/thread/mutex.hpp>

#include <map>

using musik::core::db::Statement;
using musik::core::db::Row;

using namespace musik::core::db;
using namespace musik::core::library::constants;
using namespace musik::box;

#define reset(x) x.reset(new std::vector<std::shared_ptr<Result> >);

static const std::string ALBUM_QUERY =
    "SELECT DISTINCT albums.id, albums.name "
    "FROM albums, tracks "
    "WHERE albums.id = tracks.album_id "
    "ORDER BY albums.sort_order;";

static const std::string ARTIST_QUERY =
    "SELECT DISTINCT artists.id, artists.name "
    "FROM artists, tracks "
    "WHERE artists.id = tracks.visual_artist_id "
    "ORDER BY artists.sort_order;";

static const std::string GENRE_QUERY =
    "SELECT DISTINCT genres.id, genres.name "
    "FROM genres, tracks "
    "WHERE genres.id = tracks.visual_genre_id "
    "ORDER BY genres.sort_order;";

static boost::mutex QUERY_MAP_MUTEX;
static std::map<std::string, std::string> FIELD_TO_QUERY_MAP;

static void initFieldToQueryMap() {
    FIELD_TO_QUERY_MAP[Track::ALBUM_ID] = ALBUM_QUERY;
    FIELD_TO_QUERY_MAP[Track::ARTIST_ID] = ARTIST_QUERY;
    FIELD_TO_QUERY_MAP[Track::GENRE_ID] = GENRE_QUERY;
}

CategoryListViewQuery::CategoryListViewQuery(const std::string& trackField) {
    this->trackField = trackField;

    reset(result);

    {
        boost::mutex::scoped_lock lock(QUERY_MAP_MUTEX);

        if (!FIELD_TO_QUERY_MAP.size()) {
            initFieldToQueryMap();
        }
    }

    if (FIELD_TO_QUERY_MAP.find(trackField) == FIELD_TO_QUERY_MAP.end()) {
        throw "invalid field for CategoryListView specified";
    }
}

CategoryListViewQuery::~CategoryListViewQuery() {

}

CategoryListViewQuery::ResultList CategoryListViewQuery::GetResult() {
    return this->result;
}

bool CategoryListViewQuery::OnRun(Connection& db) {
    reset(result);

    std::string query = FIELD_TO_QUERY_MAP[this->trackField];
    Statement stmt(query.c_str(), db);

    while (stmt.Step() == Row) {
        std::shared_ptr<Result> row(new Result());
        row->id = stmt.ColumnInt64(0);
        row->displayValue = stmt.ColumnText(1);
        result->push_back(row);
    }

    return true;
}
