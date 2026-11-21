#include <app/util/AsyncCallbackScope.h>
#include <musikcore/musikcore.hpp>
#include <sqlite3.h>

#include <cstdio>

extern "C" void MC_CALL query_test_wakeup(void*);
extern "C" int query_test_wait_for_wakeup(unsigned count);
extern "C" void query_test_release_wakeup();

namespace {
    using namespace musik::core::api;

    /* Create an isolated catalog with a known title and the same track ID as the other catalog. */
    bool seed(const std::string& directory, const char* title) {
        sqlite3* raw = nullptr;
        const auto status = sqlite3_open((directory + "/musik.db").c_str(), &raw);
        const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(raw, sqlite3_close);
        if (status != SQLITE_OK ||
            sqlite3_exec(db.get(),
                "CREATE TABLE IF NOT EXISTS tracks (id INTEGER PRIMARY KEY, title TEXT, visible INTEGER,"
                " duration INTEGER, album_id INTEGER, visual_artist_id INTEGER, visual_genre_id INTEGER,"
                " disc INTEGER, track INTEGER);"
                "CREATE TABLE IF NOT EXISTS albums (id INTEGER PRIMARY KEY, name TEXT);"
                "CREATE TABLE IF NOT EXISTS artists (id INTEGER PRIMARY KEY, name TEXT);"
                "CREATE TABLE IF NOT EXISTS genres (id INTEGER PRIMARY KEY, name TEXT);"
                "INSERT OR IGNORE INTO albums VALUES (1, 'Album');"
                "INSERT OR IGNORE INTO artists VALUES (1, 'Artist');"
                "INSERT OR IGNORE INTO genres VALUES (1, 'Genre');",
                nullptr, nullptr, nullptr) != SQLITE_OK) {
            return false;
        }

        sqlite3_stmt* statement = nullptr;
        const auto prepared = sqlite3_prepare_v2(db.get(),
            "INSERT OR REPLACE INTO tracks VALUES (7, ?1, 1, 1, 1, 1, 1, 1, 1)", -1, &statement, nullptr);
        const std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> insert(statement, sqlite3_finalize);
        return prepared == SQLITE_OK && sqlite3_bind_text(insert.get(), 1, title, -1, SQLITE_STATIC) == SQLITE_OK &&
            sqlite3_step(insert.get()) == SQLITE_DONE;
    }

    /* Use matching IDs in two catalogs to expose stale rendering or accidental retargeting that distinct
       IDs would hide. */
    bool checkSwitch(const char* origin, const char* destination) {
        constexpr auto originTitle = "Origin";
        constexpr auto destinationTitle = "Destination";
        if (!seed(origin, originTitle) || !seed(destination, destinationTitle)) {
            return false;
        }

        auto generation = std::make_unique<musik::cube::AsyncCallbackScope>();
        TrackResult initial;
        TrackRef reference;
        TrackResult visible;
        TrackPage resolved;
        unsigned stale = 0;
        unsigned rendered = 0;
        unsigned wakes = 0;

        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.services = MC_SERVICE_NONE;
        options.wakeup = query_test_wakeup;
        Context context(options);
        if (context.GetLibrary()) {
            return false;
        }
        context.SetLibrary(Library(origin));
        const auto pinned = context.GetLibrary();
        if (!pinned) {
            return false;
        }
        query_test_release_wakeup();

        OpenTrackSearch(context, TrackSearch { originTitle },
            [&](mc_result status, TrackResult result, std::string_view) {
                if (status == MC_OK) {
                    initial = std::move(result);
                }
            });
        if (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK || initial.Size() != 1) {
            return false;
        }
        reference = initial.Track(0);

        const auto search = [&](std::string_view title) {
            const auto token = generation->CurrentToken();
            return OpenTrackSearch(context, TrackSearch { title },
                [&, token](mc_result status, TrackResult result, std::string_view) {
                    if (!token.IsCurrent()) {
                        if (status == MC_OK) {
                            ++stale;
                        }
                        return;
                    }
                    ++rendered;
                    visible = std::move(result);
                });
        };

        /* Hold delivery until the old query is ready and cancellation can no longer win. */
        const auto old = search(originTitle);
        if (!query_test_wait_for_wakeup(++wakes)) {
            return false;
        }
        generation->Invalidate();
        if (context.Cancel(old) != MC_ERROR_NOT_FOUND) {
            return false;
        }
        context.SetLibrary(Library(destination));
        if (context.Dispatch() != MC_OK || stale != 1 || rendered != 0 || visible) {
            return false;
        }

        search(destinationTitle);
        if (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK ||
            stale != 1 || rendered != 1 || visible.Size() != 1 || visible.Id(0) != reference.Id()) {
            return false;
        }

        const TrackRange range { 0, 1, { MC_TRACK_FIELD_TITLE } };
        const auto receivePage = [&](mc_result status, TrackPage page, std::string_view) {
            if (status == MC_OK) {
                resolved = std::move(page);
            }
        };
        ReadTracks(context, visible, range, receivePage);
        if (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK ||
            resolved.Size() != 1 || resolved.Track(0).Title() != destinationTitle) {
            return false;
        }

        /* Both libraries contain the same track ID. Resolve the reference after releasing the old handle. */
        resolved = {};
        ReadTrack(context, reference, { MC_TRACK_FIELD_TITLE }, receivePage);
        if (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK ||
            resolved.Size() != 1 || resolved.Track(0).Title() != originTitle) {
            return false;
        }

        resolved = {};
        ReadTracks(context, initial, range, receivePage);
        initial = {};
        if (!query_test_wait_for_wakeup(++wakes) || context.Dispatch() != MC_OK ||
            resolved.Size() != 1 || resolved.Track(0).Title() != originTitle) {
            return false;
        }

        /* A ready callback also loses permission to update a destroyed view. */
        context.ClearLibrary();
        if (context.GetLibrary()) {
            return false;
        }
        context.SetLibrary(Library(destination));
        search(destinationTitle);
        if (!query_test_wait_for_wakeup(++wakes)) {
            return false;
        }
        generation.reset();
        return context.Shutdown() == MC_OK && stale == 2 && rendered == 1;
    }
}

/* Run the two-catalog switch scenario using independently supplied fixture directories. */
int main(int argc, char** argv) {
    try {
        if (argc == 3 && checkSwitch(argv[1], argv[2])) {
            return 0;
        }
        std::fputs("Library switch contract failed\n", stderr);
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "Library switch failed: %s\n", error.what());
    }
    return 1;
}
