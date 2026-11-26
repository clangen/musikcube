#include <musikcore/musikcore.hpp>
#include <common/Filesystem.h>
#include <common/FilesystemPath.h>
#include <common/SubtrackFile.h>
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
            /* tracker titles may use legacy single-byte character sets. */
            bytes[0] = 'A';
            bytes[1] = static_cast<char>(0xf1);
            bytes[950] = 1;
            std::memcpy(bytes.data() + 1080, "M.K.", 4);
        }
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), bytes.size());
        require(bool(out), "write fixture");
    }

    /* NSFE files can contain legacy single-byte text in track labels. */
    void createLegacyNsfe(const std::filesystem::path& path) {
        std::vector<char> bytes { 'N', 'S', 'F', 'E' };
        const auto chunk = [&](const char* name, const std::vector<char>& value) {
            const auto length = static_cast<uint32_t>(value.size());
            for (unsigned shift = 0; shift < 32; shift += 8) {
                bytes.push_back(static_cast<char>((length >> shift) & 0xff));
            }
            bytes.insert(bytes.end(), name, name + 4);
            bytes.insert(bytes.end(), value.begin(), value.end());
        };
        chunk("INFO", { 0, static_cast<char>(0x80), 0, static_cast<char>(0x80),
            0, static_cast<char>(0x80), 0, 0, 3, 0 });
        chunk("DATA", { 0x60 });
        chunk("tlbl", { 'B', 'a', 'd', ' ', static_cast<char>(0xf1), 0, 'O', 'k', 0,
            'C', 'a', 'f', static_cast<char>(0xc3), static_cast<char>(0xa9), 0 });
        chunk("NEND", {});
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), bytes.size());
        require(bool(out), "write legacy NSFE fixture");
    }

    /* the first pattern loops before the second, producing two subsongs. */
    void createMultisongMod(const std::filesystem::path& path) {
        std::vector<char> bytes(1084 + 2 * 64 * 4 * 4);
        bytes[950] = 2;
        bytes[953] = 1;
        bytes[1086] = 0x0b;
        std::memcpy(bytes.data() + 1080, "M.K.", 4);
        std::ofstream out(path, std::ios::binary);
        out.write(bytes.data(), bytes.size());
        require(bool(out), "write multisong MOD fixture");
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

    std::vector<int64_t> trackIds(Context& context, test_host_event& event,
        std::string* artist = nullptr, std::vector<std::string>* titles = nullptr)
    {
        mc_result status = MC_ERROR_BUSY;
        std::vector<int64_t> ids;
        TrackResult result;
        OpenTrackSearch(context, TrackSearch {}, [&](mc_result resultStatus, TrackResult tracks, std::string_view) {
            status = resultStatus;
            for (size_t row = 0; row < tracks.Size(); ++row) {
                ids.push_back(tracks.Id(row));
            }
            if (artist || titles) {
                result = std::move(tracks);
            }
        });
        wait(context, event, [&] { return status != MC_ERROR_BUSY; });
        require(status == MC_OK, "query catalog");
        if ((artist || titles) && !ids.empty()) {
            status = MC_ERROR_BUSY;
            TrackRange range;
            range.limit = 100;
            range.fields = { artist ? MC_TRACK_FIELD_ARTIST : MC_TRACK_FIELD_TITLE };
            ReadTracks(context, result, range, [&](mc_result resultStatus, TrackPage page, std::string_view) {
                status = resultStatus;
                if (resultStatus == MC_OK && page.Size()) {
                    if (artist) {
                        *artist = page.Value(0, 0);
                    }
                    if (titles) {
                        for (size_t row = 0; row < page.Size(); ++row) {
                            titles->emplace_back(page.Value(row, 0));
                        }
                    }
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
    using musik::core::sdk::IsWithinDirectory;
    require(IsWithinDirectory("/music/chip/song.nsf", "/music/chip") &&
        IsWithinDirectory("/music/chip/song.nsf", "/music/chip/") &&
        !IsWithinDirectory("/music/chiptunes/song.nsf", "/music/chip") &&
        !IsWithinDirectory("/music/chip/song.nsf", ""), "match whole directory components");
    const bool gme = std::string(argv[2]) == "gme";
    const auto root = musik::core::sdk::fs::pathFromUtf8(argv[1]);
    const auto file = root / (gme ? "fixture.nsf" : "fixture.mod");
    createFile(file, gme);
    const auto legacyFile = root / "bad-metadata.nsfe";
    const auto laterFile = root / (gme ? "later.nsf" : "later.mod");
    const auto brokenFile = root / (gme ? "broken.nsfe" : "broken.mod");
    const auto multiFile = root / "multi.mod";
    const auto interruptedFile = root / (gme ? "later.nsf" : "multi2.mod");
    const auto unicodeFile = root / musik::core::sdk::fs::pathFromUtf8(
        gme ? "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e.nsf" : "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e.mod");
    auto playlistFile = unicodeFile;
    playlistFile.replace_extension(".m3u");
    createFile(laterFile, gme);
    createFile(unicodeFile, gme);
    {
        std::ofstream broken(brokenFile, std::ios::binary);
        broken << "invalid music file";
        require(bool(broken), "write corrupt music fixture");
    }
    if (gme) {
        createLegacyNsfe(legacyFile);
        std::ofstream playlist(playlistFile, std::ios::binary);
        playlist << "fixture.nsf,$00,Playlist first,0:00:05\n"
                    "fixture.nsf,$01,Playlist second,0:00:05\n";
        require(bool(playlist), "write Unicode-path playlist fixture");
    }
    else {
        createMultisongMod(multiFile);
        createMultisongMod(interruptedFile);
    }
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
        if (gme) {
            mc_preference_entry setting {};
            setting.key = { "enable_m3u_support", 18 };
            setting.value.type = MC_PREFERENCE_BOOLEAN;
            setting.value.data.boolean = true;
            require(mc_plugin_write_setting({ "org.musikcube.decoder.gme", 25 }, &setting) == MC_OK,
                "enable M3U playlists");
        }
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
        std::vector<std::string> titles;
        const auto ids = trackIds(context, *event, gme ? nullptr : &artist, gme ? &titles : nullptr);
        require(ids.size() == (gme ? 9u : 7u), "index valid files after corrupt input, including Unicode filenames");
        if (gme) {
            require(std::find(titles.begin(), titles.end(), "Bad ?") != titles.end(),
                "replace invalid metadata bytes and retain the track");
            require(std::find(titles.begin(), titles.end(), "Caf\xc3\xa9") != titles.end(),
                "preserve valid UTF-8 metadata");
            require(std::find(titles.begin(), titles.end(), "Playlist first") != titles.end(),
                "read a playlist with a Unicode filename");
        }
        if (!gme) {
            require(artist == "literal %n", "keep literal percent sequences in text settings");
        }
        {
            sqlite3* raw = nullptr;
            sqlite3_stmt* statement = nullptr;
            require(sqlite3_open_v2(database.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_PRIVATECACHE, nullptr) ==
                SQLITE_OK, "open catalog for indexing checks");
            require(sqlite3_prepare_v2(raw,
                "SELECT count(*) FROM tracks t JOIN directories d ON d.id=t.directory_id WHERE t.source_id<>0",
                -1, &statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
                sqlite3_column_int64(statement, 0) == static_cast<sqlite3_int64>(ids.size()),
                "assign a directory to every track, including cached directories");
            sqlite3_finalize(statement);
            require(sqlite3_prepare_v2(raw,
                "SELECT count(*) FROM sqlite_master WHERE type='index' AND name IN ("
                "'artist_name_index','genre_name_index','directory_name_index',"
                "'metavalues_key_content_index','tracks_album_index')",
                -1, &statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
                sqlite3_column_int(statement, 0) == 5, "install indexing lookup indexes");
            sqlite3_finalize(statement);
            require(sqlite3_prepare_v2(raw,
                "EXPLAIN QUERY PLAN SELECT id FROM meta_values WHERE meta_key_id=1 AND content='missing'",
                -1, &statement, nullptr) == SQLITE_OK && sqlite3_step(statement) == SQLITE_ROW &&
                std::strstr(reinterpret_cast<const char*>(sqlite3_column_text(statement, 3)),
                    "metavalues_key_content_index") != nullptr,
                "use the key and content index for metadata lookups");
            sqlite3_finalize(statement);
            require(sqlite3_close(raw) == SQLITE_OK, "close indexing checks");
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
            /* rewriting a two-track container as one must remove only the
            obsolete subtrack while keeping all surviving track IDs. */
            const auto& changedFile = gme ? file : multiFile;
            std::fstream changed(changedFile, std::ios::in | std::ios::out | std::ios::binary);
            changed.seekp(gme ? 6 : 1086);
            changed.put(gme ? 1 : 0);
            changed.close();
            require(bool(changed), "rewrite container track count");
            std::filesystem::last_write_time(changedFile,
                std::filesystem::last_write_time(changedFile) + std::chrono::seconds(2));

            /* simulate a previous scan that checkpointed the surviving
            subtrack before it could validate and remove the old one. */
            std::fstream interrupted(interruptedFile, std::ios::in | std::ios::out | std::ios::binary);
            interrupted.seekp(gme ? 6 : 1086);
            interrupted.put(gme ? 1 : 0);
            interrupted.close();
            require(bool(interrupted), "rewrite interrupted container");
            std::filesystem::last_write_time(interruptedFile,
                std::filesystem::last_write_time(interruptedFile) + std::chrono::seconds(2));
            const auto encodedInterrupted = interruptedFile.u8string();
            const std::string interruptedPath(encodedInterrupted.begin(), encodedInterrupted.end());
            const auto modified = musik::core::sdk::fs::getLastModifiedTime(interruptedPath);
            require(sqlite3_open_v2(database.c_str(), &raw, SQLITE_OPEN_READWRITE | SQLITE_OPEN_PRIVATECACHE, nullptr) ==
                SQLITE_OK, "open catalog for interrupted scan fixture");
            require(sqlite3_prepare_v2(raw,
                "UPDATE tracks SET filetime=? WHERE source_id<>0 AND filename LIKE ? AND track=1",
                -1, &statement, nullptr) == SQLITE_OK, "prepare interrupted scan fixture");
            const char* pattern = gme ? "%later.nsf" : "%multi2.mod";
            sqlite3_bind_int64(statement, 1, modified);
            sqlite3_bind_text(statement, 2, pattern, -1, SQLITE_STATIC);
            require(sqlite3_step(statement) == SQLITE_DONE && sqlite3_changes(raw) == 1,
                "checkpoint surviving subtrack");
            sqlite3_finalize(statement);
            require(sqlite3_close(raw) == SQLITE_OK, "close interrupted scan fixture");

            scan.finished = false;
            require(indexer.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK, "schedule changed container scan");
            wait(context, *event, [&] { return scan.finished; });
            const auto updated = trackIds(context, *event);
            require(updated.size() + 2 == ids.size(), "remove obsolete and interrupted container subtracks");
            require(std::includes(ids.begin(), ids.end(), updated.begin(), updated.end()),
                "retain the IDs of surviving subtracks");
            std::filesystem::remove(file);
            std::filesystem::remove(laterFile);
            std::filesystem::remove(brokenFile);
            std::filesystem::remove(unicodeFile);
            if (gme) {
                std::filesystem::remove(legacyFile);
                std::filesystem::remove(playlistFile);
            }
            else {
                std::filesystem::remove(multiFile);
                std::filesystem::remove(interruptedFile);
            }
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
                "DROP INDEX artist_name_index; DROP INDEX genre_name_index; DROP INDEX directory_name_index; "
                "DROP INDEX metavalues_key_content_index; DROP INDEX tracks_album_index; "
                "UPDATE tracks SET source_id=19088743 WHERE source_id<>0; "
                "INSERT INTO playlist_tracks(playlist_id, track_external_id, source_id) "
                "SELECT 999, external_id, source_id FROM tracks LIMIT 1; UPDATE version SET version=10",
                nullptr, nullptr, nullptr) == SQLITE_OK, "prepare old source identities");
            require(sqlite3_close(raw) == SQLITE_OK, "close migration fixture");
        }
    }
}
