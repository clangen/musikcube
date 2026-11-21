#include "cpp-wrapper-cases.h"

#ifdef MUSIKCORE_TEST_PRODUCTION_RUNTIME
#include "../../support/CoreRuntime.h"
#endif

#include <cstdio>
#include <cstring>
#include <exception>
#include <utility>

using namespace wrapper_tests;

/* Select one independently reported contract for CTest, or retain the full
   legacy flow when no case is specified. Every invocation owns fresh services. */
int main(int argc, char** argv) {
    if (argc != 2 && (argc != 4 || std::strcmp(argv[2], "--case") != 0)) {
        std::fputs("Expected catalog directory [--case name]\n", stderr);
        return 1;
    }

    const char* selected = argc == 4 ? argv[3] : nullptr;
    /* Only query deliberately blocks the producer hook. Other cases must run
       independently instead of relying on query having released it first. */
    if (selected && std::strcmp(selected, "query") != 0) {
        query_test_release_wakeup();
    }
    try {
#ifdef MUSIKCORE_TEST_PRODUCTION_RUNTIME
        CoreRuntime runtime;
#endif
        const std::pair<const char*, std::function<bool()>> cases[] = {
            { "query", [&] { return checkQuery(argv[1]); } },
            { "search", [&] { return checkSearch(argv[1]); } },
            { "result_windows", [&] { return checkResultWindows(argv[1]); } },
#ifdef MUSIKCORE_TEST_PRODUCTION_RUNTIME
            { "message_cleanup", [] { return checkMessageCleanup(); } },
            { "playback_commands", [] { return checkPlaybackCommands(); } },
            { "remote_payloads", [] { return checkRemoteQueryPayloads(); } },
            { "browse_reads", [&] { return checkBrowseReads(argv[1]); } },
            { "selected_backend", [] { return checkSelectedBackend(); } },
#endif
        };

        bool matched = false;
        for (const auto& entry : cases) {
            if (selected && std::strcmp(selected, entry.first) != 0) {
                continue;
            }
            matched = true;
            if (!entry.second()) {
                std::fprintf(stderr, "C++ wrapper case %s failed\n", entry.first);
                return 1;
            }
        }
        if (!matched) {
            std::fprintf(stderr, "Unknown C++ wrapper case: %s\n", selected);
            return 1;
        }
        return 0;
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "C++ wrapper case %s failed: %s\n", selected ? selected : "all", error.what());
        return 1;
    }
}
