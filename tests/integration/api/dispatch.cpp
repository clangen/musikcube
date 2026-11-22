#include "request.h"
#include <musikcore/online.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

bool context_test_allocation_failures();
bool context_test_notification_dispatch_without_allocations();

namespace {
    struct State {
        mc_context *context = nullptr;
        std::mutex mutex;
        std::condition_variable changed;
        unsigned wakeups = 0;
        std::thread::id wakeup_thread;
        bool hold_wakeup = false;
        bool wakeup_returned = false;
        bool shutdown_returned = false;
        bool work_started = false;
        bool release_work = false;
        unsigned completions = 0;
        std::thread::id completion_thread;
        mc_result result = MC_ERROR_INTERNAL;
        mc_result nested_dispatch = MC_OK;
        std::function<void()> after_completion;
    };
}

/* Count wakeups and optionally hold the producer inside its hook to exercise shutdown synchronization. */
extern "C" void MC_CALL dispatch_test_wakeup(void *data) {
    auto &state = *static_cast<State *>(data);
    std::unique_lock<std::mutex> lock(state.mutex);
    ++state.wakeups;
    state.wakeup_thread = std::this_thread::get_id();
    state.changed.notify_all();
    state.changed.wait(lock, [&] { return !state.hold_wakeup; });
    state.wakeup_returned = true;
}

/* Record terminal delivery and probe nested dispatch before invoking the case-specific callback action. */
extern "C" void MC_CALL dispatch_test_completion(mc_result result, void *data) {
    auto &state = *static_cast<State *>(data);
    ++state.completions;
    state.result = result;
    state.completion_thread = std::this_thread::get_id();
    state.nested_dispatch = mc_context_dispatch(state.context);
    if (state.after_completion) state.after_completion();
}

namespace {
    const auto deadline = std::chrono::seconds(5);

    /* Wait with a deadline until the requested number of producer hooks has entered. */
    bool wait_for_wakeups(State &state, unsigned count) {
        std::unique_lock<std::mutex> lock(state.mutex);
        return state.changed.wait_for(lock, deadline, [&] { return state.wakeups >= count; });
    }

    /* Hold a worker at a known gate so cancellation can win before a terminal result is selected. */
    mc_result block_work(State &state) {
        std::unique_lock<std::mutex> lock(state.mutex);
        state.work_started = true;
        state.changed.notify_all();
        state.changed.wait(lock, [&] { return state.release_work; });
        return MC_OK;
    }

    /* Wait for the worker's gate-entry signal instead of guessing its scheduling with a sleep. */
    bool wait_for_work(State &state) {
        std::unique_lock<std::mutex> lock(state.mutex);
        return state.changed.wait_for(lock, deadline, [&] { return state.work_started; });
    }

    /* Verify dispatcher binding, deferred callback delivery, cancellation, and rejection after shutdown. */
    bool check_dispatch(State &state) {
        using musik::core::ffi::submit_request;
        const auto dispatcher = std::this_thread::get_id();

        /* Bind dispatch to this thread and verify another thread is rejected. */
        if (mc_context_dispatch(state.context) != MC_OK) return false;

        mc_result wrong_thread = MC_OK;
        std::thread other([&] { wrong_thread = mc_context_dispatch(state.context); });
        other.join();

        if (wrong_thread != MC_ERROR_WRONG_THREAD) return false;

        /* Let the worker finish first and verify its wakeup leaves delivery to dispatch. */
        mc_request_id first = 0;
        if (submit_request(state.context, [] { return MC_OK; },
            dispatch_test_completion, &state, &first) != MC_OK || !first) return false;

        {
            std::unique_lock<std::mutex> lock(state.mutex);
            if (!state.changed.wait_for(lock, std::chrono::seconds(5), [&] { return state.wakeups != 0; }) ||
                state.wakeup_thread == dispatcher) return false;
        }

        if (state.completions || mc_request_cancel(state.context, first) != MC_ERROR_NOT_FOUND ||
            mc_context_dispatch(state.context) != MC_OK || state.completions != 1 ||
            state.result != MC_OK || state.completion_thread != dispatcher ||
            state.nested_dispatch != MC_ERROR_BUSY) return false;

        /* Hold the worker at a known point so cancellation wins. */
        mc_request_id second = 0;
        if (submit_request(state.context, [&] { return block_work(state); },
            dispatch_test_completion, &state, &second) != MC_OK ||
            !second || first == second || !wait_for_work(state)) return false;

        if (mc_request_cancel(state.context, second) != MC_OK || state.completions != 1) return false;

        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.release_work = true;
            state.changed.notify_all();
        }
        /* Drain the cancelled completion during shutdown and verify admission stays closed. */
        if (mc_context_shutdown(state.context) != MC_OK || state.completions != 2 ||
            state.result != MC_ERROR_CANCELLED || state.completion_thread != dispatcher ||
            state.nested_dispatch != MC_ERROR_BUSY) return false;

