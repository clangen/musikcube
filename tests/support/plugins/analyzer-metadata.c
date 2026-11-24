#include <sqlite3.h>
#include <stdlib.h>

/* inspect committed data after the indexer closes its database. */
int main(int argc, char** argv) {
    sqlite3* database = NULL;
    sqlite3_stmt* query = NULL;
    if ((argc != 3 && argc != 4) || sqlite3_open_v2(argv[1], &database, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        return 1;
    }
    const char* sql = "SELECT count(*) FROM track_meta t "
        "JOIN meta_values v ON v.id = t.meta_value_id "
        "JOIN meta_keys k ON k.id = v.meta_key_id "
        "WHERE k.name = 'analyzer_test' AND v.content = 'present'";
    int ok = sqlite3_prepare_v2(database, sql, -1, &query, NULL) == SQLITE_OK &&
        sqlite3_step(query) == SQLITE_ROW && sqlite3_column_int(query, 0) == atoi(argv[2]);
    sqlite3_finalize(query);
    if (ok && argc == 4) {
        query = NULL;
        ok = sqlite3_prepare_v2(database, "SELECT count(*) FROM thumbnails", -1, &query, NULL) == SQLITE_OK &&
            sqlite3_step(query) == SQLITE_ROW && sqlite3_column_int(query, 0) == atoi(argv[3]);
        sqlite3_finalize(query);
    }
    sqlite3_close(database);
    return ok ? 0 : 1;
}
