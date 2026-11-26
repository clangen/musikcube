#include <musikcore/musikcore.hpp>
#include <common/FilesystemPath.h>
#include <sqlite3.h>
#include "../../support/host-wakeup.h"

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {
    using namespace musik::core::api;
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    void require(bool ok, const char* message) {
        if (!ok) {
            std::fprintf(stderr, "index provider: %s\n", message);
            std::abort();
        }
    }

    /* a tiny two-track NSF and an empty ProTracker module are valid inputs to
    the real libraries, with no machine-specific media files. */
    void createFile(const std::filesystem::path& path, bool gme) {
        std::vector<char> bytes(gme ? 129 : 1084 + 64 * 4 * 4);
        if (gme) {
            std::memcpy(bytes.data(), "NESM\x1a", 5);
            bytes[5] = 1;
            bytes[6] = 2;
            bytes[7] = 1;
            for (size_t highByte : { 9, 11, 13 }) {
                bytes[highByte] = static_cast<char>(0x80);
            }
            bytes[110] = 0x1a;
            bytes[111] = 0x41;
            bytes.back() = 0x60;
        }
        else {
            bytes[950] = 1;
            std::memcpy(bytes.data() + 1080, "M.K.", 4);
        }
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), bytes.size());
        require(bool(out), "write fixture");
    }

    template<class Ready>
    void wait(Context& context, test_host_event& event, Ready ready) {
        const auto deadline = Clock::now() + 20s;
        while (!ready()) {
            require(context.Dispatch() == MC_OK, "dispatch");
            if (!ready()) {
                const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
                require(left > 0ms && test_host_event_wait_for(&event, static_cast<unsigned>(left.count())),
                    "operation completed before deadline");
            }
        }
    }

    struct Scan {
        bool finished = false;
        static void MC_CALL Event(void* user, mc_indexer_event event, int) {
            if (event == MC_INDEXER_FINISHED) {
                static_cast<Scan*>(user)->finished = true;
            }
        }
    };

    std::vector<int64_t> trackIds(Context& context, test_host_event& event, std::string* artist = nullptr) {
        mc_result status = MC_ERROR_BUSY;
        std::vector<int64_t> ids;
        TrackResult result;
        OpenTrackSearch(context, TrackSearch {}, [&](mc_result resultStatus, TrackResult tracks, std::string_view) {
            status = resultStatus;
            for (size_t row = 0; row < tracks.Size(); ++row) {
                ids.push_back(tracks.Id(row));
            }
            if (artist) {
                result = std::move(tracks);
            }
        });
        wait(context, event, [&] { return status != MC_ERROR_BUSY; });
        require(status == MC_OK, "query catalog");
        if (artist && !ids.empty()) {
            status = MC_ERROR_BUSY;
            TrackRange range;
            range.limit = 1;
            range.fields = { MC_TRACK_FIELD_ARTIST };
            ReadTracks(context, result, range, [&](mc_result resultStatus, TrackPage page, std::string_view) {
                status = resultStatus;
                if (resultStatus == MC_OK && page.Size()) {
                    *artist = page.Value(0, 0);
                }
            });
            wait(context, event, [&] { return status != MC_ERROR_BUSY; });
            require(status == MC_OK, "read source metadata");
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }
}