        mc_request_id rejected = second;
        return submit_request(state.context, [] { return MC_OK; },
            dispatch_test_completion, &state, &rejected) == MC_ERROR_BUSY && !rejected &&
            mc_context_dispatch(state.context) == MC_OK && state.completions == 2;
    }

    /* Make work ready during callback delivery to catch a lost rearm that would leave a sleeping host
       with an undelivered completion. */
    bool check_dispatch_rearm(State &state) {
        using musik::core::ffi::submit_request;
        mc_request_id first = 0;
        mc_request_id second = 0;
        bool second_ready = false;

        /* Make the next completion ready while the first callback is still running. */
        state.after_completion = [&] {
            if (state.completions != 1) return;
            second_ready = submit_request(state.context, [] { return MC_OK; },
                dispatch_test_completion, &state, &second) == MC_OK &&
                wait_for_wakeups(state, 2);
        };

        const bool first_batch = submit_request(state.context, [] { return MC_OK; },
            dispatch_test_completion, &state, &first) == MC_OK &&
            wait_for_wakeups(state, 1) &&
            mc_context_dispatch(state.context) == MC_OK && second_ready && state.completions == 1;
        state.after_completion = nullptr;

        return first_batch && first != second &&
            mc_context_dispatch(state.context) == MC_OK && state.completions == 2;
    }

    /* Retain terminal slots until dispatch so finished workers cannot bypass the admission bound or
       generate redundant host wakeups. */
    bool check_queue_capacity(State &state) {
        using musik::core::ffi::submit_request;
        mc_request_id first = 0;
        if (submit_request(state.context, [&] { return block_work(state); },
            dispatch_test_completion, &state, &first) != MC_OK || !wait_for_work(state)) return false;

        /* Fill the queue without dispatching so each accepted request retains its slot. */
        unsigned accepted = 1;
        mc_request_id second = 0;
        for (; accepted < 256; ++accepted) {
            mc_request_id id = first;
            const auto result = submit_request(state.context, [] { return MC_OK; },
                dispatch_test_completion, &state, &id);
            if (result == MC_ERROR_QUEUE_FULL && !id) break;
            if (result != MC_OK || !id) return false;
            if (accepted == 1) second = id;
        }
        if (accepted == 256 || !second) return false;

        /* Two terminal results should post one wakeup until dispatch rearms it. */
        if (mc_request_cancel(state.context, first) != MC_OK ||
            mc_request_cancel(state.context, second) != MC_OK) return false;
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            if (state.wakeups != 1) return false;
            state.release_work = true;
            state.changed.notify_all();
        }

        return mc_context_shutdown(state.context) == MC_OK &&
            state.completions == accepted &&
            mc_context_dispatch(state.context) == MC_OK && state.completions == accepted;
    }

    /* Verify shutdown waits for an in-flight wakeup hook before returning and drains cancellation once. */
    bool check_shutdown_wakeup(State &state) {
        using musik::core::ffi::submit_request;
        state.hold_wakeup = true;
        mc_request_id request = 0;
        if (submit_request(state.context, [&] { return block_work(state); },
            dispatch_test_completion, &state, &request) != MC_OK || !wait_for_work(state)) return false;

        /* Cancellation runs the hook on a thread that shutdown cannot join as its worker. */
        mc_result cancelled = MC_ERROR_INTERNAL;
        std::thread canceller([&] { cancelled = mc_request_cancel(state.context, request); });
        const bool hook_entered = wait_for_wakeups(state, 1);
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.release_work = true;
            state.changed.notify_all();
        }

        /* Hold the hook open while shutdown runs, then let the worker exit. */
        bool returned_while_held = false;
        std::thread release([&] {
            std::unique_lock<std::mutex> lock(state.mutex);
            returned_while_held = state.changed.wait_for(lock, std::chrono::milliseconds(50),
                [&] { return state.shutdown_returned; });
            state.hold_wakeup = false;
            state.changed.notify_all();
        });

        const auto result = mc_context_shutdown(state.context);
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.shutdown_returned = true;
            state.changed.notify_all();
        }
        release.join();
        canceller.join();

        return hook_entered && cancelled == MC_OK && result == MC_OK &&
            !returned_while_held && state.wakeup_returned &&
            state.completions == 1 && state.result == MC_ERROR_CANCELLED;
    }
}

