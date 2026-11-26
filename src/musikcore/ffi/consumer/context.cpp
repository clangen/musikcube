#include <musikcore/context.h>
#include <musikcore/equalizer.h>

#include "../common/error.h"
#include "../common/primitives.h"
#include "request.h"

#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
    /* the ABI-only target has no application logger during failed teardown. */
    [[noreturn]] void failPluginScope(const char* message) noexcept {
        std::fprintf(stderr, "musikcore fatal plugin request failure: %s\n", message);
        std::abort();
    }

    enum class RequestState { queued, running, ready };

    struct ExternalRequests {
        std::mutex mutex;
        mc_context *context = nullptr;
    };

    struct Request {
        mc_request_id id;
        std::function<mc_result()> work;
        musik::core::ffi::RequestCompletion completion;
        void *user_data;
        RequestState state;
        mc_result result;
        std::shared_ptr<std::atomic_bool> cancel;
        musik::core::ffi::RequestEffect effect;
        bool external;
        const void* owner;
        std::function<void()> cancel_external;
    };
}

struct mc_context {
    mc_context(std::string plugins, mc_wakeup_fn signal, void *signal_data,
        musik::core::ffi::TrackQueryLimits query_limits, mc_services services)
    : plugin_directory(std::move(plugins))
    , track_query_limits(query_limits)
    , services(services)
    , wakeup(signal)
    , wakeup_user_data(signal_data)
    {
        external->context = this;
    }

    std::string plugin_directory;
    const musik::core::ffi::TrackQueryLimits track_query_limits;

    const mc_services services;

    mc_wakeup_fn wakeup;
    void *wakeup_user_data;

    mutable std::mutex mutex;
    std::condition_variable changed;
    std::thread worker;
    std::shared_ptr<std::atomic_bool> active_cancel;
    std::thread::id dispatcher;
    std::shared_ptr<const LibraryInstance> library;
    musik::core::ffi::LibraryStatus libraryStatus;
    std::unique_ptr<musik::core::ffi::ContextRuntime> runtime;
    mc_indexer_event_fn indexerCallback = nullptr;
    void* indexerData = nullptr;
    uint64_t indexerSubscription = 0;
    std::vector<std::pair<mc_indexer_event, int>> indexerEvents;
    mc_playlist_changed_fn playlistChanged = nullptr;
    void* playlistChangedData = nullptr;
    uint64_t playlistSubscription = 0;
    std::vector<std::pair<mc_playlist_event, int64_t>> playlistEvents;
    mc_equalizer_changed_fn equalizerChanged = nullptr;
    void* equalizerChangedData = nullptr;
    uint64_t equalizerSubscription = 0;
    bool equalizerNotificationPending = false;
    mc_library_changed_fn libraryChanged = nullptr;
    void* libraryChangedData = nullptr;
    uint64_t librarySubscription = 0;
    bool libraryNotificationPending = false;
    bool librarySelectionChanged = false;
    std::list<std::weak_ptr<musik::core::ffi::ContextNotification>> notifications;
    std::list<std::weak_ptr<musik::core::ffi::ContextNotification>> notification_batch;

    std::list<Request> requests;
    std::list<Request> dispatch_batch;
    std::shared_ptr<ExternalRequests> external = std::make_shared<ExternalRequests>();
    mc_request_id next_request = 1;

    bool dispatching = false;
    bool stopping = false;
    bool wakeup_pending = false;
    size_t wakeups_in_flight = 0;
};

std::shared_ptr<const LibraryInstance> musik::core::ffi::context_library(const mc_context& context) {
    std::lock_guard<std::mutex> lock(context.mutex);
    return context.library;
}

mc_result MC_CALL mc_context_get_services(const mc_context* context, mc_services* out_services) {
    if (!context || !out_services) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    *out_services = context->services;
    return MC_OK;
}

mc_result MC_CALL mc_player_create(mc_context* context, mc_player** out_player) {
    if (!context || !out_player || *out_player) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        musik::core::ffi::ContextRuntime* runtime;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->dispatcher != std::thread::id {} && context->dispatcher != std::this_thread::get_id()) {
                return MC_ERROR_WRONG_THREAD;
            }
            if (context->stopping) {
                return MC_ERROR_BUSY;
            }
            if (!(context->services & MC_SERVICE_PLUGINS)) {
                return MC_ERROR_UNSUPPORTED;
            }
            runtime = context->runtime.get();
        }
        return runtime ? runtime->CreatePlayer(out_player) : MC_ERROR_UNSUPPORTED;
    });
}

mc_result MC_CALL mc_context_get_playback(const mc_context* context, mc_playback** out_playback) {
    if (!context || !out_playback || *out_playback) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        musik::core::ffi::ContextRuntime* runtime;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->dispatcher != std::thread::id {} &&
                context->dispatcher != std::this_thread::get_id())
            {
                return MC_ERROR_WRONG_THREAD;
            }
            if (context->stopping) {
                return MC_ERROR_BUSY;
            }
            runtime = context->runtime.get();
        }
        return runtime ? runtime->OpenPlayback(out_playback) : MC_ERROR_UNSUPPORTED;
    });
}

