#include <musikcore/context.h>
#include <musikcore/equalizer.h>

#include "../common/error.h"
#include "../common/primitives.h"
#include "request.h"
#include <musikcore/library/IIndexer.h>

#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
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
    };
}

struct mc_context {
    mc_context(std::string plugins, mc_wakeup_fn signal, void *signal_data,
        musik::core::ffi::TrackQueryLimits query_limits)
        : plugin_directory(std::move(plugins)),
          track_query_limits(query_limits), wakeup(signal), wakeup_user_data(signal_data) {
        external->context = this;
    }

    std::string plugin_directory;
    const musik::core::ffi::TrackQueryLimits track_query_limits;

    mc_wakeup_fn wakeup;
    void *wakeup_user_data;

    mutable std::mutex mutex;
    std::condition_variable changed;
    std::thread worker;
    std::thread::id dispatcher;
    std::shared_ptr<const LibraryIdentity> library;
    musik::core::ffi::LibraryStatus libraryStatus;
    std::unique_ptr<musik::core::ffi::ContextRuntime> runtime;
    std::shared_ptr<musik::core::audio::PlaybackService> ownedPlayback;
    std::function<mc_result(mc_library_kind)> selectLibrary;
    std::function<void()> reconnectRemote;
    musik::core::IIndexer* indexer = nullptr;
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
    bool libraryNotificationPending = false;
    bool librarySelectionChanged = false;
    musik::core::audio::PlaybackService* playback = nullptr;
    musik::core::ffi::OpenPlayback open_playback = nullptr;

    std::list<Request> requests;
    std::list<Request> dispatch_batch;
    std::shared_ptr<ExternalRequests> external = std::make_shared<ExternalRequests>();
    mc_request_id next_request = 1;

    bool dispatching = false;
    bool stopping = false;
    bool wakeup_pending = false;
    size_t wakeups_in_flight = 0;
};

std::shared_ptr<const LibraryIdentity> musik::core::ffi::context_library(const mc_context& context) {
    std::lock_guard<std::mutex> lock(context.mutex);
    return context.library;
}

mc_result musik::core::ffi::attach_context_playback(
    mc_context& context, audio::PlaybackService& playback, OpenPlayback open)
{
    std::lock_guard<std::mutex> lock(context.mutex);
    if (context.stopping || context.playback) {
        return MC_ERROR_BUSY;
    }
    context.playback = &playback;
    context.open_playback = open;
    return MC_OK;
}

void musik::core::ffi::own_context_playback(
    mc_context& context, std::shared_ptr<audio::PlaybackService> playback)
{
    std::lock_guard<std::mutex> lock(context.mutex);
    context.ownedPlayback = std::move(playback);
}

mc_result MC_CALL mc_context_get_playback(const mc_context* context, mc_playback** out_playback) {
    if (!context || !out_playback || *out_playback) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        musik::core::audio::PlaybackService* playback;
        musik::core::ffi::OpenPlayback open;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->stopping) {
                return MC_ERROR_BUSY;
            }
            playback = context->playback;
            open = context->open_playback;
        }
        return open ? open(*playback, out_playback) : MC_ERROR_UNSUPPORTED;
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

            lock.unlock();
            const auto result = musik::core::ffi::exception_barrier(nullptr, work);
            work = nullptr;
            lock.lock();

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

        /* Rearm before invoking callbacks so concurrent producers can wake the next dispatch. */
        context.wakeup_pending = false;
        lock.unlock();

        if (notifyLibrary) {
            musik::core::ffi::exception_barrier(nullptr, [&] {
                notifyLibrary(libraryData, selectionChanged);
                return MC_OK;
            });
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
        for (const auto &request : ready) {
            musik::core::ffi::exception_barrier(nullptr, [&] {
                request.completion(request.result, request.user_data);
                return MC_OK;
            });
        }
        ready.clear();

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
    mc_result update_library(mc_context& context, std::shared_ptr<const LibraryIdentity> library,
        musik::core::ffi::LibraryStatus status, bool managed, bool selectionChanged)
    {
        bool wake = false;
        {
            std::lock_guard<std::mutex> lock(context.mutex);
            if (context.stopping) {
                return MC_ERROR_BUSY;
            }
            if (!managed && context.selectLibrary) {
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
    std::shared_ptr<const LibraryIdentity> library, LibraryStatus status)
{
    return update_library(context, std::move(library), std::move(status), false, true);
}

mc_result musik::core::ffi::publish_library_selection(mc_context& context,
    std::shared_ptr<const LibraryIdentity> library, LibraryStatus status, bool selectionChanged)
{
    return update_library(context, std::move(library), std::move(status), true, selectionChanged);
}

void musik::core::ffi::attach_library_selector(mc_context& context,
    std::function<mc_result(mc_library_kind)> selector, std::function<void()> reconnect, IIndexer& indexer)
{
    std::lock_guard<std::mutex> lock(context.mutex);
    context.selectLibrary = std::move(selector);
    context.reconnectRemote = std::move(reconnect);
    context.indexer = &indexer;
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
        if (context->stopping || !is_dispatcher(*context)) {
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
        if (context->stopping || !is_dispatcher(*context)) {
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
        std::function<void()> reconnect;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->stopping || !is_dispatcher(*context)) {
                return MC_ERROR_BUSY;
            }
            reconnect = context->reconnectRemote;
        }
        if (!reconnect) {
            return MC_ERROR_UNSUPPORTED;
        }
        reconnect();
        return MC_OK;
    });
}

namespace {
    template<typename Operation>
    mc_result with_indexer(const mc_context* context, Operation operation) {
        if (!context) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
            musik::core::IIndexer* indexer;
            {
                std::lock_guard<std::mutex> lock(context->mutex);
                indexer = context->indexer;
            }
            if (!indexer) {
                return MC_ERROR_UNSUPPORTED;
            }
            operation(*indexer);
            return MC_OK;
        });
    }
}

