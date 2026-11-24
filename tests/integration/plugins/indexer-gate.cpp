#include <musikcore/musikcore.hpp>
#include <common/FilesystemPath.h>
#include "../../support/host-wakeup.h"
#include <sqlite3.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

namespace {
    using namespace musik::core::api;
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    void require(bool ok, const char* message) {
        if (!ok) {
            std::fprintf(stderr, "index gate: %s\n", message);
            std::abort();
        }
    }

    struct Events {
        bool waiting = false;
        bool rollback = false;
        bool terminal = false;

        static void MC_CALL Log(void* data, mc_log_level, mc_string_view tag, mc_string_view message) {
            auto& events = *static_cast<Events*>(data);
            if (std::string_view(tag.data, tag.length) == "index-gate") {
                const std::string_view text(message.data, message.length);
                events.waiting |= text == "waiting";
                events.rollback |= text == "rolled back";
            }
        }

        static void MC_CALL Index(void* data, mc_indexer_event event, int) {
            if (event == MC_INDEXER_CANCELLED || event == MC_INDEXER_FINISHED) {
                static_cast<Events*>(data)->terminal = true;
            }
        }
    };

    template<class Ready>
    void wait(Context& context, test_host_event& event, Ready ready) {
        const auto deadline = Clock::now() + 15s;
        while (!ready()) {
            require(context.Dispatch() == MC_OK, "dispatch");
            if (!ready()) {
                const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());
                require(left > 0ms && test_host_event_wait_for(&event, static_cast<unsigned>(left.count())),
                    "operation completed before deadline");
            }
        }
    }
}

/* only checkpointed tracks survive a cancelled or failed C-provider scan. */
int main(int argc, char** argv) {
    require(argc == 2, "expected indexer fixture mode");
    const bool createFailure = std::string_view(argv[1]) == "create-fail";
#ifdef WIN32
    _putenv_s("MUSIKCORE_TEST_INDEX_MODE", createFailure ? "fail" : argv[1]);
#else
    setenv("MUSIKCORE_TEST_INDEX_MODE", createFailure ? "fail" : argv[1], 1);
#endif
    const bool cancel = std::string_view(argv[1]) == "cancel";
    std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)> event(
        test_host_event_create(), test_host_event_destroy);
    require(bool(event), "create wakeup");
    Events events;
    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_INDEXER;
    options.flags = MC_CONTEXT_ENABLE_LOGGING | MC_CONTEXT_START_LOCAL;
    options.wakeup = test_host_event_signal;
    options.wakeup_user_data = event.get();
    options.log = Events::Log;
    options.log_user_data = &events;
    Context context(options);
    if (std::string_view(argv[1]) == "database" || std::string_view(argv[1]) == "ignore") {
        /* reject the second track after the first has been checkpointed. */
        const auto path = (musik::core::sdk::fs::pathFromUtf8(ReadPath(MC_PATH_DATA)) / "1" / "musik.db").u8string();
        const std::string database(path.begin(), path.end());
        sqlite3* raw = nullptr;
        require(sqlite3_open(database.c_str(), &raw) == SQLITE_OK, "open fixture catalog");
        require(sqlite3_exec(raw,
            "CREATE TRIGGER reject_pending BEFORE INSERT ON tracks WHEN NEW.filename='pending' "
            "BEGIN SELECT RAISE(FAIL, 'injected failure'); END", nullptr, nullptr, nullptr) == SQLITE_OK,
            "install failing write");
        require(sqlite3_close(raw) == SQLITE_OK, "close fixture catalog");
    }
    Indexer indexer(context);
    require(indexer.SetCallback(Events::Index, &events) == MC_OK &&
        indexer.Schedule(MC_INDEXER_SCAN_ALL) == MC_OK, "schedule gated scan");
    if (cancel) {
        wait(context, *event, [&] { return events.waiting; });
        require(indexer.Cancel() == MC_OK, "interrupt active source");
    }
    wait(context, *event, [&] { return events.terminal && events.rollback; });
    mc_result status = MC_ERROR_BUSY;
    size_t count = 0;
    OpenTrackSearch(context, TrackSearch {}, [&](mc_result result, TrackResult tracks, std::string_view) {
        status = result;
        count = tracks.Size();
    });
    wait(context, *event, [&] { return status != MC_ERROR_BUSY; });
    require(status == MC_OK && count == 1, "only checkpointed track survives rollback");
    require(context.Close() == MC_OK, "unload source after scan stops");

    if (createFailure) {
        /* a failed source must terminate before another scan can remove its tracks. */
#ifdef WIN32
        _putenv_s("MUSIKCORE_TEST_INDEX_MODE", "create-fail");
#else
        setenv("MUSIKCORE_TEST_INDEX_MODE", "create-fail", 1);
#endif
        events = {};
        Context reopened(options);
        require(false, "failed source unexpectedly allowed startup");
    }
}