namespace {
    std::atomic<bool> context_active{ false };
    constexpr size_t max_requests = 64;

    auto find_request(mc_context &context, mc_request_id id) -> std::list<Request>::iterator {
        return std::find_if(context.requests.begin(), context.requests.end(),
            [id](const Request &request) { return request.id == id; });
    }

    bool select_wakeup(mc_context &context) {
        if (!context.wakeup || context.stopping || context.wakeup_pending) return false;

        context.wakeup_pending = true;
        ++context.wakeups_in_flight;
        return true;
    }

    void signal_host(mc_context &context) {
        musik::core::ffi::exception_barrier(nullptr, [&] {
            context.wakeup(context.wakeup_user_data);
            return MC_OK;
        });

        /* Let shutdown proceed once the host hook returns. */
        std::lock_guard<std::mutex> lock(context.mutex);
        --context.wakeups_in_flight;
        context.changed.notify_all();
    }

    void run_worker(mc_context &context) {
        std::unique_lock<std::mutex> lock(context.mutex);

        while (!context.stopping) {
            auto request = std::find_if(context.requests.begin(), context.requests.end(),
                [](const Request &entry) { return entry.state == RequestState::queued; });

            if (request == context.requests.end()) {
                context.changed.wait(lock);
                continue;
            }

            /* Cancellation may retire the entry while its work is running. */
            const auto id = request->id;
            auto work = std::move(request->work);
            request->state = RequestState::running;
            context.active_cancel = request->cancel;

            lock.unlock();
            const auto result = musik::core::ffi::exception_barrier(nullptr, work);
            work = nullptr;
            lock.lock();

            context.active_cancel.reset();

            /* Preserve the terminal result if cancellation won during execution. */
            request = find_request(context, id);
            if (request == context.requests.end() || request->state == RequestState::ready) continue;

            request->result = result;
            request->state = RequestState::ready;
            const bool notify = select_wakeup(context);

            lock.unlock();
            if (notify) signal_host(context);
            lock.lock();
        }
    }

    bool enter_dispatch(mc_context &context) {
        const auto thread = std::this_thread::get_id();
        if (context.dispatching ||
            (context.dispatcher != std::thread::id{} && context.dispatcher != thread)) return false;

        context.dispatcher = thread;
        context.dispatching = true;
        return true;
    }

    void deliver_ready(mc_context &context, std::unique_lock<std::mutex> &lock) {
        /* Take the current batch so callbacks can enqueue work for a later dispatch. */
        auto &ready = context.dispatch_batch;
        for (auto it = context.requests.begin(); it != context.requests.end();) {
            auto entry = it++;
            if (entry->state == RequestState::ready) ready.splice(ready.end(), context.requests, entry);
        }

        const auto notifyLibrary = context.libraryNotificationPending
            ? context.libraryChanged
            : nullptr;
        const auto selectionChanged = context.librarySelectionChanged;
        auto* const libraryData = context.libraryChangedData;
        const auto librarySubscription = context.librarySubscription;
        context.libraryNotificationPending = false;
        context.librarySelectionChanged = false;

        const auto notifyIndexer = context.indexerCallback;
        auto* const indexerData = context.indexerData;
        const auto indexerSubscription = context.indexerSubscription;
        std::vector<std::pair<mc_indexer_event, int>> indexerEvents;
        indexerEvents.swap(context.indexerEvents);

        const auto notifyPlaylist = context.playlistChanged;
        auto* const playlistData = context.playlistChangedData;
        const auto playlistSubscription = context.playlistSubscription;
        std::vector<std::pair<mc_playlist_event, int64_t>> playlistEvents;
        playlistEvents.swap(context.playlistEvents);

        const auto notifyEqualizer = context.equalizerNotificationPending ? context.equalizerChanged : nullptr;
        auto* const equalizerData = context.equalizerChangedData;
        const auto equalizerSubscription = context.equalizerSubscription;
        context.equalizerNotificationPending = false;

        /* detach existing slots without allocating during dispatch. callbacks can
        register fresh slots without invalidating this delivery batch. */
        auto& notifications = context.notification_batch;
        notifications.splice(notifications.end(), context.notifications);

        /* Rearm before invoking callbacks so concurrent producers can wake the next dispatch. */
        context.wakeup_pending = false;
        lock.unlock();

        for (auto it = notifications.begin(); it != notifications.end();) {
            if (const auto notification = it->lock()) {
                notification->Deliver();
                ++it;
            }
            else {
                it = notifications.erase(it);
            }
        }
        lock.lock();
        context.notifications.splice(context.notifications.end(), notifications);
        lock.unlock();

        if (notifyLibrary) {
            /* an earlier observer may replace/remove this callback and release
            its user_data. only the captured subscription can receive this batch. */
            lock.lock();
            const bool subscribed = context.librarySubscription == librarySubscription;
            lock.unlock();
            if (subscribed) {
                musik::core::ffi::exception_barrier(nullptr, [&] {
                    notifyLibrary(libraryData, selectionChanged);
                    return MC_OK;
                });
            }
        }
        for (const auto& event : indexerEvents) {
            lock.lock();
            const bool subscribed = notifyIndexer && context.indexerSubscription == indexerSubscription;
            lock.unlock();
            if (!subscribed) {
                break;
            }
            musik::core::ffi::exception_barrier(nullptr, [&] {
                notifyIndexer(indexerData, event.first, event.second);
                return MC_OK;
            });
        }
        for (const auto& event : playlistEvents) {
            lock.lock();
            const bool subscribed = notifyPlaylist && context.playlistSubscription == playlistSubscription;
            lock.unlock();
            if (!subscribed) {
                break;
            }
            musik::core::ffi::exception_barrier(nullptr, [&] {
                notifyPlaylist(playlistData, event.first, event.second);
                return MC_OK;
            });
        }
        if (notifyEqualizer) {
            lock.lock();
            const bool subscribed = context.equalizerSubscription == equalizerSubscription;
            lock.unlock();
            if (subscribed) {
                musik::core::ffi::exception_barrier(nullptr, [&] {
                    notifyEqualizer(equalizerData);
                    return MC_OK;
                });
            }
        }
        while (!ready.empty()) {
            /* remove the active callback first; a preceding callback may stop
            another plugin and cancel its remaining entries in this batch. */
            auto request = std::move(ready.front());
            ready.pop_front();
            musik::core::ffi::exception_barrier(nullptr, [&] {
                request.completion(request.result, request.user_data);
                return MC_OK;
            });
        }

        lock.lock();
        context.dispatching = false;
    }

