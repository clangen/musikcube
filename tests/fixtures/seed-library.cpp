extern "C" {
#include <musikcore/musikcore_c.h>
}

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {
    std::mutex mutex;
    std::condition_variable changed;
    bool finished = false;
    bool verified = false;
    int64_t track_count = 0;

    void on_finished(mcsdk_svc_indexer, int) {
        std::lock_guard<std::mutex> lock(mutex);
        finished = true;
        changed.notify_one();
    }

    bool has_tracks(mcsdk_svc_library, mcsdk_db_connection database, void *) {
        const auto statement = mcsdk_db_statement_create(database,
            "SELECT COUNT(*) FROM tracks WHERE visible = 1");
        const bool found = mcsdk_db_statement_step(statement) == mcsdk_db_result_row;
        const auto count = found ? mcsdk_db_statement_column_int64(statement, 0) : 0;
        mcsdk_db_statement_release(statement);
        std::lock_guard<std::mutex> lock(mutex);
        track_count = count;
        verified = true;
        changed.notify_one();
        return found;
    }

    bool seed(mcsdk_context &context, const char *media, int64_t expected) {
        static mcsdk_svc_indexer_callbacks callbacks{ nullptr, on_finished, nullptr };

        /* Wait for the indexer to commit its work before inspecting the library. */
        mcsdk_svc_indexer_add_callbacks(context.indexer, &callbacks);
        mcsdk_svc_indexer_add_path(context.indexer, media);
        mcsdk_svc_indexer_schedule(context.indexer, mcsdk_svc_indexer_sync_type_local);
        {
            std::unique_lock<std::mutex> lock(mutex);
            if (!changed.wait_for(lock, std::chrono::minutes(5), [] { return finished; })) {
                std::fputs("Indexer did not finish\n", stderr);
                return false;
            }
        }

        /* The legacy wrapper queues the verification even with its synchronous flag. */
        mcsdk_svc_library_run_query(context.library, "Verify fixture tracks", nullptr,
            has_tracks, mcsdk_svc_library_query_flag_none);
        std::unique_lock<std::mutex> lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(5), [] { return verified; })) return false;
        if (track_count != expected) {
            std::fprintf(stderr, "Expected %lld visible tracks, indexed %lld\n",
                static_cast<long long>(expected), static_cast<long long>(track_count));
            return false;
        }
        return true;
    }
}

int main(int argc, char **argv) {
    if (argc != 3) return 1;

    mcsdk_context *context = nullptr;
    mcsdk_context_init(&context);
    const bool populated = context && seed(*context, argv[1], std::strtoll(argv[2], nullptr, 10));
    if (context) mcsdk_context_release(&context);
    mcsdk_env_release();
    if (!populated) std::fputs("Fixture library is empty or could not be indexed\n", stderr);
    return populated ? 0 : 1;
}