/* Give each dispatch case a fresh context and release held gates before cleanup, including on failure. */
static bool run_check(const char *name, bool (*check)(State &)) {
    State state;
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_NONE;
    options.wakeup = dispatch_test_wakeup;
    options.wakeup_user_data = &state;
    if (mc_context_create(&options, &state.context, nullptr) != MC_OK) return false;
    const bool passed = check(state);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.release_work = true;
        state.hold_wakeup = false;
        state.changed.notify_all();
    }
    mc_context_destroy(&state.context);
    if (!passed || state.context) {
        std::fprintf(stderr, "async dispatch contract failed: %s\n", name);
        return false;
    }
    return true;
}

/* Verify late external replies cannot redeliver cancelled work or access a destroyed context. */
static bool check_external_reply(State &state) {
    mc_request_id request = 0;
    std::function<void(mc_result)> finish;
    if (musik::core::ffi::reserve_external_request(state.context, dispatch_test_completion,
        &state, &request, finish) != MC_OK) return false;
    if (mc_request_cancel(state.context, request) != MC_OK ||
        mc_context_dispatch(state.context) != MC_OK) return false;

    finish(MC_OK);
    if (mc_context_dispatch(state.context) != MC_OK || state.completions != 1 ||
        state.result != MC_ERROR_CANCELLED) return false;

    if (musik::core::ffi::reserve_external_request(state.context, dispatch_test_completion,
        &state, &request, finish) != MC_OK) return false;
    mc_context_destroy(&state.context);
    finish(MC_OK);
    return state.completions == 2 && state.result == MC_ERROR_CANCELLED;
}

/* Verify a callback cannot shut down or destroy its own active context, preserving its handle for retry. */
static bool check_callback_lifecycle(State &state) {
    mc_request_id request = 0;
    std::function<void(mc_result)> finish;
    if (musik::core::ffi::reserve_external_request(state.context, dispatch_test_completion,
        &state, &request, finish) != MC_OK) return false;

    bool preserved = false;
    bool rejected = false;
    state.after_completion = [&] {
        const auto original = state.context;
        rejected = mc_context_shutdown(original) == MC_ERROR_BUSY;
        rejected = rejected && mc_context_destroy(&state.context) == MC_ERROR_BUSY;
        preserved = state.context == original;
    };
    finish(MC_OK);
    if (mc_context_dispatch(state.context) != MC_OK || !preserved || !rejected || state.completions != 1) return false;
    state.after_completion = nullptr;
    return mc_context_shutdown(state.context) == MC_OK && state.completions == 1;
}

/* Cancel a peer from a callback and verify the peer's later success cannot replace cancellation. */
static bool check_callback_cancellation(State &state) {
    using musik::core::ffi::reserve_external_request;
    mc_request_id first = 0;
    mc_request_id second = 0;
    std::function<void(mc_result)> finishFirst;
    std::function<void(mc_result)> finishSecond;
    if (reserve_external_request(state.context, dispatch_test_completion, &state, &first, finishFirst) != MC_OK ||
        reserve_external_request(state.context, dispatch_test_completion, &state, &second, finishSecond) != MC_OK) return false;
    mc_result cancelled = MC_ERROR_INTERNAL;
    state.after_completion = [&] { cancelled = mc_request_cancel(state.context, second); };
    finishFirst(MC_OK);
    if (mc_context_dispatch(state.context) != MC_OK || state.completions != 1 || cancelled != MC_OK) return false;
    state.after_completion = nullptr;
    finishSecond(MC_OK);
    return mc_context_dispatch(state.context) == MC_OK && state.completions == 2 &&
        state.result == MC_ERROR_CANCELLED && mc_context_dispatch(state.context) == MC_OK && state.completions == 2;
}

