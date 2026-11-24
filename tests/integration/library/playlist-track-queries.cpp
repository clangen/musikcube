#include <musikcore/db/Connection.h>
#include <musikcore/library/query/PlaylistTrackQueries.h>

#include <cstdio>

/* query inputs must remain valid when the caller changes its original buffer. */
int main() {
    musik::core::db::Connection database;
    if (database.Open(":memory:") != musik::core::db::Okay ||
        database.Execute("CREATE TABLE tracks (id INTEGER PRIMARY KEY, external_id TEXT)") != musik::core::db::Okay ||
        database.Execute("INSERT INTO tracks VALUES (42, 'first')") != musik::core::db::Okay)
    {
        std::fputs("could not create track fixture\n", stderr);
        return 1;
    }

    char value[] = "first";
    const char* ids[] = { value };
    musik::core::library::query::ExternalIdListToTrackListQuery query(nullptr, ids, 1);
    value[0] = 'x';
    if (!query.Run(database) || query.GetResult()->Count() != 1 || query.GetResult()->GetId(0) != 42) {
        std::fputs("track lookup did not retain its inputs\n", stderr);
        return 1;
    }
    return 0;
}
