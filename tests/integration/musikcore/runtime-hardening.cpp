/* lifecycle regressions adapted from the earlier owned-runtime implementation.
public calls exercise thread and startup contracts; registry checks deliberately
inspect the private factory because its numeric identity is not a public API. */
#include <musikcore/musikcore.hpp>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>
#include <musikcore/io/DataStreamFactory.h>
#include <musikcore/audio/Streams.h>
#include "../../support/check.h"
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sqlite3.h>
#include "../../support/host-wakeup.h"
#include <string_view>
#include <algorithm>

namespace {
    using namespace musik::core;

    /* exercise every selection in one process: omitted prerequisites must be
    added, disabled services must stay absent, and teardown must permit a
    different selection next time without retaining scanner/player state. */
    bool checkServices() {
        for (mc_services requested = MC_SERVICE_NONE; requested <= MC_SERVICE_ALL; ++requested) {
            auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
            options.services = requested;
            api::Context context(options);
            const auto effective = requested & (MC_SERVICE_PLAYBACK | MC_SERVICE_INDEXER)
                ? requested | MC_SERVICE_LIBRARY | MC_SERVICE_PLUGINS
                : requested;
            if (!TEST_CHECK(context.Services() == effective)) {
                return false;
            }

            /* a library remains queryable/configurable without a scanner. */
            const bool library = (effective & MC_SERVICE_LIBRARY) != 0;
            if (!TEST_CHECK(context.GetLibrary().has_value() == library) ||
                !TEST_CHECK(context.SelectLibrary(MC_LIBRARY_LOCAL) == (library ? MC_OK : MC_ERROR_UNSUPPORTED)))
            {
                return false;
            }
            if (library && !TEST_CHECK(bool(LibraryFactory::Instance().DefaultLocalLibrary()->Indexer()) ==
                bool(effective & MC_SERVICE_INDEXER)))
            {
                return false;
            }

            /* unavailable service calls reject cleanly, including subscriptions. */
            mc_playback* playback = nullptr;
            const bool player = (effective & MC_SERVICE_PLAYBACK) != 0;
            mc_indexer_state state = MC_INDEXER_IDLE;
            const auto scanner = effective & MC_SERVICE_INDEXER ? MC_OK : MC_ERROR_UNSUPPORTED;
            if (!TEST_CHECK(mc_context_get_playback(context.Get(), &playback) == (player ? MC_OK : MC_ERROR_UNSUPPORTED)) ||
                !TEST_CHECK(mc_indexer_get_state(context.Get(), &state) == scanner) ||
                !TEST_CHECK(mc_indexer_set_callback(context.Get(), nullptr, nullptr) == scanner))
            {
                mc_playback_destroy(&playback);
                return false;
            }
            mc_playback_destroy(&playback);

            /* selections are fixed and safe to inspect from a non-owner thread;
            service operations remain restricted to the creating thread. */
            bool offThread = false;
            std::thread reader([&] {
                mc_services observed = MC_SERVICE_NONE;
                offThread = mc_context_get_services(context.Get(), &observed) == MC_OK && observed == effective;
                if (effective) {
                    offThread = offThread && mc_context_shutdown(context.Get()) == MC_ERROR_WRONG_THREAD;
                }
            });
            reader.join();
            if (!TEST_CHECK(offThread) || !TEST_CHECK(context.Dispatch() == MC_OK) ||
                !TEST_CHECK(context.Shutdown() == MC_OK) || !TEST_CHECK(context.Services() == effective) ||
                !TEST_CHECK(context.Close() == MC_OK))
            {
                return false;
            }
        }

        /* NULL and an older sized prefix select the full engine. poisoned tail
        bytes must never override defaults when outside the advertised size. */
        mc_context* context = nullptr;
        mc_services services = MC_SERVICE_NONE;
        if (!TEST_CHECK(mc_context_create(nullptr, &context, nullptr) == MC_OK) ||
            !TEST_CHECK(mc_context_get_services(context, &services) == MC_OK && services == MC_SERVICE_ALL) ||
            !TEST_CHECK(mc_context_destroy(&context) == MC_OK))
        {
            mc_context_destroy(&context);
            return false;
        }
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.struct_size = offsetof(mc_context_options, services);
        options.services = UINT32_MAX;
        api::Context prefix(options);
        if (!TEST_CHECK(prefix.Services() == MC_SERVICE_ALL) || !TEST_CHECK(prefix.Close() == MC_OK)) {
            return false;
        }

        /* invalid combinations must fail before reserving process services. */
        options.struct_size = sizeof(options);
        if (!TEST_CHECK(mc_context_create(&options, &context, nullptr) == MC_ERROR_UNSUPPORTED && !context)) {
            return false;
        }
        options.services = MC_SERVICE_PLUGINS;
        options.flags = MC_CONTEXT_START_LOCAL;
        if (!TEST_CHECK(mc_context_create(&options, &context, nullptr) == MC_ERROR_UNSUPPORTED && !context)) {
            return false;
        }
        options.services = MC_SERVICE_NONE;
        options.flags = MC_CONTEXT_ENABLE_LOGGING;
        if (!TEST_CHECK(mc_context_create(&options, &context, nullptr) == MC_ERROR_UNSUPPORTED && !context)) {
            return false;
        }
        options.flags = 0;
        api::Context recovered(options);
        return TEST_CHECK(recovered.Services() == MC_SERVICE_NONE);
    }