/* index actual source files through the production library, then remove them
and verify source validation removes their previously indexed subtracks. */
int main(int argc, char** argv) {
    require(argc == 3, "expected fixture directory and provider family");
    const bool gme = std::string(argv[2]) == "gme";
    const auto root = musik::core::sdk::fs::pathFromUtf8(argv[1]);
    const auto file = root / (gme ? "fixture.nsf" : "fixture.mod");
    createFile(file, gme);
    const auto encoded = root.u8string();
    const std::string path(encoded.begin(), encoded.end());
    std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
        test_host_event_create(), test_host_event_destroy);
    require(bool(event), "create wakeup");
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_INDEXER;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = event.get();
    std::vector<int64_t> originalIds;
    std::string database;
    for (int run = 0; run < 2; ++run) {
        Context context(options);
        if (!run) {
            const auto encodedDatabase = (musik::core::sdk::fs::pathFromUtf8(ReadPath(MC_PATH_DATA)) /
                "1" / "musik.db").u8string();
            database.assign(encodedDatabase.begin(), encodedDatabase.end());
        }
        Indexer indexer(context);
        struct Labels {
            bool done = false;
            bool files = false;
            bool provider = false;
            std::string expected;
        } labels {
            .expected = gme ? "Game music (GME)" : "Tracker modules (OpenMPT)",
        };
        auto library = context.GetLibrary();
        require(library.has_value(), "get source catalog");
        library->ListSources(
            context,
            [](mc_result result, const mc_library_source* sources, size_t count, const mc_error*, void* data) {
                auto& labels = *static_cast<Labels*>(data);
                require(result == MC_OK, "list source labels");
                for (size_t i = 0; i < count; ++i) {
                    const std::string_view label(sources[i].label.data, sources[i].label.length);
                    labels.files |= sources[i].id == 0 && label == "Music files";
                    labels.provider |= label == labels.expected;
                }
                labels.done = true;
            },
            &labels);
        wait(context, *event, [&] {
            return labels.done;
        });
        require(labels.files && labels.provider, "show source names independently of decoder names");
        if (!gme) {
            /* a user-supplied default is text, not a printf format string. */
            mc_preference_entry setting {};
            setting.key = { "default_artist_name", 19 };
            setting.value.type = MC_PREFERENCE_TEXT;
            setting.value.data.text = { "literal %n", 10 };
            require(mc_plugin_write_setting({ "org.musikcube.decoder.openmpt", 29 }, &setting) == MC_OK,
                "save source text setting");
        }
        Scan scan;
        require(context.SelectLibrary(MC_LIBRARY_LOCAL) == MC_OK &&
            indexer.SetCallback(Scan::Event, &scan) == MC_OK && indexer.AddPath(path) == MC_OK,
            "configure library scan");
        require(indexer.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK, "schedule scan");
        wait(context, *event, [&] { return scan.finished; });
        std::string artist;
        const auto ids = trackIds(context, *event, gme ? nullptr : &artist);
        require(ids.size() == (gme ? 2u : 1u), "index subtracks through C source");
        if (!gme) {
            require(artist == "literal %n", "keep literal percent sequences in text settings");
        }
        if (!run) {
            originalIds = ids;
        }
        else {
            require(ids == originalIds, "keep track IDs after a runtime restart");
            sqlite3* raw = nullptr;
            sqlite3_stmt* statement = nullptr;
            require(sqlite3_open_v2(database.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE, nullptr) ==
                SQLITE_OK, "open catalog to verify playlist identity");
            require(sqlite3_prepare_v2(raw,
                "SELECT count(*) FROM playlist_tracks p JOIN tracks t "
                "ON p.track_external_id=t.external_id AND p.source_id=t.source_id", -1, &statement, nullptr) == SQLITE_OK &&
                sqlite3_step(statement) == SQLITE_ROW && sqlite3_column_int(statement, 0) == 1,
                "preserve playlist source identity");
            sqlite3_finalize(statement);
            sqlite3_close(raw);
            std::filesystem::remove(file);
            scan.finished = false;
            require(indexer.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK, "schedule validation");
            wait(context, *event, [&] { return scan.finished; });
            require(trackIds(context, *event).empty(), "remove stale source tracks");
        }
        require(context.Close() == MC_OK, "shut down");
        if (!run) {
            /* model an older catalog whose source ID came from a different hash. */
            sqlite3* raw = nullptr;
            require(sqlite3_open(database.c_str(), &raw) == SQLITE_OK, "open catalog for migration fixture");
            require(sqlite3_exec(raw,
                "UPDATE tracks SET source_id=19088743 WHERE source_id<>0; "
                "INSERT INTO playlist_tracks(playlist_id, track_external_id, source_id) "
                "SELECT 999, external_id, source_id FROM tracks LIMIT 1; UPDATE version SET version=10",
                nullptr, nullptr, nullptr) == SQLITE_OK, "prepare old source identities");
            require(sqlite3_close(raw) == SQLITE_OK, "close migration fixture");
        }
    }
}