    bool valid_directory(mc_string_view directory) {
        return musik::core::ffi::valid_text(directory);
    }

}

void musik::core::ffi::wake_context(mc_context& context) {
    bool notify;
    {
        std::lock_guard<std::mutex> lock(context.mutex);
        notify = select_wakeup(context);
    }
    if (notify) {
        signal_host(context);
    }
}

namespace {
    mc_result update_library(mc_context& context, std::shared_ptr<const LibraryInstance> library,
        musik::core::ffi::LibraryStatus status, bool managed, bool selectionChanged)
    {
        bool wake = false;
        {
            std::lock_guard<std::mutex> lock(context.mutex);
            if (context.stopping) {
                return MC_ERROR_BUSY;
            }
            if (!managed && (context.services & MC_SERVICE_LIBRARY)) {
                return MC_ERROR_UNSUPPORTED;
            }
            if (!managed && context.library == library) {
                return MC_OK;
            }

            /* Release the previous identity after dropping the context lock. */
            context.library.swap(library);
            context.libraryStatus = std::move(status);
            if (selectionChanged) {
                ++context.playlistSubscription;
                context.playlistEvents.clear();
            }
            if (context.libraryChanged) {
                context.libraryNotificationPending = true;
                context.librarySelectionChanged |= selectionChanged;
                wake = select_wakeup(context);
            }
        }
        if (wake) {
            signal_host(context);
        }
        return MC_OK;
    }

    bool is_dispatcher(const mc_context& context) {
        return context.dispatcher == std::thread::id {} || context.dispatcher == std::this_thread::get_id();
    }
}

mc_result musik::core::ffi::set_context_library(mc_context& context,
    std::shared_ptr<const LibraryInstance> library, LibraryStatus status)
{
    if (context.services & MC_SERVICE_LIBRARY) {
        ContextRuntime* runtime;
        {
            std::lock_guard<std::mutex> lock(context.mutex);
            if (!is_dispatcher(context)) {
                return MC_ERROR_WRONG_THREAD;
            }
            if (context.stopping) {
                return MC_ERROR_BUSY;
            }
            runtime = context.runtime.get();
        }
        return runtime && library ? runtime->SelectLibrary(library) : MC_ERROR_UNSUPPORTED;
    }
    return update_library(context, std::move(library), std::move(status), false, true);
}

mc_result musik::core::ffi::publish_library_selection(mc_context& context,
    std::shared_ptr<const LibraryInstance> library, LibraryStatus status, bool selectionChanged)
{
    return update_library(context, std::move(library), std::move(status), true, selectionChanged);
}

void musik::core::ffi::publish_indexer_event(mc_context& context, mc_indexer_event event, int updates) {
    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(context.mutex);
        if (context.stopping || !context.indexerCallback) {
            return;
        }
        if (event == MC_INDEXER_PROGRESS && !context.indexerEvents.empty() &&
            context.indexerEvents.back().first == event)
        {
            context.indexerEvents.back().second = updates;
        }
        else {
            context.indexerEvents.emplace_back(event, updates);
        }
        wake = select_wakeup(context);
    }
    if (wake) {
        signal_host(context);
    }
}

