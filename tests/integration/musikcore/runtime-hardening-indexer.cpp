#include <musikcore/library/Indexer.h>
#include <musikcore/library/LocalLibrary.h>
#include <musikcore/library/query/SourceListQuery.h>
#include "runtime-hardening-cases.h"
#include "../../support/ApiHost.h"
#include "../../support/check.h"
#include <musikcore/musikcore.hpp>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/plugin/Plugins.h>
#include <common/FilesystemPath.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace runtime_tests {
    namespace {
        /* hold the scan worker before filesystem/provider work, without depending on catalog size. */
        class ScanGate {
            public:
                /* stop only the armed scan at its worker boundary and announce every start. */
                void Started() {
                    std::unique_lock lock(this->mutex);
                    ++this->starts;
                    this->changed.notify_all();
                    if (this->starts == this->blockAt) {
                        this->changed.wait(lock, [&] {
                            return this->released >= this->starts || this->closing;
                        });
                    }
                }

                /* bound the caller's wait for the selected scan to reach the gate. */
                bool Wait(unsigned count) {
                    std::unique_lock lock(this->mutex);
                    return this->changed.wait_for(lock, std::chrono::seconds(5), [&] {
                        return this->starts >= count;
                    });
                }

                /* select a later scan boundary without blocking intervening completed work. */
                void Arm(unsigned count) {
                    std::lock_guard lock(this->mutex);
                    this->blockAt = count;
                }

                /* release the current worker, or all future waits during failure cleanup. */
                void Release(bool closing = false) {
                    std::lock_guard lock(this->mutex);
                    this->released = this->starts;
                    this->closing = closing;
                    this->changed.notify_all();
                }

            private:
                std::mutex mutex;
                std::condition_variable changed;
                unsigned starts = 0;
                unsigned blockAt = 1;
                unsigned released = 0;
                bool closing = false;
        };
    }

    /* prove that a provider interrupt cannot overlap the next scan's flag reset. */
    bool checkIndexerProviderCancel() {
        using namespace musik::core;
        runtime::MessageQueue host;
        auto options = TestHostOptions(host);
        options.services = MC_SERVICE_INDEXER;
        options.flags = MC_CONTEXT_START_LOCAL;
        api::Context context(options);
        using Wait = bool (*)();
        using Signal = void (*)();
        std::string filename;
        for (const auto& plugin : musik::core::plugin::ListPlugins()) {
            if (plugin.id == "org.musikcube.test.runtime-indexer") {
                filename = plugin.filename;
            }
        }
#ifdef _WIN32
        auto handle = LoadLibraryW(musik::core::sdk::fs::pathFromUtf8(filename).c_str());
        const auto symbol = [&](const char* name) { return GetProcAddress(handle, name); };
        const auto close = [&] { if (handle) { FreeLibrary(handle); } };
#else
        auto handle = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL);
        const auto symbol = [&](const char* name) { return handle ? dlsym(handle, name) : nullptr; };
        const auto close = [&] { if (handle) { dlclose(handle); } };
