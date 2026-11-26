/* exercise the reset/interrupt handoff through the C source contract. */
#include <musikcore/plugin/indexer.hpp>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>

namespace {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned resets = 0;
    unsigned scans = 0;
    unsigned interrupts = 0;
    bool interrupted = false;
    bool nextStarted = false;
    bool fenced = false;
    bool failed = false;

    /* overlap a cancelled scan's interrupt with the following provider reset. */
    class Source {
    public:
        Source(const mc_host_api&, const mc_plugin_services& services) : host(*services.indexer) {
            if (!this->host.request_rescan || this->host.schedule_async) {
                throw std::runtime_error("source service must grant only source-bound rescans");
            }
        }

        static const std::string& Key() {
            static const std::string key = "runtime-indexer-fixture";
            return key;
        }

        void OnBeforeScan() {
            std::lock_guard lock(mutex);
            interrupted = false;
            ++resets;
            changed.notify_all();
        }

        void OnAfterScan() {}

        bool Scan(musik::core::plugin::IndexerWriter*, const char**, size_t) {
            std::unique_lock lock(mutex);
            ++scans;
            changed.notify_all();
            if (scans == 1 && !changed.wait_for(lock, std::chrono::seconds(5), [] { return interrupted; })) {
                failed = true;
            }
            return true;
        }

        void ScanTrack(musik::core::plugin::IndexerWriter*, const char*) {}
        bool HasStableIds() const { return true; }
        bool NeedsTrackScan() const { return false; }

        void Interrupt() {
            {
                std::lock_guard lock(mutex);
                interrupted = true;
                ++interrupts;
                changed.notify_all();
            }
            /* a rescan requested after cancellation must remain queued. */
            if (this->host.request_rescan(this->host.context) != MC_OK) {
                std::lock_guard lock(mutex);
                failed = true;
                return;
            }
            std::unique_lock lock(mutex);
            if (!changed.wait_for(lock, std::chrono::seconds(5), [] { return nextStarted; })) {
                failed = true;
                return;
            }
            /* the next worker has started but cannot reset this source until
            the preceding interrupt has returned. */
            fenced = !changed.wait_for(lock, std::chrono::milliseconds(250), [] { return resets >= 2; });
        }

    private:
        const mc_plugin_indexer_service& host;
    };

    mc_result MC_CALL initialize(const mc_host_api* host, void**, mc_error**) {
        return musik::core::plugin::RegisterIndexerSource<Source>(*host);
    }

    void MC_CALL shutdown(void*) {}

}

extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t version, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error)
{
    const auto result = mc_plugin_validate_host_api(version, host, plugin, error);
    if (result != MC_OK) {
        return result;
    }
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "org.musikcube.test.runtime-indexer", sizeof("org.musikcube.test.runtime-indexer") - 1 };
    plugin->name = { "RuntimeIndexer", 14 };
    plugin->version = { "1", 1 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
}

/* the test uses separate probes so it can synchronize with the loaded source. */
extern "C" MC_PLUGIN_API bool indexer_fixture_wait_scan() {
    std::unique_lock lock(mutex);
    return changed.wait_for(lock, std::chrono::seconds(5), [] { return scans == 1; });
}

extern "C" MC_PLUGIN_API void indexer_fixture_next_started() {
    std::lock_guard lock(mutex);
    nextStarted = true;
    changed.notify_all();
}

extern "C" MC_PLUGIN_API bool indexer_fixture_fenced() {
    std::lock_guard lock(mutex);
    return fenced && !failed && resets == 2 && scans == 2 && interrupts == 1;
}