void musik::core::ffi::publish_playlist_event(mc_context& context, mc_playlist_event event, int64_t playlistId) {
    bool wake = false;
    {
        std::lock_guard<std::mutex> lock(context.mutex);
        if (context.stopping || !context.playlistChanged || context.libraryStatus.kind != MC_LIBRARY_LOCAL) {
            return;
        }
        context.playlistEvents.emplace_back(event, playlistId);
        wake = select_wakeup(context);
    }
    if (wake) {
        signal_host(context);
    }
}

mc_result MC_CALL mc_context_set_playlist_changed_callback(
    mc_context* context, mc_playlist_changed_fn callback, void* data)
{
    if (!context) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&] {
        std::lock_guard<std::mutex> lock(context->mutex);
        if (!is_dispatcher(*context)) {
            return MC_ERROR_WRONG_THREAD;
        }
        if (context->stopping) {
            return MC_ERROR_BUSY;
        }
        ++context->playlistSubscription;
        context->playlistEvents.clear();
        context->playlistChanged = callback;
        context->playlistChangedData = callback ? data : nullptr;
        return MC_OK;
    });
}

void musik::core::ffi::publish_equalizer_event(mc_context& context) {
    {
        std::lock_guard<std::mutex> lock(context.mutex);
        if (context.stopping || !context.equalizerChanged) {
            return;
        }
        context.equalizerNotificationPending = true;
    }
    wake_context(context);
}

mc_result MC_CALL mc_context_set_equalizer_changed_callback(
    mc_context* context, mc_equalizer_changed_fn callback, void* data)
{
    if (!context) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&] {
        std::lock_guard<std::mutex> lock(context->mutex);
        if (!is_dispatcher(*context)) {
            return MC_ERROR_WRONG_THREAD;
        }
        if (context->stopping) {
            return MC_ERROR_BUSY;
        }
        ++context->equalizerSubscription;
        context->equalizerNotificationPending = false;
        context->equalizerChanged = callback;
        context->equalizerChangedData = callback ? data : nullptr;
        return MC_OK;
    });
}

mc_result MC_CALL mc_context_reconnect_remote_library(mc_context* context) {
    if (!context) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        musik::core::ffi::ContextRuntime* runtime;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (!is_dispatcher(*context)) {
                return MC_ERROR_WRONG_THREAD;
            }
            if (context->stopping) {
                return MC_ERROR_BUSY;
            }
            runtime = context->runtime.get();
        }
        if (!runtime) {
            return MC_ERROR_UNSUPPORTED;
        }
        return runtime->ReconnectRemote();
    });
}

namespace {
    template<typename Operation>
    mc_result with_indexer(const mc_context* context, Operation operation) {
        if (!context) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
            musik::core::ffi::ContextRuntime* runtime;
            {
                std::lock_guard<std::mutex> lock(context->mutex);
                if (!is_dispatcher(*context)) {
                    return MC_ERROR_WRONG_THREAD;
                }
                if (context->stopping) {
                    return MC_ERROR_BUSY;
                }
                runtime = context->runtime.get();
            }
            return runtime ? operation(*runtime) : MC_ERROR_UNSUPPORTED;
        });
    }
}

mc_result MC_CALL mc_indexer_get_state(const mc_context* context, mc_indexer_state* state) {
    if (!state) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::ffi::ContextRuntime& runtime) {
        return runtime.IndexerState(state);
    });
}

mc_result MC_CALL mc_indexer_read_paths(const mc_context* context, mc_indexer_paths_fn read, void* userData) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::ffi::ContextRuntime& runtime) {
        return runtime.IndexerPaths(read, userData);
    });
}

mc_result MC_CALL mc_indexer_add_path(mc_context* context, mc_string_view path) {
    if (!musik::core::ffi::valid_text(path, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::ffi::ContextRuntime& runtime) {
        return runtime.AddIndexerPath(std::string(path.data, path.length));
    });
}

mc_result MC_CALL mc_indexer_remove_path(mc_context* context, mc_string_view path) {
    if (!musik::core::ffi::valid_text(path, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::ffi::ContextRuntime& runtime) {
        return runtime.RemoveIndexerPath(std::string(path.data, path.length));
    });
}

mc_result MC_CALL mc_indexer_schedule(mc_context* context, mc_indexer_scan scan) {
    if (scan > MC_INDEXER_REBUILD) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::ffi::ContextRuntime& runtime) {
        return runtime.ScheduleIndexer(scan);
    });
}

mc_result MC_CALL mc_indexer_cancel(mc_context* context) {
    return with_indexer(context, [](musik::core::ffi::ContextRuntime& runtime) {
        return runtime.CancelIndexer();
    });
}