mc_result MC_CALL mc_indexer_get_state(const mc_context* context, mc_indexer_state* state) {
    if (!state) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::IIndexer& indexer) {
        *state = static_cast<mc_indexer_state>(indexer.GetState());
    });
}

mc_result MC_CALL mc_indexer_read_paths(const mc_context* context, mc_indexer_paths_fn read, void* userData) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::IIndexer& indexer) {
        std::vector<std::string> paths;
        indexer.GetPaths(paths);
        std::vector<mc_string_view> views;
        views.reserve(paths.size());
        for (const auto& path : paths) {
            views.push_back({ path.data(), path.size() });
        }
        read(userData, views.data(), views.size());
    });
}

mc_result MC_CALL mc_indexer_add_path(mc_context* context, mc_string_view path) {
    if (!musik::core::ffi::valid_text(path, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::IIndexer& indexer) {
        indexer.AddPath(std::string(path.data, path.length));
    });
}

mc_result MC_CALL mc_indexer_remove_path(mc_context* context, mc_string_view path) {
    if (!musik::core::ffi::valid_text(path, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::IIndexer& indexer) {
        indexer.RemovePath(std::string(path.data, path.length));
    });
}

mc_result MC_CALL mc_indexer_schedule(mc_context* context, mc_indexer_scan scan) {
    if (scan > MC_INDEXER_REBUILD) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    return with_indexer(context, [&](musik::core::IIndexer& indexer) {
        indexer.Schedule(static_cast<musik::core::IIndexer::SyncType>(scan));
    });
}