    /* rejecting a directory before first discovery must not pin the default
    directory as a side effect of rollback. a later valid custom root must work. */
    bool checkPluginDirectory() {
        const auto directory = std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "custom-plugins";
        const auto encoded = directory.u8string();
        const std::string path(reinterpret_cast<const char*>(encoded.data()), encoded.size());
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        options.plugin_directory = { path.data(), path.size() };
        mc_context* failed = nullptr;
        if (!TEST_CHECK(mc_context_create(&options, &failed, nullptr) == MC_ERROR_INVALID_ARGUMENT && !failed)) {
            return false;
        }
        std::filesystem::create_directories(directory);
        api::Context recovered(options);
        return TEST_CHECK(recovered.Services() == MC_SERVICE_PLUGINS) && TEST_CHECK(recovered.Close() == MC_OK);
    }

    /* use the same isolated catalog in separate processes so process-local
    migration flags cannot accidentally make a restart test pass. */
    std::string migrationFilename() {
        const auto encoded = (std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "1" / "musik.db").u8string();
        return std::string(reinterpret_cast<const char*>(encoded.data()), encoded.size());
    }

    /* inspect committed state through a private-cache WAL reader. joining the
    legacy shared cache could block the very completion write being tested. */
    int readMigrationValue(const char* sql) {
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
    bool editMigrationDatabase(const char* sql) {
        sqlite3* raw = nullptr;
        const auto opened = sqlite3_open_v2(migrationFilename().c_str(), &raw, SQLITE_OPEN_READWRITE, nullptr);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(raw, sqlite3_close);
        return TEST_CHECK(opened == SQLITE_OK) &&
            TEST_CHECK(sqlite3_exec(raw, sql, nullptr, nullptr, nullptr) == SQLITE_OK);
    }

    /* migrate without Indexer, then exit while the required scan remains pending.
    a current-schema fixture with an older version exercises the real v6->v10
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
        return TEST_CHECK(readMigrationValue("SELECT version FROM version") == 10) &&
            TEST_CHECK(readMigrationValue("SELECT count(*) FROM pending_migration_scan") == 1);
    }

    /* start only Indexer and let startup resume pending work, without an explicit
    scan request. a failed completion write must preserve the marker for another
    process; successful retry must clear it in the committed scan transaction. */
    bool resumeMigration(bool failCompletion) {
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
        else if (!editMigrationDatabase("DROP TRIGGER defer_migration_scan")) {
            return false;
        }

        const std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
            test_host_event_create(), test_host_event_destroy);
        if (!TEST_CHECK(bool(event))) {
            return false;
        }
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_INDEXER;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        api::Context context(options);
        const auto complete = [&] {
            return failCompletion
                ? readMigrationValue("SELECT count FROM migration_scan_attempts") > 0
                : readMigrationValue("SELECT count(*) FROM pending_migration_scan") == 0;
        };
        while (!complete()) {
            if (!TEST_CHECK(test_host_event_wait(event.get())) || !TEST_CHECK(context.Dispatch() == MC_OK)) {
                return false;
            }
        }
        return TEST_CHECK(context.Shutdown() == MC_OK) &&
            TEST_CHECK(readMigrationValue("SELECT count(*) FROM pending_migration_scan") == (failCompletion ? 1 : 0));
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

    /* public visualizer APIs work with Plugins alone. session shutdown must hide
    and release providers before a disabled replacement session rediscovers them. */
    bool checkVisualizerSession() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_PLUGINS;
        auto preferences = Preferences::ForPlugin("RuntimeVisualizer");
        api::Context context(options);
        size_t count = 0;
        const auto read = [](void* data, const mc_string_view*, size_t size) {
            *static_cast<size_t*>(data) = size;
        };
        if (!TEST_CHECK(mc_visualizer_list(read, &count) == MC_OK && count == 1) ||
            !TEST_CHECK(mc_visualizer_show(0) == MC_OK) ||
            !TEST_CHECK(context.Close() == MC_OK) ||
            !TEST_CHECK(preferences->GetInt("hides") == 1 && preferences->GetInt("releases") == 1) ||
            !TEST_CHECK(mc_visualizer_hide() == MC_ERROR_NOT_FOUND))
        {
            return false;
        }
        const auto plugins = api::ListPlugins();
        const auto plugin = std::find_if(plugins.begin(), plugins.end(), [](const auto& item) {
            return item.name == "RuntimeVisualizer";
        });
        if (!TEST_CHECK(plugin != plugins.end()) || !TEST_CHECK(api::SetPluginEnabled(plugin->filename, false) == MC_OK)) {
            return false;
        }
        api::Context reopened(options);
        count = 99;
        return TEST_CHECK(mc_visualizer_list(read, &count) == MC_OK && count == 0) &&
            TEST_CHECK(mc_visualizer_show(0) == MC_ERROR_NOT_FOUND);
    }

