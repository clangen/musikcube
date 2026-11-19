#include <musikcore/context.h>

#include "../common/error.h"
#include "../common/primitives.h"
#include "request.h"

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

namespace {
    enum class RequestState { queued, running, ready };

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
          track_query_limits(query_limits), wakeup(signal), wakeup_user_data(signal_data) {}

    std::string plugin_directory;
    const musik::core::ffi::TrackQueryLimits track_query_limits;

    mc_wakeup_fn wakeup;
    void *wakeup_user_data;

    std::mutex mutex;
    std::condition_variable changed;
    std::thread worker;
    std::thread::id dispatcher;

    std::list<Request> requests;
    std::list<Request> dispatch_batch;
    mc_request_id next_request = 1;

    bool dispatching = false;
    bool stopping = false;
    bool wakeup_pending = false;
    size_t wakeups_in_flight = 0;
};

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

        /* Rearm before invoking callbacks so concurrent producers can wake the next dispatch. */
        context.wakeup_pending = false;
        lock.unlock();

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
        return !directory.length ||
            (directory.data && !std::memchr(directory.data, '\0', directory.length));
    }

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

} } }

mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error) {
    using musik::core::ffi::exception_barrier;
    using musik::core::ffi::fail;
    using musik::core::ffi::read_option;

    return exception_barrier(out_error, [&]() -> mc_result {
        /* Clear the output before any validation can fail. */
        if (!out_context) return fail(MC_ERROR_INVALID_ARGUMENT, "Missing context output", out_error);
        *out_context = nullptr;

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

        if (read_option(options, &mc_context_options::flags, offsetof(mc_context_options, flags))) {
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

        *out_context = context.release();
        return MC_OK;
    });
}

mc_result MC_CALL mc_context_dispatch(mc_context *context) {
    return musik::core::ffi::exception_barrier(nullptr, [&]() -> mc_result {
        if (!context) return MC_ERROR_INVALID_ARGUMENT;

        std::unique_lock<std::mutex> lock(context->mutex);
        if (!enter_dispatch(*context)) return MC_ERROR_BUSY;
        deliver_ready(*context, lock);
        return MC_OK;
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
        if (context->worker.joinable()) context->worker.join();
        lock.lock();
        context->changed.wait(lock, [&] { return context->wakeups_in_flight == 0; });

        deliver_ready(*context, lock);
        return MC_OK;
    });
}

void MC_CALL mc_context_destroy(mc_context **context) {
    if (!context || !*context) return;
    if (mc_context_shutdown(*context) != MC_OK) return;

    delete *context;
    *context = nullptr;
    context_active.store(false);
}
