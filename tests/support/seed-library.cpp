#include <musikcore/musikcore.hpp>
#include "host-wakeup.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
    using namespace musik::core::api;
    using Clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    template<class Ready>
    bool dispatchUntil(Context& context, test_host_event& event, Ready ready) {
        const auto deadline = Clock::now() + 5min;
        while (!ready()) {
            if (context.Dispatch() != MC_OK) {
                return false;
            }
            if (ready()) {
                return true;
            }

            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(deadline - Clock::now());
            if (remaining <= 0ms || !test_host_event_wait_for(&event, static_cast<unsigned>(remaining.count()))) {
                return false;
            }
        }
        return true;
    }

    struct SeedState {
        int64_t expected;
        bool indexed = false;

        static void MC_CALL OnIndexEvent(void* data, mc_indexer_event event, int scanned) {
            auto& state = *static_cast<SeedState*>(data);
            /* An upgrade can finish its own scan before the requested media scan. */
            if (event == MC_INDEXER_FINISHED && scanned >= state.expected) {
                state.indexed = true;
            }
        }
    };

    bool seed(const char* media, int64_t expected) {
        std::unique_ptr<test_host_event, decltype(&test_host_event_destroy)>
            event(test_host_event_create(), test_host_event_destroy);
        if (!event) {
            return false;
        }

        SeedState state { expected };
        mc_result result = MC_ERROR_BUSY;
        size_t count = 0;
        auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
        options.flags = MC_CONTEXT_MANAGED_RUNTIME;
        options.wakeup = test_host_event_signal;
        options.wakeup_user_data = event.get();
        Context context(options);
        Indexer indexer(context);
        if (context.SelectLibrary(MC_LIBRARY_LOCAL) != MC_OK ||
            indexer.SetCallback(SeedState::OnIndexEvent, &state) != MC_OK ||
            indexer.AddPath(media) != MC_OK || indexer.Schedule(MC_INDEXER_SCAN_LOCAL) != MC_OK ||
            !dispatchUntil(context, *event, [&] { return state.indexed; }))
        {
            std::fputs("Indexer did not finish\n", stderr);
            return false;
        }

        /* Read the committed catalog through the same typed search used by Cube. */
        OpenTrackSearch(context, TrackSearch {}, [&](mc_result status, TrackResult tracks, std::string_view) {
            result = status;
            count = tracks.Size();
        });
        if (!dispatchUntil(context, *event, [&] { return result != MC_ERROR_BUSY; }) || result != MC_OK) {
            std::fputs("Could not verify indexed tracks\n", stderr);
            return false;
        }
        if (count != static_cast<uint64_t>(expected)) {
            std::fprintf(stderr, "Expected %lld visible tracks, indexed %zu\n", static_cast<long long>(expected), count);
            return false;
        }
        return true;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        return 1;
    }

    const auto expected = std::strtoll(argv[2], nullptr, 10);
    if (expected < 0) {
        return 1;
    }

    try {
        return seed(argv[1], expected) ? 0 : 1;
    }
    catch (const Error& error) {
        std::fprintf(stderr, "Seeder failed: %s\n", error.what());
        return 1;
    }
}