    /* a repeated name must keep its origin, while different catalogs need
    distinct persisted IDs even when the settings file starts empty. */
    bool checkRegistry() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        api::Context context(options);
        auto& factory = LibraryFactory::Instance();
        const auto local = factory.DefaultLocalLibrary();
        const auto remote = factory.DefaultRemoteLibrary();
        return TEST_CHECK(local->Id() != remote->Id()) &&
            TEST_CHECK(factory.CreateLibrary(local->Name(), local->GetType()) == local) &&
            TEST_CHECK(factory.CreateLibrary(remote->Name(), remote->GetType()) == remote);
    }

    /* successful startup must finish restoring preferences before a caller
    changes playback; dispatch must not silently overwrite that new command. */
    bool checkStartup() {
        const auto settings = Preferences::ForComponent(prefs::components::Settings);
        const auto remote = static_cast<int>(ILibrary::Type::Remote);
        settings->SetInt(prefs::keys::LibraryType, remote);

        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        options.flags = MC_CONTEXT_START_LOCAL;
        api::Context context(options);
        if (!TEST_CHECK(context.GetLibraryStatus().kind == MC_LIBRARY_LOCAL) ||
            !TEST_CHECK(settings->GetInt(prefs::keys::LibraryType, -1) == remote))
        {
            return false;
        }
        auto playback = context.GetPlayback();
        if (!TEST_CHECK(playback.Control(MC_PLAYBACK_SET_VOLUME, 0.375) == MC_OK) ||
            !TEST_CHECK(context.Dispatch() == MC_OK))
        {
            return false;
        }
        const auto state = playback.Snapshot();
        return TEST_CHECK(state && state->volume == 0.375);
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
            bool committed = false;
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
            state.committed = status == MC_OK &&
                api::AsStringView(mc_track_page_get_string(page, 0, 0)) == "committed";
            state.done = true;
            mc_track_page_destroy(&page);
        };
        auto query = mc_track_query MC_TRACK_QUERY_DEFAULT;
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
        return TEST_CHECK(state.committed);
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
        api::Context recovered(options);
        return TEST_CHECK(recovered.GetLibrary().has_value());
    }

    /* wrong-thread calls must leave the context and playback usable by their
    owner; distinguish this contract violation from transient reentrancy. */
    bool checkOwner() {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        api::Context context(options);
        auto playback = context.GetPlayback();
        bool correct = false;
        std::thread other([&] {
            mc_context* retained = context.Get();
            mc_playback* unexpected = nullptr;
            correct = mc_context_dispatch(context.Get()) == MC_ERROR_WRONG_THREAD &&
                mc_context_shutdown(context.Get()) == MC_ERROR_WRONG_THREAD &&
                mc_context_destroy(&retained) == MC_ERROR_WRONG_THREAD && retained == context.Get() &&
                mc_context_get_playback(context.Get(), &unexpected) == MC_ERROR_WRONG_THREAD && !unexpected &&
                playback.Control(MC_PLAYBACK_STOP) == MC_ERROR_WRONG_THREAD &&
                context.SelectLibrary(MC_LIBRARY_REMOTE) == MC_ERROR_WRONG_THREAD;
        });
        other.join();
        return TEST_CHECK(correct) && TEST_CHECK(context.Dispatch() == MC_OK) &&
            TEST_CHECK(playback.Control(MC_PLAYBACK_STOP) == MC_OK);
    }

    /* every provider must lose all host pointers despite a throwing peer.
    shutdown reports failure, drains requests, and leaves destruction retryable. */
    bool checkFailure(bool caches = false) {
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_ALL;
        auto first = Preferences::ForPlugin("RuntimeFailureFirst");
        auto second = Preferences::ForPlugin("RuntimeFailureSecond");
        {
            unsigned completions = 0;
            mc_result completed = MC_ERROR_BUSY;
            api::Context context(options);
            auto playback = context.GetPlayback();
            if (caches) {
                /* exercise concurrent first lookup; no fixture accepts the URI. */
                const auto path = std::filesystem::u8path(api::ReadPath(MC_PATH_DATA)) / "uncreated-runtime-stream";
                const auto text = path.u8string();
                const std::string uri(reinterpret_cast<const char*>(text.data()), text.size());
                std::vector<std::thread> readers;
                for (unsigned reader = 0; reader < 4; ++reader) {
                    readers.emplace_back([&] {
                        const auto stream = io::DataStreamFactory::OpenSharedDataStream(uri.c_str(), sdk::OpenFlags::Read);
                    });
                }
                for (auto& reader : readers) {
                    reader.join();
                }
                audio::streams::GetEncoderForType("runtime-fixture-unsupported-codec");
            }
            api::OpenTrackSearch(context, {}, [&](mc_result status, api::TrackResult, std::string_view) {
                ++completions;
                completed = status;
            });
            const int expectedMask = caches ? 127 : 15;
            if (!TEST_CHECK(context.Shutdown() == MC_ERROR_INTERNAL) ||
                !TEST_CHECK((first->GetInt("cleanup_mask") & expectedMask) == expectedMask) ||
                !TEST_CHECK((second->GetInt("cleanup_mask") & expectedMask) == expectedMask) ||
                !TEST_CHECK(completions == 1 && (completed == MC_OK || completed == MC_ERROR_CANCELLED)) ||
                !TEST_CHECK(!playback.Snapshot()) ||
                !TEST_CHECK(context.Close() == MC_OK))
            {
                return false;
            }
        }
        if (caches) {
            for (const auto& preferences : { first, second }) {
                for (const std::string kind : { "stream", "decoder", "encoder" }) {
                    if (!TEST_CHECK(preferences->GetInt(kind + "_acquisitions") > 0) ||
                        !TEST_CHECK(preferences->GetInt(kind + "_acquisitions") ==
                            preferences->GetInt(kind + "_releases")))
                    {
                        return false;
                    }
                }
            }
        }
        /* a failed startup must release the process slot even if rollback hooks fail. */
        first->SetBool("fail_startup", true);
        mc_context* failed = nullptr;
        const auto result = mc_context_create(&options, &failed, nullptr);
        first->SetBool("fail_startup", false);
        if (!TEST_CHECK(result == MC_ERROR_INTERNAL && !failed)) {
            return false;
        }
        api::Context reopened(options);
        return TEST_CHECK(reopened.Shutdown() == MC_ERROR_INTERNAL) &&
            TEST_CHECK(reopened.Close() == MC_OK);
    }
}

