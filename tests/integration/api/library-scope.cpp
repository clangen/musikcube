#include <musikcore/musikcore.h>
#include <sqlite3.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

namespace {
    void require(bool value, const char* message) {
        if (!value) {
            std::fprintf(stderr, "library scope: %s\n", message);
            std::abort();
        }
    }
    struct Reply {
        bool done = false;
        mc_result status = MC_OK;
        size_t rows = 0;
    };
    void MC_CALL completed(mc_result status, mc_track_page* page, const mc_error*, void* data) {
        auto& reply = *static_cast<Reply*>(data);
        reply.status = status;
        reply.rows = mc_track_page_get_count(page);
        reply.done = true;
        mc_track_page_destroy(&page);
    }
    void wait(mc_context* context, Reply& reply) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!reply.done && std::chrono::steady_clock::now() < deadline) {
            mc_context_dispatch(context);
            /* bounded dispatch pumping for this worker/dispatcher contract test. */
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        require(reply.done, "completion deadline");
    }
    /* rendezvous at the public call boundary without assuming worker timing. */
    class Gate {
    public:
        void Open() {
            std::lock_guard lock(this->mutex);
            this->open = true;
            this->changed.notify_all();
        }

        void Wait() {
            std::unique_lock lock(this->mutex);
            require(this->changed.wait_for(lock, std::chrono::seconds(3),
                        [&] {
                            return this->open;
                        }),
                "synchronization deadline");
        }

    private:
        std::mutex mutex;
        std::condition_variable changed;
        bool open = false;
    };

    void selectFiles(mc_library* library) {
        int32_t id = 0;
        auto sources = mc_library_source_filter MC_LIBRARY_SOURCE_FILTER_DEFAULT;
        sources.ids = &id;
        sources.count = 1;
        auto scope = mc_library_scope MC_LIBRARY_SCOPE_DEFAULT;
        scope.sources = &sources;
        require(mc_library_set_scope(library, &scope) == MC_OK, "select files");
    }

    void checkConcurrentSubmission(mc_context* context, mc_library* library) {
        for (unsigned iteration = 0; iteration < 40; ++iteration) {
            require(mc_library_clear_scope(library) == MC_OK, "reset racing selection");
            Gate start;
            Reply reply;
            mc_result submitted = MC_ERROR_INTERNAL;
            std::thread caller([&] {
                start.Wait();
                auto query = mc_track_query MC_TRACK_QUERY_DEFAULT;
                mc_request_id request = 0;
                submitted = mc_library_query_tracks_async(context, library, &query, completed, &reply, &request);
            });
            std::thread updater([&] {
                start.Wait();
                selectFiles(library);
            });
            start.Open();
            caller.join();
            updater.join();
            require(submitted == MC_OK, "concurrent submission accepted");
            wait(context, reply);
            /* dispatch follows both calls: old membership may only be cancelled. */
            require(reply.status == MC_ERROR_CANCELLED || (reply.status == MC_OK && reply.rows == 1),
                "submission cannot escape concurrent invalidation");
        }
    }

    void checkObserverLifetime(mc_library* library) {
        require(mc_library_clear_scope(library) == MC_OK, "reset observer selection");
        struct Observer {
            Gate entered;
            Gate release;
            unsigned calls = 0;
        } observer;
        mc_subscription* blocked = nullptr;
        require(mc_library_subscribe_scope(
                    library,
                    [](void* data, const void*) {
                        auto& observer = *static_cast<Observer*>(data);
                        ++observer.calls;
                        observer.entered.Open();
                        observer.release.Wait();
                    },
                    &observer, &blocked) == MC_OK,
            "subscribe blocked observer");
        std::thread updater([&] {
            selectFiles(library);
        });
        observer.entered.Wait();

        /* registration can miss an already admitted publish. subscribing before
        the initial read still observes the committed selection. */
        std::atomic<unsigned> newerCalls { 0 };
        mc_subscription* newer = nullptr;
        require(mc_library_subscribe_scope(
                    library,
                    [](void* data, const void*) {
                        ++*static_cast<std::atomic<unsigned>*>(data);
                    },
                    &newerCalls, &newer) == MC_OK,
            "register during update delivery");
        mc_library_scope_snapshot* current = nullptr;
        require(mc_library_get_scope(library, &current) == MC_OK && mc_library_scope_read(current)->sources &&
                mc_library_scope_read(current)->sources->count == 1,
            "subscribe then read cannot miss concurrent replacement");
        mc_library_scope_destroy(&current);

        /* reentrant removal cannot wait for callbacks on another thread.
        borrowed callback data stays alive until that admitted callback exits. */
        mc_event_source* unrelated = nullptr;
        mc_subscription* remover = nullptr;
        require(mc_event_source_create(&unrelated) == MC_OK &&
                mc_event_source_subscribe(
                    unrelated,
                    [](void* data, const void*) {
                        mc_subscription_destroy(static_cast<mc_subscription**>(data));
                    },
                    &blocked, &remover) == MC_OK,
            "subscribe reentrant removal");
        mc_event_source_publish(unrelated, nullptr);
        require(!blocked, "reentrant removal returned while another callback is admitted");
        observer.release.Open();
        updater.join();
        const auto before = newerCalls.load();
        require(mc_library_clear_scope(library) == MC_OK && newerCalls == before + 1 && observer.calls == 1,
            "only connected observers receive subsequent changes");
        mc_subscription_destroy(&newer);
        mc_subscription_destroy(&remover);
        mc_event_source_destroy(&unrelated);
    }

    void checkInvalidationBeforeRefresh(mc_context* context, mc_library* library) {
        require(mc_library_clear_scope(library) == MC_OK, "reset refresh selection");
        struct Refresh {
            mc_context* context;
            Reply old;
            Reply fresh;
            bool notified = false;
        } refresh { context, {}, {}, false };
        auto query = mc_track_query MC_TRACK_QUERY_DEFAULT;
        mc_request_id request = 0;
        require(mc_library_query_tracks_async(context, library, &query, completed, &refresh.old, &request) == MC_OK,
            "submit before refresh notification");
        mc_subscription* observer = nullptr;
        require(mc_library_subscribe_scope(
                    library,
                    [](void* data, const void* value) {
                        auto& refresh = *static_cast<Refresh*>(data);
                        const auto& event = *static_cast<const mc_library_scope_change*>(value);
                        require(mc_context_dispatch(refresh.context) == MC_OK && refresh.old.done &&
                                refresh.old.status == MC_ERROR_CANCELLED,
                            "pending invalidation precedes refresh delivery");
                        auto query = mc_track_query MC_TRACK_QUERY_DEFAULT;
                        mc_request_id request = 0;
                        require(mc_library_query_tracks_async(refresh.context, event.library, &query, completed,
                                    &refresh.fresh, &request) == MC_OK,
                            "reentrant refresh submission");
                        refresh.notified = true;
                    },
                    &refresh, &observer) == MC_OK,
            "subscribe refresh");
        selectFiles(library);
        require(refresh.notified, "committed update delivered");
        mc_subscription_destroy(&observer);
        wait(context, refresh.fresh);
        require(refresh.fresh.status == MC_OK && refresh.fresh.rows == 1, "refresh sees committed selection");
    }

    void checkCreationBoundary(mc_context* context, mc_library* library) {
        auto options = mc_playlist_create_options MC_PLAYLIST_CREATE_OPTIONS_DEFAULT;
        options.name = { "Boundary", 8 };
        unsigned callbacks = 0;
        const auto completion = [](mc_result, int64_t, const mc_error*, void* data) {
            ++*static_cast<unsigned*>(data);
        };
        const auto reject = [&] {
            mc_request_id request = 99;
            require(mc_library_create_playlist_async(context, library, &options, nullptr, 0, completion, &callbacks,
                        &request) == MC_ERROR_INVALID_ARGUMENT &&
                    request == 0 && callbacks == 0,
                "reject malformed creation before backend admission");
        };
        options.struct_size = 0;
        reject();
        options.struct_size = sizeof(options);
        options.ownership = UINT32_MAX;
        reject();
        options.ownership = MC_PLAYLIST_SOURCE;
        options.source_id = -1;
        reject();
        options.struct_size = offsetof(mc_playlist_create_options, source_id);
        reject();
        options.struct_size = sizeof(options);
        options.source_id = 0;
        options.name = { nullptr, 8 };
        reject();
        options.name = { "Boundary", 8 };
        mc_request_id request = 99;
        require(mc_library_create_playlist_async(
                    context, library, &options, nullptr, 0, completion, &callbacks, &request) == MC_ERROR_UNSUPPORTED &&
                request == 0 && callbacks == 0,
            "valid creation reaches the unsupported backend only after validation");
    }

    void MC_CALL changed(void* data, const void* event) {
        ++*static_cast<int*>(data);
        auto& change = *static_cast<const mc_library_scope_change*>(event);
        mc_library_scope_snapshot* snapshot = nullptr;
        require(mc_library_get_scope(change.library, &snapshot) == MC_OK, "reentrant scope read");
        mc_library_scope_destroy(&snapshot);
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "expected fixture directory");
    const auto directory =
        std::filesystem::path(argv[1]) / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(directory);
    sqlite3* database = nullptr;
    require(sqlite3_open((directory / "musik.db").string().c_str(), &database) == SQLITE_OK, "open fixture");
    require(sqlite3_exec(database,
                "CREATE TABLE tracks(id INTEGER PRIMARY KEY,title TEXT,visible INTEGER,source_id INTEGER);"
                "INSERT INTO tracks VALUES(1,'Files',1,0),(2,'Provider',1,7);",
                nullptr, nullptr, nullptr) == SQLITE_OK,
        "seed fixture");
    sqlite3_close(database);
    const auto locator = directory.string();
    auto options = mc_library_options MC_LIBRARY_OPTIONS_DEFAULT;
    options.locator = { locator.data(), locator.size() };
    mc_library* library = nullptr;
    mc_library* independent = nullptr;
    require(mc_library_create(&options, &library) == MC_OK && mc_library_create(&options, &independent) == MC_OK,
        "create instances");
    mc_context* context = nullptr;
    auto contextOptions = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    contextOptions.services = MC_SERVICE_NONE;
    require(mc_context_create(&contextOptions, &context, nullptr) == MC_OK, "create context");
    require(mc_context_set_library(context, library) == MC_OK, "select instance");
    mc_library* alias = nullptr;
    require(mc_context_get_library(context, &alias) == MC_OK, "get alias");
    int first = 0, second = 0;
    mc_subscription* one = nullptr;
    mc_subscription* two = nullptr;
    auto legacyScope = mc_library_scope MC_LIBRARY_SCOPE_DEFAULT;
    legacyScope.struct_size = sizeof(legacyScope.struct_size);
    require(mc_library_set_scope(library, &legacyScope) == MC_OK, "older scope defaults missing filter to null");
    require(mc_library_subscribe_scope(library, changed, &first, &one) == MC_OK &&
            mc_library_subscribe_scope(alias, changed, &second, &two) == MC_OK,
        "subscribe observers");
    int32_t ids[] { 7, 0, 7 };
    auto scope = mc_library_scope MC_LIBRARY_SCOPE_DEFAULT;
    mc_library_source_filter filter = MC_LIBRARY_SOURCE_FILTER_DEFAULT;
    scope.sources = &filter;
    filter.ids = ids;
    filter.count = 3;
    require(mc_library_set_scope(library, &scope) == MC_OK && first == 1 && second == 1, "effective change");
    require(mc_library_set_scope(alias, &scope) == MC_OK && first == 1, "equivalent set");
    mc_library_scope_snapshot* captured = nullptr;
    require(mc_library_get_scope(alias, &captured) == MC_OK, "capture alias");
    ids[0] = 99;
    const auto* view = mc_library_scope_read(captured);
    require(
        view->sources->count == 2 && view->sources->ids[0] == 0 && view->sources->ids[1] == 7, "copied normalized IDs");
    mc_library_scope_snapshot* separate = nullptr;
    require(
        mc_library_get_scope(independent, &separate) == MC_OK && mc_library_scope_read(separate)->sources == nullptr,
        "independent instance");
    mc_library_scope_destroy(&separate);
    scope.struct_size = sizeof(scope) + 8;
    require(
        mc_library_set_scope(library, &scope) == MC_ERROR_UNSUPPORTED && first == 1, "unknown future filters rejected");
    scope.struct_size = sizeof(scope);
    filter.struct_size = offsetof(mc_library_source_filter, count);
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "present missing field rejected");
    filter.struct_size = sizeof(filter) + 8;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_UNSUPPORTED && first == 1,
        "unknown future filter layout rejected without event");
    filter.struct_size = sizeof(filter);
    filter.count = MC_LIBRARY_SCOPE_MAX_SOURCES + 1;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "selection bound before reading");
    filter.count = 1;
    filter.ids = nullptr;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "null array rejected");
    int32_t invalid = -1;
    filter.ids = &invalid;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "sentinel rejected");
    auto query = mc_track_query MC_TRACK_QUERY_DEFAULT;
    Reply old;
    mc_request_id request = 0;
    require(mc_library_query_tracks_async(context, library, &query, completed, &old, &request) == MC_OK,
        "submit original intent");
    filter.ids = ids;
    filter.count = 0;
    require(mc_library_set_scope(alias, &scope) == MC_OK, "switch to empty");
    wait(context, old);
    require(old.status == MC_ERROR_CANCELLED && old.rows == 0, "invalidate queued completion");
    Reply empty;
    require(mc_library_query_tracks_async(context, nullptr, &query, completed, &empty, &request) == MC_OK,
        "context-selected scope");
    wait(context, empty);
    require(empty.status == MC_OK && empty.rows == 0, "empty selection");
    int32_t files = 0;
    filter.ids = &files;
    filter.count = 1;
    require(mc_library_set_scope(library, &scope) == MC_OK, "select Files");
    Reply fileRows;
    require(
        mc_library_query_tracks_async(context, alias, &query, completed, &fileRows, &request) == MC_OK, "query Files");
    wait(context, fileRows);
    require(fileRows.status == MC_OK && fileRows.rows == 1, "source zero filters before page");
    /* once admitted, a callback may finish while another thread replaces scope.
    joining that update also proves delivery holds no scope lock. */
    struct Admitted {
        mc_library* library;
        Reply reply;
    } admitted { library, {} };
    const auto finishAdmitted = [](mc_result status, mc_track_page* page, const mc_error*, void* data) {
        auto& admitted = *static_cast<Admitted*>(data);
        std::thread update([&] {
            auto scope = mc_library_scope MC_LIBRARY_SCOPE_DEFAULT;
            auto sources = mc_library_source_filter MC_LIBRARY_SOURCE_FILTER_DEFAULT;
            scope.sources = &sources;
            require(mc_library_set_scope(admitted.library, &scope) == MC_OK, "update during admitted callback");
        });
        update.join();
        completed(status, page, nullptr, &admitted.reply);
    };
    require(mc_library_query_tracks_async(context, library, &query, finishAdmitted, &admitted, &request) == MC_OK,
        "submit admitted completion");
    wait(context, admitted.reply);
    require(admitted.reply.status == MC_OK && admitted.reply.rows == 1, "admission wins before scope update");
    require(mc_library_scope_read(captured)->sources->count == 2, "snapshot survives updates");
    mc_subscription_destroy(&one);
    mc_subscription_destroy(&two);
    checkCreationBoundary(context, library);
    checkConcurrentSubmission(context, library);
    checkObserverLifetime(library);
    checkInvalidationBeforeRefresh(context, library);
    const auto otherDirectory = directory / "other";
    std::filesystem::create_directories(otherDirectory);
    require(
        sqlite3_open((otherDirectory / "musik.db").string().c_str(), &database) == SQLITE_OK, "open second catalog");
    require(sqlite3_exec(database,
                "CREATE TABLE tracks(id INTEGER PRIMARY KEY,title TEXT,visible INTEGER,source_id INTEGER);"
                "INSERT INTO tracks VALUES(1,'Other catalog',1,0);",
                nullptr, nullptr, nullptr) == SQLITE_OK,
        "seed overlapping catalog identity");
    sqlite3_close(database);
    const auto otherLocator = otherDirectory.string();
    options.locator = { otherLocator.data(), otherLocator.size() };
    mc_library* other = nullptr;
    require(mc_library_create(&options, &other) == MC_OK, "create separate catalog");
    Reply otherReply;
    require(mc_library_query_tracks_async(
                context, other, &query,
                [](mc_result status, mc_track_page* page, const mc_error*, void* data) {
                    const auto title = mc_track_page_get_string(page, 0, 0);
                    require(status == MC_OK && std::string(title.data, title.length) == "Other catalog",
                        "overlapping IDs stay in their originating catalog");
                    completed(status, page, nullptr, data);
                },
                &otherReply, &request) == MC_OK,
        "query separate catalog");
    wait(context, otherReply);
    mc_library_destroy(&other);
    mc_library_scope_destroy(&captured);

    int afterShutdown = 0;
    mc_subscription* survivor = nullptr;
    require(mc_library_subscribe_scope(library, changed, &afterShutdown, &survivor) == MC_OK,
        "library observer independent of context lifetime");
    require(mc_library_clear_scope(library) == MC_OK, "reset shutdown selection");
    afterShutdown = 0;
    Reply shutdown;
    require(mc_library_query_tracks_async(context, library, &query, completed, &shutdown, &request) == MC_OK,
        "submit before shutdown");
    selectFiles(library);
    afterShutdown = 0;
    require(mc_context_shutdown(context) == MC_OK && shutdown.done && shutdown.status == MC_ERROR_CANCELLED,
        "shutdown drains pending scoped request exactly once");
    require(mc_library_clear_scope(library) == MC_OK, "scope remains mutable after context shutdown");
    require(afterShutdown == 1, "context shutdown preserves library observers");
    mc_subscription_destroy(&survivor);
    mc_context_destroy(&context);
    mc_library_destroy(&alias);
    mc_library_destroy(&library);
    mc_library_destroy(&independent);
    std::filesystem::remove_all(directory);
}