mc_result MC_CALL mc_indexer_set_callback(mc_context* context, mc_indexer_event_fn callback, void* userData) {
    if (!context) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(context->mutex);
    if (!(context->services & MC_SERVICE_INDEXER)) {
        return MC_ERROR_UNSUPPORTED;
    }
    if (!is_dispatcher(*context)) {
        return MC_ERROR_WRONG_THREAD;
    }
    if (context->stopping && callback) {
        return MC_ERROR_BUSY;
    }

    context->indexerCallback = callback;
    context->indexerData = userData;
    ++context->indexerSubscription;
    context->indexerEvents.clear();
    return MC_OK;
}

mc_result MC_CALL mc_context_select_library(mc_context* context, mc_library_kind kind) {
    if (!context || (kind != MC_LIBRARY_LOCAL && kind != MC_LIBRARY_REMOTE)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        musik::core::ffi::ContextRuntime* runtime;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (!is_dispatcher(*context)) {
                return MC_ERROR_WRONG_THREAD;
            }
            if (context->stopping) {
                return MC_ERROR_BUSY;
            }
            runtime = context->runtime.get();
        }
        return runtime ? runtime->SelectLibrary(kind) : MC_ERROR_UNSUPPORTED;
    });
}

mc_result MC_CALL mc_context_read_library_status(const mc_context* context, mc_library_status_fn read, void* userData) {
    if (!context || !read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return musik::core::ffi::exception_barrier(nullptr, [&] {
        musik::core::ffi::LibraryStatus status;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            status = context->libraryStatus;
        }
        const mc_library_status snapshot {
            status.kind, status.state, status.error, status.configured,
            { status.endpoint.data(), status.endpoint.size() },
            { status.serverVersion.data(), status.serverVersion.size() }
        };
        read(userData, &snapshot);
        return MC_OK;
    });
}

mc_result MC_CALL mc_context_set_library_changed_callback(
    mc_context* context, mc_library_changed_fn callback, void* userData)
{
    if (!context) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(context->mutex);
    if (!is_dispatcher(*context)) {
        return MC_ERROR_WRONG_THREAD;
    }
    if (context->stopping && callback) {
        return MC_ERROR_BUSY;
    }
    ++context->librarySubscription;
    context->libraryChanged = callback;
    context->libraryChangedData = userData;
    context->libraryNotificationPending = false;
    context->librarySelectionChanged = false;
    return MC_OK;
}

namespace musik { namespace core { namespace ffi {

    const TrackQueryLimits &context_track_query_limits(const mc_context &context) {
        return context.track_query_limits;
    }

    mc_result submit_request(mc_context *context, std::function<mc_result()> work,
        RequestCompletion completion, void *user_data, mc_request_id *out_request,
        std::shared_ptr<std::atomic_bool> cancel, RequestEffect effect, const void* owner)
    {
        return exception_barrier(nullptr, [&]() -> mc_result {
            if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
            *out_request = 0;
            if (!context || !work || !completion) return MC_ERROR_INVALID_ARGUMENT;

            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->stopping) return MC_ERROR_BUSY;
            if (context->requests.size() >= max_requests || !context->next_request) return MC_ERROR_QUEUE_FULL;

            /* Reserve the completion slot before publishing the request ID. */
            if (!context->worker.joinable()) context->worker = std::thread(run_worker, std::ref(*context));
            const auto id = context->next_request;
            context->requests.push_back({ id, std::move(work), std::move(completion), user_data,
                RequestState::queued, MC_OK, std::move(cancel), effect, false, owner, {} });

            *out_request = id;
            ++context->next_request;
            context->changed.notify_all();
            return MC_OK;
        });
    }

    mc_result reserve_external_request(mc_context *context, RequestCompletion completion,
        void *user_data, mc_request_id *out_request, std::function<void(mc_result)> &finish, RequestEffect effect,
        std::shared_ptr<std::atomic_bool> cancel, const void* owner, std::function<void()> cancel_external) {
        std::lock_guard<std::mutex> lock(context->mutex);
        if (context->stopping) return MC_ERROR_BUSY;
        if (context->requests.size() >= max_requests || !context->next_request) return MC_ERROR_QUEUE_FULL;

        const auto id = context->next_request;
        const auto external = context->external;
        finish = [external, id](mc_result result) {
            std::unique_lock<std::mutex> lifetime(external->mutex);
            auto *context = external->context;
            if (!context) return;

            std::unique_lock<std::mutex> lock(context->mutex);
            const auto request = find_request(*context, id);
            if (request == context->requests.end() || request->state != RequestState::running) return;

            request->result = result;
            request->state = RequestState::ready;
            const bool notify = select_wakeup(*context);

            /* Shutdown waits for the selected wakeup before destroying host state. */
            lock.unlock();
            lifetime.unlock();
            if (notify) {
                signal_host(*context);
            }
        };

        context->requests.push_back({ id, {}, std::move(completion), user_data, RequestState::running, MC_OK,
            std::move(cancel), effect, true, owner, std::move(cancel_external) });
        *out_request = id;
        ++context->next_request;
        return MC_OK;
    }