/* each case runs in its own process and isolated configuration root. */
int main(int argc, char** argv) {
    if (!TEST_CHECK(argc == 2)) {
        return 1;
    }
    const std::string_view mode(argv[1]);
    if (mode == "migration_prepare") {
        return prepareMigration() ? 0 : 1;
    }
    if (mode == "migration_fail_completion") {
        return resumeMigration(true) ? 0 : 1;
    }
    if (mode == "migration_resume") {
        return resumeMigration(false) ? 0 : 1;
    }
    if (mode == "migration_verify") {
        return verifyMigrationComplete() ? 0 : 1;
    }
    if (mode == "visualizer_session") {
        return checkVisualizerSession() ? 0 : 1;
    }
    if (mode == "plugin_directory") {
        return checkPluginDirectory() ? 0 : 1;
    }
    if (mode == "services") {
        return checkServices() ? 0 : 1;
    }
    if (mode == "registry") {
        return checkRegistry() ? 0 : 1;
    }
    if (mode == "startup") {
        return checkStartup() ? 0 : 1;
    }
    if (mode == "read_isolation") {
        return checkReadIsolation() ? 0 : 1;
    }
    if (mode == "locked_read_shutdown") {
        return checkLockedReadShutdown() ? 0 : 1;
    }
    if (mode == "database_failure") {
        return checkDatabaseFailure() ? 0 : 1;
    }
    if (mode == "owner") {
        return checkOwner() ? 0 : 1;
    }
    if (mode == "caches") {
        return checkFailure(true) ? 0 : 1;
    }
    if (mode == "failure") {
        return checkFailure() ? 0 : 1;
    }
    return 1;
}
