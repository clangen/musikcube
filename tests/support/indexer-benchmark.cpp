#include <musikcore/musikcore.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

namespace {
    int completed = 0;
    bool cancelled = false;

    void MC_CALL onIndexEvent(void*, mc_indexer_event event, int updates) {
        if (event == MC_INDEXER_FINISHED) {
            std::printf("terminal %u %d\n", event, updates);
            ++completed;
        }
        else if (event == MC_INDEXER_CANCELLED) {
            cancelled = true;
        }
    }
}

/* match the captured baseline: wait three seconds after the final scan so
queued startup work has time to begin before the isolated process exits. */
int main(int argc, char** argv) {
    if (argc != 3) {
        std::fputs("expected media and plugin directories\n", stderr);
        return 1;
    }

    auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_INDEXER;
    options.flags = MC_CONTEXT_START_LOCAL | MC_CONTEXT_ENABLE_LOGGING;
    options.plugin_directory = { argv[2], std::strlen(argv[2]) };
    mc_context* context = nullptr;
    if (mc_context_create(&options, &context, nullptr) != MC_OK ||
        mc_indexer_set_callback(context, onIndexEvent, nullptr) != MC_OK ||
        mc_indexer_add_path(context, { argv[1], std::strlen(argv[1]) }) != MC_OK ||
        mc_indexer_schedule(context, MC_INDEXER_SCAN_LOCAL) != MC_OK)
    {
        return 1;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(5);
    int idleChecks = 0;
    while (std::chrono::steady_clock::now() < deadline) {
        mc_indexer_state state;
        if (mc_context_dispatch(context) != MC_OK || mc_indexer_get_state(context, &state) != MC_OK) {
            return 1;
        }
        if (completed && state == MC_INDEXER_IDLE) {
            if (++idleChecks >= 30) {
                break;
            }
        }
        else {
            idleChecks = 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    const bool finished = completed && !cancelled && idleChecks >= 30;
    const auto destroyed = mc_context_destroy(&context);
    return finished && destroyed == MC_OK ? 0 : 1;
}