    void cancel_owned_requests(mc_context& context, const void* owner) noexcept {
        std::list<Request> requests;
        {
            std::lock_guard<std::mutex> lock(context.mutex);
            if (context.dispatcher != std::thread::id {} && context.dispatcher != std::this_thread::get_id()) {
                failPluginScope("requests cancelled on the wrong thread");
            }
            const auto detach = [&](std::list<Request>& source) {
                for (auto it = source.begin(); it != source.end();) {
                    auto current = it++;
                    if (current->owner != owner) {
                        continue;
                    }
                    if (current->state != RequestState::ready) {
                        current->result = MC_ERROR_CANCELLED;
                        if (current->cancel) {
                            current->cancel->store(true);
                        }
                    }
                    requests.splice(requests.end(), source, current);
                }
            };
            detach(context.requests);
            detach(context.dispatch_batch);
        }
        for (auto& request : requests) {
            try {
                if (request.cancel_external) {
                    request.cancel_external();
                }
                request.completion(request.result, request.user_data);
            }
            catch (...) {
                failPluginScope("request cancellation or completion failed");
            }
        }
    }

} } }

namespace {
    /* owns a dispatch callback without retaining the context or its publisher.
    registration, removal, and delivery share the host thread; Notify may run
    on a producer thread after either owner has begun shutdown. */
    class ExternalNotification final : public musik::core::ffi::ContextNotification {
    public:
        ExternalNotification(std::function<void()> callback, std::function<void()> wake)
            : callback(std::move(callback)), wake(std::move(wake)) {}

        void Notify() override {
            if (this->active.load()) {
                this->pending.store(true);
                this->wake();
            }
        }

        void Deactivate() override {
            this->active.store(false);
            this->pending.store(false);
        }

        void Deliver() override {
            if (this->active.load() && this->pending.exchange(false)) {
                musik::core::ffi::exception_barrier(nullptr, [&] {
                    this->callback();
                    return MC_OK;
                });
            }
        }

    private:
        const std::function<void()> callback;
        const std::function<void()> wake;
        std::atomic<bool> active { true };
        std::atomic<bool> pending { false };
    };
}

mc_result musik::core::ffi::create_context_notification(mc_context* context,
    std::function<void()> callback, std::shared_ptr<ContextNotification>& notification)
{
    if (!context || !callback) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return exception_barrier(nullptr, [&]() -> mc_result {
        std::lock_guard<std::mutex> lock(context->mutex);
        if (context->stopping) {
            return MC_ERROR_BUSY;
        }
        const auto thread = std::this_thread::get_id();
        if (context->dispatcher != std::thread::id {} && context->dispatcher != thread) {
            return MC_ERROR_WRONG_THREAD;
        }
        const auto external = context->external;
        auto wake = [external] {
            std::unique_lock<std::mutex> lifetime(external->mutex);
            auto* context = external->context;
            if (!context) {
                return;
            }
            std::unique_lock<std::mutex> lock(context->mutex);
            const bool notify = select_wakeup(*context);
            lock.unlock();
            lifetime.unlock();
            if (notify) {
                signal_host(*context);
            }
        };
        auto next = std::make_shared<ExternalNotification>(std::move(callback), std::move(wake));
        context->notifications.remove_if([](const std::weak_ptr<ContextNotification>& weak) { return weak.expired(); });
        context->notifications.push_back(next);
        context->dispatcher = thread;
        notification = std::move(next);
        return MC_OK;
    });
}

mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error) {
    using musik::core::ffi::exception_barrier;
    using musik::core::ffi::fail;
    using musik::core::ffi::read_option;

    /* Preserve an existing owned error instead of losing the caller's handle. */
    if (out_error && *out_error) return MC_ERROR_INVALID_ARGUMENT;
    return exception_barrier(out_error, [&]() -> mc_result {
        /* Never overwrite a live owner, including the currently active context. */
        if (!out_context || *out_context) {
            return fail(MC_ERROR_INVALID_ARGUMENT, "Context output must be empty", out_error);
        }

        /* Validate the options prefix and negotiate the ABI version. */
        if (options && options->struct_size < sizeof(options->struct_size)) {
            return fail(MC_ERROR_INVALID_ARGUMENT, "Context options are too small", out_error);
        }

        const auto version = read_option(options, &mc_context_options::requested_abi_version,
            offsetof(mc_context_options, requested_abi_version));
        if (version && (MC_ABI_VERSION_MAJOR(version) != MC_ABI_VERSION_MAJOR(MC_ABI_VERSION) ||
            MC_ABI_VERSION_MINOR(version) > MC_ABI_VERSION_MINOR(MC_ABI_VERSION))) {
            return fail(MC_ERROR_UNSUPPORTED, "Unsupported ABI version", out_error);
        }

        const auto flags = read_option(options, &mc_context_options::flags, offsetof(mc_context_options, flags));
        if (flags & ~(MC_CONTEXT_ENABLE_LOGGING | MC_CONTEXT_START_LOCAL)) {
            return fail(MC_ERROR_UNSUPPORTED, "Unsupported context flags", out_error);
        }

        /* an omitted field preserves the ergonomic full-engine default; explicit
        zero opts out. expansion happens before validating dependent options. */
        mc_services services = MC_SERVICE_ALL;
        if (options && options->struct_size >= offsetof(mc_context_options, services) + sizeof(options->services)) {
            services = options->services;
        }
        if (services & ~MC_SERVICE_ALL) {
            return fail(MC_ERROR_UNSUPPORTED, "Unsupported context services", out_error);
        }
        if (services & (MC_SERVICE_INDEXER | MC_SERVICE_PLAYBACK)) {
            services |= MC_SERVICE_LIBRARY | MC_SERVICE_PLUGINS;
        }
        if ((flags & MC_CONTEXT_ENABLE_LOGGING) && !services) {
            return fail(MC_ERROR_UNSUPPORTED, "Logging requires a context service", out_error);
        }
        if ((flags & MC_CONTEXT_START_LOCAL) && !(services & MC_SERVICE_LIBRARY)) {
            return fail(MC_ERROR_UNSUPPORTED, "Local startup requires the library service", out_error);
        }

        /* Check that the optional plugin directory can be copied. */
        const auto plugins = read_option(options, &mc_context_options::plugin_directory,
            offsetof(mc_context_options, plugin_directory));
        if (!valid_directory(plugins)) {
            return fail(MC_ERROR_INVALID_ARGUMENT, "Invalid directory view", out_error);
        }

        /* Avoid allocating a context when the process slot is already occupied. */
        if (context_active.load()) {
            return fail(MC_ERROR_BUSY, "A context is already active", out_error);
        }

        /* Normalize limits once, including options from older callers. */
        const auto default_limit = read_option(options, &mc_context_options::track_query_default_limit,
            offsetof(mc_context_options, track_query_default_limit));
        const auto max_limit = read_option(options, &mc_context_options::track_query_max_limit,
            offsetof(mc_context_options, track_query_max_limit));
        const auto max_fields = read_option(options, &mc_context_options::track_query_max_fields,
            offsetof(mc_context_options, track_query_max_fields));
        const auto row_cap = static_cast<size_t>(max_limit > 0 ? max_limit : MC_TRACK_QUERY_MAX_LIMIT);
        const auto default_rows = static_cast<size_t>(default_limit > 0 ? default_limit : MC_TRACK_QUERY_DEFAULT_LIMIT);
        const auto field_cap = static_cast<size_t>(max_fields > 0 ? max_fields : MC_TRACK_QUERY_MAX_FIELDS);

        /* Claim the process slot after allocations succeed. */
        std::unique_ptr<mc_context> context(new mc_context{
            std::string(plugins.length ? plugins.data : "", plugins.length),
            read_option(options, &mc_context_options::wakeup, offsetof(mc_context_options, wakeup)),
            read_option(options, &mc_context_options::wakeup_user_data, offsetof(mc_context_options, wakeup_user_data)),
            { std::min(default_rows, row_cap), row_cap, field_cap }, services
        });
        if (context_active.exchange(true)) {
            return fail(MC_ERROR_BUSY, "A context is already active", out_error);
        }

        if (services != MC_SERVICE_NONE) {
            try {
                context->runtime = musik::core::ffi::make_context_runtime();
                if (!context->runtime) {
                    context_active.store(false);
                    return fail(MC_ERROR_UNSUPPORTED, "This build has no managed runtime", out_error);
                }
                context->dispatcher = std::this_thread::get_id();
                const auto log = read_option(options, &mc_context_options::log, offsetof(mc_context_options, log));
                const auto logData = read_option(options, &mc_context_options::log_user_data,
                    offsetof(mc_context_options, log_user_data));
                context->runtime->Start(*context, context->plugin_directory, services, flags, log, logData);
            }
            catch (const std::system_error& error) {
                auto* failed = context.release();
                if (mc_context_destroy(&failed) != MC_OK) {
                    mc_context_destroy(&failed);
                }
                return fail(MC_ERROR_IO, error.what(), out_error);
            }
            catch (const std::invalid_argument& error) {
                auto* failed = context.release();
                if (mc_context_destroy(&failed) != MC_OK) {
                    /* cleanup ran to completion; retry only releases the stopped owner. */
                    mc_context_destroy(&failed);
                }
                return fail(MC_ERROR_INVALID_ARGUMENT, error.what(), out_error);
            }
            catch (...) {
                auto* failed = context.release();
                if (mc_context_destroy(&failed) != MC_OK) {
                    /* cleanup ran to completion; retry only releases the stopped owner. */
                    mc_context_destroy(&failed);
                }
                throw;
            }
        }

        *out_context = context.release();
        return MC_OK;
    });
}