mc_result MC_CALL mc_indexer_set_callback(mc_context* context, mc_indexer_event_fn callback, void* userData) {
    if (!context) {
        return MC_ERROR_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(context->mutex);
    if (!context->indexer) {
        return MC_ERROR_UNSUPPORTED;
    }
    if (!is_dispatcher(*context) || (context->stopping && callback)) {
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
        std::function<mc_result(mc_library_kind)> select;
        {
            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->stopping || !is_dispatcher(*context)) {
                return MC_ERROR_BUSY;
            }
            select = context->selectLibrary;
        }
        return select ? select(kind) : MC_ERROR_UNSUPPORTED;
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
    if (!is_dispatcher(*context) || (context->stopping && callback)) {
        return MC_ERROR_BUSY;
    }
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
        RequestCompletion completion, void *user_data, mc_request_id *out_request) {
        return exception_barrier(nullptr, [&]() -> mc_result {
            if (!out_request) return MC_ERROR_INVALID_ARGUMENT;
            *out_request = 0;
            if (!context || !work || !completion) return MC_ERROR_INVALID_ARGUMENT;

            std::lock_guard<std::mutex> lock(context->mutex);
            if (context->stopping || context->requests.size() >= max_requests ||
                !context->next_request) return MC_ERROR_BUSY;

            /* Reserve the completion slot before publishing the request ID. */
            if (!context->worker.joinable()) context->worker = std::thread(run_worker, std::ref(*context));
            const auto id = context->next_request;
            context->requests.push_back({ id, std::move(work), std::move(completion), user_data,
                RequestState::queued, MC_OK });

            *out_request = id;
            ++context->next_request;
            context->changed.notify_all();
            return MC_OK;
        });
    }

    mc_result reserve_external_request(mc_context *context, RequestCompletion completion,
        void *user_data, mc_request_id *out_request, std::function<void(mc_result)> &finish) {
        std::lock_guard<std::mutex> lock(context->mutex);
        if (context->stopping || context->requests.size() >= max_requests ||
            !context->next_request) return MC_ERROR_BUSY;

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
            if (notify) signal_host(*context);
        };

        context->requests.push_back({ id, {}, std::move(completion), user_data, RequestState::running, MC_OK });
        *out_request = id;
        ++context->next_request;
        return MC_OK;
    }

} } }

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
        if (flags & ~MC_CONTEXT_MANAGED_RUNTIME) {
            return fail(MC_ERROR_UNSUPPORTED, "Unsupported context flags", out_error);
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
            { std::min(default_rows, row_cap), row_cap, field_cap }
        });
        if (context_active.exchange(true)) {
            return fail(MC_ERROR_BUSY, "A context is already active", out_error);
        }

        if (flags & MC_CONTEXT_MANAGED_RUNTIME) {
            try {
                context->runtime = musik::core::ffi::make_context_runtime();
                if (!context->runtime) {
                    context_active.store(false);
                    return fail(MC_ERROR_UNSUPPORTED, "This build has no managed runtime", out_error);
                }
                context->dispatcher = std::this_thread::get_id();
                context->runtime->Start(*context);
            }
            catch (...) {
                auto* failed = context.release();
                mc_context_destroy(&failed);
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
        if (!enter_dispatch(*context)) return MC_ERROR_BUSY;
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

mc_result MC_CALL mc_request_cancel(mc_context *context, mc_request_id id) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!context || !id) return MC_ERROR_INVALID_ARGUMENT;

        std::unique_lock<std::mutex> lock(context->mutex);
        const auto request = find_request(*context, id);
        if (request == context->requests.end() || request->state == RequestState::ready) {
            return MC_ERROR_NOT_FOUND;
        }

        /* Select the terminal result under lock, then notify the host outside it. */
        request->state = RequestState::ready;
        request->result = MC_ERROR_CANCELLED;
        const bool notify = select_wakeup(*context);

        lock.unlock();
        if (notify) signal_host(*context);
        return MC_OK;
    });
}

mc_result MC_CALL mc_context_shutdown(mc_context *context) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!context) return MC_ERROR_INVALID_ARGUMENT;

        std::unique_lock<std::mutex> lock(context->mutex);
        if (!enter_dispatch(*context)) return MC_ERROR_BUSY;
        context->stopping = true;

        /* Preserve terminal results already selected and cancel everything else. */
        for (auto &request : context->requests) {
            if (request.state == RequestState::ready) continue;
            request.state = RequestState::ready;
            request.result = MC_ERROR_CANCELLED;
        }
        context->changed.notify_all();

        /* Let the worker and in-flight wakeups finish before calling host code. */
        lock.unlock();
        if (context->runtime) {
            context->runtime->Stop();
        }
        if (context->worker.joinable()) context->worker.join();
        lock.lock();
        context->changed.wait(lock, [&] { return context->wakeups_in_flight == 0; });

        deliver_ready(*context, lock);
        return MC_OK;
    });
}

void MC_CALL mc_context_destroy(mc_context **context) {
    musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!context || !*context) return MC_OK;
        const auto status = mc_context_shutdown(*context);
        if (status != MC_OK) return status;

        {
            std::lock_guard<std::mutex> lock((*context)->external->mutex);
            (*context)->external->context = nullptr;
        }

        /* Detach backend and indexer signal sources before destroying context state. */
        std::function<mc_result(mc_library_kind)> selector;
        {
            std::lock_guard<std::mutex> lock((*context)->mutex);
            selector.swap((*context)->selectLibrary);
        }
        selector = nullptr;
        (*context)->ownedPlayback.reset();
        (*context)->runtime.reset();
        delete *context;
        *context = nullptr;
        context_active.store(false);
        return MC_OK;
    });
}