/* Start cancellation and completion together to expose double delivery; repeat late replies after either
   winner to check terminal state stays final. */
static bool check_terminal_races(State &state) {
    for (unsigned iteration = 0; iteration < 64; ++iteration) {
        mc_request_id request = 0;
        std::function<void(mc_result)> finish;
        if (musik::core::ffi::reserve_external_request(state.context, dispatch_test_completion,
            &state, &request, finish) != MC_OK) return false;
        std::mutex gate;
        std::condition_variable changed;
        unsigned waiting = 0;
        bool release = false;
        /* Release both threads from one gate; a sleep could consistently give
           one side a head start and conceal the competing terminal selection. */
        const auto start = [&] {
            std::unique_lock<std::mutex> lock(gate);
            ++waiting;
            changed.notify_all();
            changed.wait(lock, [&] { return release; });
        };
        mc_result cancelled = MC_ERROR_INTERNAL;
        std::thread producer([&] { start(); finish(MC_OK); finish(MC_ERROR_IO); });
        std::thread canceller([&] { start(); cancelled = mc_request_cancel(state.context, request); });
        {
            std::unique_lock<std::mutex> lock(gate);
            changed.wait(lock, [&] { return waiting == 2; });
            release = true;
            changed.notify_all();
        }
        producer.join();
        canceller.join();
        /* Either result is valid, but Cancel's return value must agree with
           the selected terminal result and only one callback may consume it. */
        if (mc_context_dispatch(state.context) != MC_OK || state.completions != iteration + 1 ||
            (cancelled == MC_OK ? state.result != MC_ERROR_CANCELLED :
                cancelled != MC_ERROR_NOT_FOUND || state.result != MC_OK)) return false;
        /* Duplicate/late producer reports cannot resurrect a delivered slot. */
        finish(MC_OK);
        if (mc_context_dispatch(state.context) != MC_OK || state.completions != iteration + 1) return false;
    }
    return true;
}

static bool check_shutdown_cancellable_worker(State& state) {
    using musik::core::ffi::submit_request;
    auto cancelled = std::make_shared<std::atomic_bool>(false);
    std::atomic_bool exited { false };
    mc_request_id request = 0;
    if (submit_request(state.context, [&] {
        {
            std::lock_guard<std::mutex> lock(state.mutex);
            state.work_started = true;
            state.changed.notify_all();
        }
        const auto end = std::chrono::steady_clock::now() + deadline;
        while (!cancelled->load() && std::chrono::steady_clock::now() < end) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        exited.store(cancelled->load());
        return MC_OK;
    }, dispatch_test_completion,
        &state, &request, cancelled) != MC_OK)
    {
        return false;
    }

    const bool started = wait_for_work(state);
    const auto shutdown = mc_context_shutdown(state.context);
    return started && shutdown == MC_OK && exited.load() && state.completions == 1 && state.result == MC_ERROR_CANCELLED;
}

static bool check_mutation_cancellation(State& state) {
    using namespace musik::core::ffi;
    mc_request_id request = 0;
    if (submit_request(state.context, [&] { return block_work(state); },
        dispatch_test_completion, &state, &request, {}, RequestEffect::Mutation) != MC_OK)
    {
        return false;
    }

    const bool running = wait_for_work(state);
    const auto cancellation = mc_request_cancel(state.context, request);
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.release_work = true;
        state.changed.notify_all();
    }
    if (!running || cancellation != MC_ERROR_IN_FLIGHT || !wait_for_wakeups(state, 1) ||
        mc_context_dispatch(state.context) != MC_OK || state.result != MC_OK || state.completions != 1)
    {
        return false;
    }

    std::function<void(mc_result)> finish;
    if (reserve_external_request(state.context, dispatch_test_completion, &state, &request, finish,
        RequestEffect::Mutation) != MC_OK || mc_request_cancel(state.context, request) != MC_ERROR_IN_FLIGHT)
    {
        return false;
    }

    const auto shutdown = mc_context_shutdown(state.context);
    finish(MC_OK);
    return shutdown == MC_OK && state.result == MC_ERROR_OUTCOME_UNKNOWN && state.completions == 2;
}