mc_result MC_CALL mc_context_dispatch(mc_context *context) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!context) return MC_ERROR_INVALID_ARGUMENT;

        std::unique_lock<std::mutex> lock(context->mutex);
        if (!is_dispatcher(*context)) {
            return MC_ERROR_WRONG_THREAD;
        }
        if (!enter_dispatch(*context)) {
            return MC_ERROR_BUSY;
        }
        auto* runtime = context->stopping ? nullptr : context->runtime.get();
        mc_result status = MC_OK;
        if (runtime) {
            lock.unlock();
            status = musik::core::ffi::exception_barrier(nullptr, [&] {
                runtime->Dispatch();
                return MC_OK;
            });
            lock.lock();
        }
        deliver_ready(*context, lock);
        lock.unlock();
        if (runtime) {
            runtime->AcknowledgeDispatch();
        }
        return status;
    });
}

namespace {
    mc_result cancelRequest(const std::shared_ptr<ExternalRequests>& external, mc_request_id id) {
        return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
            std::unique_lock lifetime(external->mutex);
            auto* context = external->context;
            if (!context || !id) {
                return MC_ERROR_NOT_FOUND;
            }

            std::unique_lock<std::mutex> lock(context->mutex);
            const auto request = find_request(*context, id);
            if (request == context->requests.end() || request->state == RequestState::ready) {
                return MC_ERROR_NOT_FOUND;
            }

            if (request->state == RequestState::running && request->effect == musik::core::ffi::RequestEffect::Mutation)
            {
                return MC_ERROR_IN_FLIGHT;
            }

            /* copy the hook before changing state; it runs outside the context lock. */
            const auto cancelExternal = request->cancel_external;
            /* Select the terminal result under lock, then notify the host outside it. */
            request->state = RequestState::ready;
            request->result = MC_ERROR_CANCELLED;
            if (request->cancel) {
                request->cancel->store(true);
            }
            const bool notify = select_wakeup(*context);

            lock.unlock();
            lifetime.unlock();
            if (cancelExternal) {
                cancelExternal();
            }
            if (notify) {
                signal_host(*context);
            }
            return MC_OK;
        });
    }

}

mc_result MC_CALL mc_request_cancel(mc_context* context, mc_request_id id) {
    if (!context || !id) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return cancelRequest(context->external, id);
}

std::function<void()> musik::core::ffi::make_request_canceller(mc_context* context, mc_request_id id) {
    return [external = context->external, id] {
        cancelRequest(external, id);
    };
}

mc_result MC_CALL mc_context_shutdown(mc_context *context) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!context) return MC_ERROR_INVALID_ARGUMENT;

        std::unique_lock<std::mutex> lock(context->mutex);
        if (!is_dispatcher(*context)) {
            return MC_ERROR_WRONG_THREAD;
        }
        if (!enter_dispatch(*context)) {
            return MC_ERROR_BUSY;
        }
        context->stopping = true;
        for (const auto& weak : context->notifications) {
            if (const auto notification = weak.lock()) {
                notification->Deactivate();
            }
        }
        if (context->active_cancel) {
            context->active_cancel->store(true);
        }

        /* Preserve terminal results already selected and cancel everything else. */
        for (auto &request : context->requests) {
            if (request.state == RequestState::ready) continue;
            if (request.state == RequestState::running &&
                request.effect == musik::core::ffi::RequestEffect::Mutation)
            {
                if (!request.external) {
                    continue; /* Join the worker and deliver the actual write result. */
                }
                request.result = MC_ERROR_OUTCOME_UNKNOWN;
            }
            else {
                request.result = MC_ERROR_CANCELLED;
                if (request.cancel) {
                    request.cancel->store(true);
                }
            }
            request.state = RequestState::ready;
        }
        context->changed.notify_all();

        /* Let the worker and in-flight wakeups finish before calling host code. */
        lock.unlock();
        if (context->worker.joinable()) {
            context->worker.join();
        }
        const auto stopped = musik::core::ffi::exception_barrier(nullptr, [&] {
            if (context->runtime) {
                context->runtime->Stop();
            }
            return MC_OK;
        });
        lock.lock();
        context->changed.wait(lock, [&] { return context->wakeups_in_flight == 0; });

        deliver_ready(*context, lock);
        return stopped;
    });
}

mc_result MC_CALL mc_context_destroy(mc_context **context) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!context || !*context) return MC_OK;
        const auto status = mc_context_shutdown(*context);
        if (status != MC_OK) return status;

        {
            std::lock_guard<std::mutex> lock((*context)->external->mutex);
            (*context)->external->context = nullptr;
        }

        (*context)->runtime.reset();
        delete *context;
        *context = nullptr;
        context_active.store(false);
        return MC_OK;
    });
}