#endif
        struct CloseLibrary {
            decltype(close)& function;
            ~CloseLibrary() { function(); }
        } closeLibrary { close };
        auto waitScan = reinterpret_cast<Wait>(symbol("indexer_fixture_wait_scan"));
        auto fenced = reinterpret_cast<Wait>(symbol("indexer_fixture_fenced"));
        auto nextStarted = reinterpret_cast<Signal>(symbol("indexer_fixture_next_started"));
        if (!TEST_CHECK(waitScan && fenced && nextStarted)) {
            return false;
        }
        unsigned starts = 0;
        auto started = LibraryFactory::Instance().DefaultLocalLibrary()->Indexer()->Started.Subscribe([&] {
            if (++starts == 2) {
                nextStarted();
            }
        });
        struct Events {
            unsigned cancelled = 0;
            unsigned finished = 0;
        } events;
        api::Indexer scanner(context);
        /* distinguish the cancelled scan from its newer successful rescan through the C callback. */
        const auto record = [](void* data, mc_indexer_event event, int) {
            auto& events = *static_cast<Events*>(data);
            if (event == MC_INDEXER_CANCELLED) {
                ++events.cancelled;
            } else if (event == MC_INDEXER_FINISHED) {
                ++events.finished;
            }
        };
        if (!TEST_CHECK(scanner.SetCallback(record, &events) == MC_OK) ||
            !TEST_CHECK(scanner.Schedule(MC_INDEXER_SCAN_ALL) == MC_OK) || !TEST_CHECK(waitScan()) ||
            !TEST_CHECK(scanner.Cancel() == MC_OK))
        {
            return false;
        }
        const bool drained = AwaitTestHost(host, [&] {
            return context.Dispatch() == MC_OK && events.cancelled == 1 && events.finished == 1;
        });
        if (!TEST_CHECK(drained) || !TEST_CHECK(fenced())) {
            return false;
        }
        /* source discovery can be constructed after shutdown. it must retain
        descriptions for loaded providers even when they had no catalog rows. */
        auto library = LibraryFactory::Instance().DefaultLocalLibrary();
        const auto sources = library->Sources();
        if (!TEST_CHECK(!sources.empty())) {
            return false;
        }
        started.Reset();
        if (!TEST_CHECK(context.Shutdown() == MC_OK)) {
            return false;
        }
        db::Connection database;
        if (!TEST_CHECK(database.Open(":memory:") == db::Okay)) {
            return false;
        }
        library::LocalLibrary::CreateDatabase(database);
        library::query::SourceListQuery discovery(library);
        if (!TEST_CHECK(discovery.Run(database))) {
            return false;
        }
        for (const auto& source : sources) {
            if (!TEST_CHECK(std::any_of(discovery.GetResult().begin(), discovery.GetResult().end(),
                    [&](const auto& entry) {
                        return entry.id == source.first && entry.label == source.second;
                    })))
            {
                return false;
            }
        }
        return true;
    }

    /* exercise cancellation admission, queue ordering, terminal state, reuse, and shutdown. */
    bool checkIndexerCancel() {
        using namespace musik::core;
        if (!TEST_CHECK(mc_indexer_cancel(nullptr) == MC_ERROR_INVALID_ARGUMENT)) {
            return false;
        }
        {
            auto options = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
            options.services = MC_SERVICE_NONE;
            api::Context context(options);
            if (!TEST_CHECK(api::Indexer(context).Cancel() == MC_ERROR_UNSUPPORTED)) {
                return false;
            }
        }

        runtime::MessageQueue host;
        auto options = TestHostOptions(host);
        options.services = MC_SERVICE_INDEXER;
        options.flags = MC_CONTEXT_START_LOCAL;
        api::Context context(options);
        api::Indexer scanner(context);
        auto* native = LibraryFactory::Instance().DefaultLocalLibrary()->Indexer();
        ScanGate gate;
        auto started = native->Started.Subscribe([&] {
            gate.Started();
        });
        std::atomic<bool> idleAtTerminal { true };
        auto cancelled = native->Cancelled.Subscribe([&](int) {
            if (native->GetState() != musik::core::Indexer::StateIdle) {
                idleAtTerminal = false;
            }
        });
        /* release a blocked worker before subscription/context destruction on every assertion failure. */
        struct ReleaseGate {
            /* unwind failed assertions without stranding a subscribed scan worker. */
            ~ReleaseGate() { this->gate.Release(true); }

            ScanGate& gate;
        } release { gate };
        struct Events {
            unsigned started = 0;
            unsigned finished = 0;
            unsigned cancelled = 0;
        } events;
        /* count lossless terminal events separately from coalescible progress notifications. */
        const auto record = [](void* data, mc_indexer_event event, int) {
            auto& events = *static_cast<Events*>(data);
            if (event == MC_INDEXER_STARTED) {
                ++events.started;
            }
            else if (event == MC_INDEXER_FINISHED) {
                ++events.finished;
            }
            else if (event == MC_INDEXER_CANCELLED) {
                ++events.cancelled;
            }
        };
        if (!TEST_CHECK(scanner.SetCallback(record, &events) == MC_OK)) {
            return false;
        }
        /* pump managed dispatch from real wakeups while enforcing the shared bounded deadline. */
        const auto await = [&](auto ready) {
            return AwaitTestHost(host, [&] {
                return context.Dispatch() == MC_OK && ready();
            });
        };

        /* cancellation targets local scans and rejects foreign threads before touching worker state. */
        mc_result offThread = MC_OK;
        std::thread caller([&] {
            offThread = mc_indexer_cancel(context.Get());
        });
        caller.join();
        if (!TEST_CHECK(offThread == MC_ERROR_WRONG_THREAD) || !TEST_CHECK(scanner.Cancel() == MC_OK) ||
            !TEST_CHECK(scanner.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK) || !TEST_CHECK(gate.Wait(1)))
        {
            return false;
        }
        if (!TEST_CHECK(scanner.Schedule(MC_INDEXER_SCAN_ALL) == MC_OK) ||
            !TEST_CHECK(scanner.Schedule(MC_INDEXER_REBUILD) == MC_OK) ||
            !TEST_CHECK(scanner.Cancel() == MC_OK) || !TEST_CHECK(scanner.Cancel() == MC_OK))
        {
            return false;
        }
        gate.Release();
        if (!TEST_CHECK(await([&] { return events.cancelled == 1; })) ||
            !TEST_CHECK(events.started == 1 && events.finished == 0) ||
            !TEST_CHECK(scanner.State() == MC_INDEXER_IDLE) || !TEST_CHECK(scanner.Cancel() == MC_OK))
        {
            return false;
        }

        /* a completed cancellation leaves the worker reusable. */
        if (!TEST_CHECK(scanner.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK) ||
            !TEST_CHECK(await([&] { return events.finished == 1; })) ||
            !TEST_CHECK(events.started == 2 && events.cancelled == 1))
        {
            return false;
        }

        /* a scan submitted after cancellation must survive while the cancelled scan still drains. */
        gate.Arm(3);
        if (!TEST_CHECK(scanner.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK) || !TEST_CHECK(gate.Wait(3)) ||
            !TEST_CHECK(scanner.Schedule(MC_INDEXER_REBUILD) == MC_OK) ||
            !TEST_CHECK(scanner.Cancel() == MC_OK) ||
            !TEST_CHECK(scanner.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK))
        {
            return false;
        }
        gate.Release();
        if (!TEST_CHECK(await([&] { return events.cancelled == 2 && events.finished == 2; })) ||
            !TEST_CHECK(events.started == 4) || !TEST_CHECK(scanner.State() == MC_INDEXER_IDLE) ||
            !TEST_CHECK(idleAtTerminal))
        {
            return false;
        }
        /* shutdown remains safe when cancellation is requested but has not yet drained. */
        gate.Arm(5);
        if (!TEST_CHECK(scanner.Schedule(MC_INDEXER_SCAN_LOCAL) == MC_OK) || !TEST_CHECK(gate.Wait(5)) ||
            !TEST_CHECK(scanner.Cancel() == MC_OK))
        {
            return false;
        }
        gate.Release();
        return TEST_CHECK(context.Shutdown() == MC_OK) && TEST_CHECK(scanner.Cancel() == MC_ERROR_BUSY);
    }
}
