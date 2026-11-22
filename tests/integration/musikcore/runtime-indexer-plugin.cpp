/* a cooperative source exposes the reset/interrupt handoff without filesystem,
network, audio hardware, or production-only fault injection. */
#include <musikcore/abi.h>
#include <musikcore/sdk/IPlugin.h>
#include <musikcore/sdk/IIndexerSource.h>
#include <musikcore/sdk/IIndexerNotifier.h>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace {
    using namespace musik::core::sdk;
    IIndexerNotifier* notifier = nullptr;
    std::mutex mutex;
    std::condition_variable changed;
    unsigned resets = 0;
    unsigned scans = 0;
    unsigned interrupts = 0;
    bool interrupted = false;
    bool nextStarted = false;
    bool fenced = false;
    bool failed = false;
    constexpr int fixtureSourceId { 417 };

    /* identify a test-only legacy provider without adding production hooks. */
    class Plugin final : public IPlugin {
        public:
            /* discovery borrows this static descriptor. */
            void Release() override {}
            /* keep discovery metadata distinct from production plugins. */
            const char* Name() override { return "RuntimeIndexer"; }
            /* provide stable fixture metadata across test builds. */
            const char* Version() override { return "1"; }
            /* identify the fixture owner without external resources. */
            const char* Author() override { return "musikcube tests"; }
            /* isolate the fixture's plugin identity and preferences. */
            const char* Guid() override { return "RuntimeIndexer"; }
            /* this headless provider has no settings dialog. */
            bool Configurable() override { return false; }
            /* never open UI during discovery or test execution. */
            void Configure() override {}
            /* configuration reload has no resources to replace. */
            void Reload() override {}
            /* reach the supported legacy source boundary during discovery. */
            int SdkVersion() override { return musik::core::sdk::SdkVersion; }
    } plugin;

    /* overlap a cancelled scan's interrupt with the following provider reset. */
    class Source final : public IIndexerSource {
        public:
            /* retire the instance only after the indexer has drained its worker. */
            void Release() override { delete this; }
            /* model providers that clear their cooperative interruption flag before each scan. */
            void OnBeforeScan() override {
                std::lock_guard lock(mutex);
                interrupted = false;
                ++resets;
                changed.notify_all();
            }
            /* retain observations after completion for the public-boundary assertion. */
            void OnAfterScan() override {}
            /* hold the first scan until cancellation; let the later scan finish without I/O. */
            ScanResult Scan(IIndexerWriter*, const char**, unsigned) override {
                std::unique_lock lock(mutex);
                ++scans;
                changed.notify_all();
                if (scans == 1 && !changed.wait_for(lock, std::chrono::seconds(5), [] {
                        return interrupted;
                    }))
                {
                    failed = true;
                }
                return ScanCommit;
            }
            /* no per-track work is needed to expose the provider lifecycle race. */
            void ScanTrack(IIndexerWriter*, ITagStore*, const char*) override {}
            /* avoid catalog-dependent per-track enumeration in this synchronization fixture. */
            bool NeedsTrackScan() override { return false; }
            /* keep source identity stable across the queued rescan. */
            bool HasStableIds() override { return true; }
            /* distinguish this source from filesystem records and ordinary plugins. */
            int SourceId() override { return fixtureSourceId; }
            /* queue newer work while interruption remains active to test the reset fence. */
            void Interrupt() override {
                {
                    std::lock_guard lock(mutex);
                    interrupted = true;
                    ++interrupts;
                    changed.notify_all();
                }
                /* this submission is newer than Cancel and must survive. */
                if (notifier) {
                    notifier->ScheduleRescan(this);
                }
                std::unique_lock lock(mutex);
                if (!changed.wait_for(lock, std::chrono::seconds(5), [] {
                        return nextStarted;
                    }))
                {
                    failed = true;
                    return;
                }
                /* the next worker has started, but its provider reset must wait
                until this interrupt returns. a deadline bounds the negative check. */
                fenced = !changed.wait_for(lock, std::chrono::milliseconds(250), [] {
                    return resets >= 2;
                });
            }
    };
}

/* expose the descriptor through the same entry point as a real legacy plugin. */
extern "C" MC_PLUGIN_API musik::core::sdk::IPlugin* GetPlugin() {
    return &plugin;
}
/* transfer an owned source instance through normal provider discovery. */
extern "C" MC_PLUGIN_API musik::core::sdk::IIndexerSource* GetIndexerSource() {
    return new Source();
}
/* borrow the session notifier so Interrupt can submit work through the existing SDK. */
extern "C" MC_PLUGIN_API void SetIndexerNotifier(musik::core::sdk::IIndexerNotifier* value) {
    notifier = value;
}
/* tell the test when cancellation can reliably overlap an active provider scan. */
extern "C" MC_PLUGIN_API bool indexer_fixture_wait_scan() {
    std::unique_lock lock(mutex);
    return changed.wait_for(lock, std::chrono::seconds(5), [] {
        return scans == 1;
    });
}
/* positively identify the next worker start before checking reset exclusion. */
extern "C" MC_PLUGIN_API void indexer_fixture_next_started() {
    std::lock_guard lock(mutex);
    nextStarted = true;
    changed.notify_all();
}
/* require both terminal scans and exactly one interruption with a protected reset. */
extern "C" MC_PLUGIN_API bool indexer_fixture_fenced() {
    std::lock_guard lock(mutex);
    return fenced && !failed && resets == 2 && scans == 2 && interrupts == 1;
}
