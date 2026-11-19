#include "request.h"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

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

extern "C" void MC_CALL dispatch_test_wakeup(void *data) {
    auto &state = *static_cast<State *>(data);
    std::unique_lock<std::mutex> lock(state.mutex);
    ++state.wakeups;
    state.wakeup_thread = std::this_thread::get_id();
    state.changed.notify_all();
    state.changed.wait(lock, [&] { return !state.hold_wakeup; });
    state.wakeup_returned = true;
}

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

    bool wait_for_wakeups(State &state, unsigned count) {
        std::unique_lock<std::mutex> lock(state.mutex);
        return state.changed.wait_for(lock, deadline, [&] { return state.wakeups >= count; });
    }

    mc_result block_work(State &state) {
        std::unique_lock<std::mutex> lock(state.mutex);
        state.work_started = true;
        state.changed.notify_all();
        state.changed.wait(lock, [&] { return state.release_work; });
        return MC_OK;
    }

    bool wait_for_work(State &state) {
        std::unique_lock<std::mutex> lock(state.mutex);
        return state.changed.wait_for(lock, deadline, [&] { return state.work_started; });
    }

    bool check_dispatch(State &state) {
        using musik::core::ffi::submit_request;
        const auto dispatcher = std::this_thread::get_id();

        /* Bind dispatch to this thread and verify another thread is rejected. */
        if (mc_context_dispatch(state.context) != MC_OK) return false;

        mc_result wrong_thread = MC_OK;
        std::thread other([&] { wrong_thread = mc_context_dispatch(state.context); });
        other.join();

        if (wrong_thread != MC_ERROR_BUSY) return false;

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
            if (result == MC_ERROR_BUSY && !id) break;
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

static bool run_check(const char *name, bool (*check)(State &)) {
    State state;
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
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

int main() {
    return run_check("completion and cancellation", check_dispatch) &&
        run_check("dispatch rearm", check_dispatch_rearm) &&
        run_check("queue capacity and wakeup coalescing", check_queue_capacity) &&
        run_check("shutdown with a wakeup in flight", check_shutdown_wakeup) &&
        run_check("external reply after cancellation and destruction", check_external_reply) ? 0 : 1;
}