static bool check_online_rejection(State& state) {
    mc_request_id request = 42;
    auto text = [](mc_result, mc_string_view, void*) {};
    auto link = [](mc_result, mc_string_view, mc_string_view, void*) {};
    if (mc_lyrics_find_async(state.context, {}, {}, text, nullptr, &request) != MC_ERROR_INVALID_ARGUMENT || request) {
        return false;
    }
    request = 42;
    if (mc_lastfm_complete_link_async(state.context, {}, text, nullptr, &request) != MC_ERROR_INVALID_ARGUMENT || request) {
        return false;
    }
    request = 42;
    return mc_lastfm_request_link_async(nullptr, link, nullptr, &request) == MC_ERROR_INVALID_ARGUMENT && !request;
}

static bool check_playlist_dispatch(State& state) {
    using namespace musik::core::ffi;
    using Event = std::pair<mc_playlist_event, int64_t>;
    std::vector<Event> events;
    auto& context = *state.context;
    if (mc_context_set_playlist_changed_callback(&context, [](void* data, mc_playlist_event event, int64_t id) {
        static_cast<std::vector<Event>*>(data)->emplace_back(event, id);
    }, &events) != MC_OK)
    {
        return false;
    }

    const auto select = [&](mc_library_kind kind) {
        LibraryStatus status;
        status.kind = kind;
        return publish_library_selection(context, {}, std::move(status), true) == MC_OK;
    };

    constexpr int64_t playlist = 7;
    const bool remote = select(MC_LIBRARY_REMOTE);
    publish_playlist_event(context, MC_PLAYLIST_CREATED, playlist);
    const bool remoteSuppressed = mc_context_dispatch(&context) == MC_OK && events.empty();

    const bool local = select(MC_LIBRARY_LOCAL);
    publish_playlist_event(context, MC_PLAYLIST_RENAMED, playlist);
    publish_playlist_event(context, MC_PLAYLIST_CONTENTS_CHANGED, playlist);
    const bool ordered = events.empty() && mc_context_dispatch(&context) == MC_OK &&
        events == std::vector<Event> {
            { MC_PLAYLIST_RENAMED, playlist }, { MC_PLAYLIST_CONTENTS_CHANGED, playlist }
        };

    publish_playlist_event(context, MC_PLAYLIST_DELETED, playlist);
    const bool switched = select(MC_LIBRARY_REMOTE);
    const bool staleSuppressed = mc_context_dispatch(&context) == MC_OK && events.size() == 2;

    return mc_context_set_playlist_changed_callback(&context, nullptr, nullptr) == MC_OK &&
        remote && remoteSuppressed && local && ordered && switched && staleSuppressed;
}

/* Run each lifecycle case independently so context/thread state cannot leak between scenarios. */
int main() {
    return context_test_allocation_failures() &&
        context_test_notification_dispatch_without_allocations() &&
        run_check("completion and cancellation", check_dispatch) &&
        run_check("dispatch rearm", check_dispatch_rearm) &&
        run_check("queue capacity and wakeup coalescing", check_queue_capacity) &&
        run_check("shutdown with a wakeup in flight", check_shutdown_wakeup) &&
        run_check("shutdown and destroy during callback", check_callback_lifecycle) &&
        run_check("cancel peer request during callback", check_callback_cancellation) &&
        run_check("completion and cancellation terminal selection races", check_terminal_races) &&
        run_check("external reply after cancellation and destruction", check_external_reply) &&
        run_check("shutdown cancels and joins active work", check_shutdown_cancellable_worker) &&
        run_check("mutation cancellation", check_mutation_cancellation) &&
        run_check("online request rejection", check_online_rejection) &&
        run_check("playlist order and selection changes", check_playlist_dispatch) ? 0 : 1;
}
