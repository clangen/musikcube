#include "runtime-hardening-cases.h"
#include <musikcore/musikcore.hpp>
#include "../../support/check.h"
#include "../../support/host-wakeup.h"
#include <musikcore/library/LibraryFactory.h>
#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace runtime_tests {
    using namespace musik::core;

    /* use the same isolated catalog in separate processes so process-local
    migration flags cannot accidentally make a restart test pass. */
    static std::string migrationFilename() {
        const auto encoded = (std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "1" / "musik.db").u8string();
        return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    }

    /* inspect committed state through a private-cache WAL reader. joining the
    legacy shared cache could block the very completion write being tested. */
    static int readMigrationValue(const char* sql) {
        sqlite3* raw = nullptr;
        const auto opened = sqlite3_open_v2(migrationFilename().c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE, nullptr);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        if (!TEST_CHECK(opened == SQLITE_OK)) {
            return -1;
        }
        sqlite3_busy_timeout(raw, 2000);
        sqlite3_stmt* statement = nullptr;
        const auto prepared = sqlite3_prepare_v2(raw, sql, -1, &statement, nullptr);
        const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> owned(statement, sqlite3_finalize);
        if (!TEST_CHECK(prepared == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW)) {
            return -1;
        }
        return sqlite3_column_int(statement, 0);
    }

    /* seed fixed migration/failure state without adding test-only core APIs. */
    static bool editMigrationDatabase(const char* sql) {
        sqlite3* raw = nullptr;
        const auto opened = sqlite3_open_v2(migrationFilename().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        return TEST_CHECK(opened == SQLITE_OK) &&
            TEST_CHECK(sqlite3_exec(raw, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    /* migrate without Indexer, then exit while the required scan remains pending.
    a current-schema fixture with an older version exercises the real v6->v11
    migration path without duplicating its full historical schema in tests. */
    bool prepareMigration() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_LIBRARY;
        {
            api::Context fresh(options);
        }
        if (!editMigrationDatabase("UPDATE version SET version=6")) {
            return false;
        }
        {
            api::Context migrated(options);
            mc_indexer_state state = MC_INDEXER_IDLE;
            if (!TEST_CHECK(mc_indexer_get_state(migrated.Get(), &state) == MC_ERROR_UNSUPPORTED)) {
                return false;
            }
        }
        return TEST_CHECK(readMigrationValue("SELECT version FROM version") == 11) &&
            TEST_CHECK(readMigrationValue("SELECT count(*) FROM pending_migration_scan") == 1);
    }

    /* start only Indexer and let startup resume pending work, without an explicit
    scan request. a failed completion write must preserve the marker for another
    process; successful retry must clear it in the committed scan transaction. */
    bool resumeMigration(MigrationFailure failure) {
        const bool failCompletion = failure == MigrationFailure::Completion;
        const bool failMetadata = failure == MigrationFailure::Metadata;
        if (failCompletion) {
            if (!editMigrationDatabase(
                "CREATE TABLE migration_scan_attempts(count INTEGER);"
                "INSERT INTO migration_scan_attempts VALUES(0);"
                "CREATE TRIGGER defer_migration_scan BEFORE DELETE ON pending_migration_scan BEGIN "
                "UPDATE migration_scan_attempts SET count=count+1;"
                "SELECT RAISE(FAIL, 'test completion failure'); END;"))
            {
                return false;
            }
        }
        else if (!editMigrationDatabase("DROP TRIGGER IF EXISTS defer_migration_scan")) {
            return false;
        }

        if (failMetadata && !editMigrationDatabase(
            "UPDATE tracks SET album_id=0;"
            "CREATE TRIGGER reject_metadata BEFORE UPDATE OF album_id ON tracks BEGIN "
            "SELECT RAISE(FAIL, 'test metadata write failure'); END;"))
        {
            return false;
        }
        if (!failCompletion && !failMetadata &&
            !editMigrationDatabase("DROP TRIGGER IF EXISTS reject_metadata"))
        {
            return false;
        }

        const auto trackCount = readMigrationValue("SELECT count(*) FROM tracks");
        if (!TEST_CHECK(trackCount >= 0) || (failMetadata && !TEST_CHECK(trackCount > 0))) {
            return false;
        }

        const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
            test_host_event_create(), test_host_event_destroy);
        if (!TEST_CHECK(bool(event))) {
            return false;
        }
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_INDEXER;
        options.flags = MC_CONTEXT_ENABLE_LOGGING;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        /* The scan can finish before context creation returns. Observe the
        queued final log to wait until the indexer has closed its transaction. */
        bool completed = false;
        options.log_user_data = &completed;
        options.log = [](void* data, mc_log_level, mc_string_view tag, mc_string_view message) {
            if (api::AsStringView(tag) == "Indexer" && api::AsStringView(message) == "done!") {
                *static_cast<bool*>(data) = true;
            }
        };
        api::Context context(options);
        while (!completed) {
            if (!TEST_CHECK(test_host_event_wait(event.get())) || !TEST_CHECK(context.Dispatch() == MC_OK)) {
                return false;
            }
        }
        if (!TEST_CHECK(context.Shutdown() == MC_OK) ||
            !TEST_CHECK(readMigrationValue("SELECT count(*) FROM pending_migration_scan") ==
                (failCompletion || failMetadata ? 1 : 0)))
        {
            return false;
        }
        if (failCompletion) {
            return TEST_CHECK(readMigrationValue("SELECT count FROM migration_scan_attempts") > 0);
        }
        const auto unrefreshed = readMigrationValue("SELECT count(*) FROM tracks WHERE COALESCE(album_id, 0)=0");
        return TEST_CHECK(readMigrationValue("SELECT count(*) FROM tracks") == trackCount) &&
            TEST_CHECK(unrefreshed == (failMetadata ? trackCount : 0));
    }

    /* once completed, a fresh scanner stays stopped until a caller requests work;
    the migration marker must not cause a perpetual scan on every startup. */
    bool verifyMigrationComplete() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_INDEXER;
        api::Context context(options);
        mc_indexer_state state = MC_INDEXER_IDLE;
        return TEST_CHECK(mc_indexer_get_state(context.Get(), &state) == MC_OK && state == MC_INDEXER_STOPPED) &&
            TEST_CHECK(readMigrationValue("SELECT count(*) FROM pending_migration_scan") == 0);
    }

    /* independent read snapshots must see committed WAL data while the indexer
    keeps an uncommitted write open, even with legacy shared-cache mode enabled. */
    bool checkReadIsolation() {
        const auto path = std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "musik.db";
        const auto text = path.u8string();
        const std::string filename(reinterpret_cast<const char*>(text.data()), text.size());
        sqlite3* writer = nullptr;
        sqlite3_enable_shared_cache(1);
        if (!TEST_CHECK(sqlite3_open_v2(filename.c_str(), &writer,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_SHAREDCACHE, nullptr) == SQLITE_OK))
        {
            sqlite3_close(writer);
            return false;
        }
        struct Writer {
            sqlite3* database;
            /* release the transaction even when the reader assertion fails. */
            ~Writer() {
                sqlite3_exec(database, "ROLLBACK", nullptr, nullptr, nullptr);
                sqlite3_close(database);
                sqlite3_enable_shared_cache(0);
            }
        } owned { writer };
        if (!TEST_CHECK(sqlite3_exec(writer,
            "PRAGMA journal_mode=WAL;"
            "CREATE TABLE tracks(id INTEGER PRIMARY KEY, title TEXT, visible INTEGER);"
            "INSERT INTO tracks VALUES(1, 'committed', 1);"
            "BEGIN IMMEDIATE; UPDATE tracks SET title='uncommitted' WHERE id=1;",
            nullptr, nullptr, nullptr) == SQLITE_OK))
        {
            return false;
        }
        const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
            test_host_event_create(), test_host_event_destroy);
        struct Completion {
            bool done = false;
            bool matched = false;
            std::string_view expected;
        } state;
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        api::Context context(options);
        auto libraryOptions = mc_library_options MC_LIBRARY_OPTIONS_DEFAULT;
        const auto rootText = path.parent_path().u8string();
        const std::string root(reinterpret_cast<const char*>(rootText.data()), rootText.size());
        libraryOptions.locator = { root.data(), root.size() };
        mc_library* raw = nullptr;
        if (!TEST_CHECK(mc_library_create(&libraryOptions, &raw) == MC_OK)) {
            return false;
        }
        api::Library library(raw);
        const auto callback = [](mc_result status, mc_track_page* page, const mc_error*, void* data) {
            auto& state = *static_cast<Completion*>(data);
            state.matched = status == MC_OK &&
                api::AsStringView(mc_track_page_get_string(page, 0, 0)) == state.expected;
            state.done = true;
            mc_track_page_destroy(&page);
        };
        auto query = mc_track_query MC_TRACK_QUERY_DEFAULT;
        const auto read = [&](std::string_view expected) -> bool {
            state = { false, false, expected };
            mc_request_id request = 0;
            if (!TEST_CHECK(mc_library_query_tracks_async(context.Get(), library.Get(), &query,
                callback, &state, &request) == MC_OK))
            {
                return false;
            }
            while (!state.done) {
                if (!TEST_CHECK(test_host_event_wait(event.get())) || !TEST_CHECK(context.Dispatch() == MC_OK)) {
                    return false;
                }
            }
            return TEST_CHECK(state.matched);
        };

        /* A second read on the same library must see the writer's new committed snapshot. */
        return read("committed") &&
            TEST_CHECK(sqlite3_exec(writer, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK) &&
            read("uncommitted");
    }

    bool checkLockedReadShutdown() {
        using namespace std::chrono;
        const auto directory = api::ReadPath(MC_PATH_DATA);
        const auto filename = std::filesystem::u8path(directory) / "musik.db";
        std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(nullptr, sqlite3_close);
        sqlite3* raw = nullptr;
        const auto opened = sqlite3_open(filename.string().c_str(), &raw);
        writer.reset(raw);
        if (!TEST_CHECK(opened == SQLITE_OK && sqlite3_exec(writer.get(),
            "PRAGMA journal_mode=DELETE;"
            "CREATE TABLE tracks(id INTEGER PRIMARY KEY, title TEXT, visible INTEGER);"
            "BEGIN EXCLUSIVE;",
            nullptr, nullptr, nullptr) == SQLITE_OK))
        {
            return false;
        }

        const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
            test_host_event_create(), test_host_event_destroy);
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        std::optional<mc_result> status;
        api::Context context(options);
        api::Library library(directory);
        api::QueryTracks(context, library, {}, [&](mc_result result, api::TrackPage, std::string_view) {
            status = result;
        });

        /* hold the lock through shutdown; a normal read cannot finish first. */
        if (!TEST_CHECK(!test_host_event_wait_for(event.get(), 150))) {
            return false;
        }
        const auto started = steady_clock::now();
        return TEST_CHECK(context.Shutdown() == MC_OK) &&
            TEST_CHECK(status == MC_ERROR_CANCELLED) &&
            TEST_CHECK(steady_clock::now() - started < seconds(2));
    }

    /* failed opens and corrupt schemas must not publish a partial engine or
    strand the single-context slot; retry after repairing the same directory. */
    bool checkDatabaseFailure() {
        const auto filename = std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "1" / "musik.db";
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        std::filesystem::create_directories(filename);
        mc_context* context = nullptr;
        /* each catalog-owning subset must roll back independently, including
        library-only startup where no plugin session exists to clean up. */
        for (auto services : { MC_SERVICE_LIBRARY, MC_SERVICE_INDEXER, MC_SERVICE_PLAYBACK, MC_SERVICE_ALL }) {
            options.services = services;
            const auto result = mc_context_create(&options, &context, nullptr);
            const bool rejected = result == MC_ERROR_IO && !context;
            mc_context_destroy(&context);
            if (!TEST_CHECK(rejected)) {
                return false;
            }
        }
        options.services = MC_SERVICE_ALL;
        std::filesystem::remove_all(filename);
        {
            std::ofstream corrupt(filename, std::ios::binary);
            corrupt << "this is not a SQLite database";
        }
        auto result = mc_context_create(&options, &context, nullptr);
        const bool corruptionRejected = result == MC_ERROR_IO && !context;
        mc_context_destroy(&context);
        if (!TEST_CHECK(corruptionRejected)) {
            return false;
        }
        std::filesystem::remove(filename);

        /* a failed migration must preserve the original catalog and version. */
        {
            sqlite3* raw = nullptr;
            const auto opened = sqlite3_open(filename.string().c_str(), &raw);
            const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
            if (!TEST_CHECK(opened == SQLITE_OK && sqlite3_exec(database.get(),
                "CREATE TABLE version(version INTEGER); INSERT INTO version VALUES(9);"
                "CREATE TABLE last_session_play_queue(id INTEGER PRIMARY KEY, unexpected INTEGER);",
                nullptr, nullptr, nullptr) == SQLITE_OK))
            {
                return false;
            }
        }
        result = mc_context_create(&options, &context, nullptr);
        const bool schemaRejected = result == MC_ERROR_IO && !context;
        mc_context_destroy(&context);
        if (!TEST_CHECK(schemaRejected)) {
            return false;
        }
        {
            sqlite3* raw = nullptr;
            const auto opened = sqlite3_open(filename.string().c_str(), &raw);
            const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
            int original = 0;
            if (!TEST_CHECK(opened == SQLITE_OK && sqlite3_exec(database.get(),
                "SELECT version FROM version WHERE version=9 AND NOT EXISTS "
                "(SELECT 1 FROM sqlite_master WHERE name='tracks') AND NOT EXISTS "
                "(SELECT 1 FROM sqlite_master WHERE name='pending_migration_scan');",
                [](void* data, int, char**, char**) {
                    ++*static_cast<int*>(data);
                    return 0;
                }, &original, nullptr) == SQLITE_OK && original == 1))
            {
                return false;
            }
            if (!TEST_CHECK(sqlite3_exec(database.get(),
                "DROP TABLE last_session_play_queue;"
                "CREATE TABLE last_session_play_queue(id INTEGER PRIMARY KEY, track_id INTEGER);",
                nullptr, nullptr, nullptr) == SQLITE_OK))
            {
                return false;
            }
        }
        {
            api::Context recovered(options);
            if (!TEST_CHECK(recovered.GetLibrary().has_value())) {
                return false;
            }
        }

        /* Restore a queue whose previous entry has been removed from the catalog. */
        {
            sqlite3* raw = nullptr;
            const auto opened = sqlite3_open(filename.string().c_str(), &raw);
            const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
            if (!TEST_CHECK(opened == SQLITE_OK && sqlite3_exec(database.get(),
                "INSERT INTO last_session_play_queue(track_id) VALUES (9999);",
                nullptr, nullptr, nullptr) == SQLITE_OK))
            {
                return false;
            }
        }
        api::Context restored(options);
        const auto queue = restored.GetPlayback().Queue();
        return TEST_CHECK(queue && queue->Size() == 0);
    }
}
