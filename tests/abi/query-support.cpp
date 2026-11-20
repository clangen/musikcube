#include <musikcore/context.h>

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <functional>

namespace {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned wakeups = 0;
    bool released = false;
}

/* Count producer signals and optionally hold delivery so tests can inspect state before dispatch. */
extern "C" void MC_CALL query_test_wakeup(void *) {
    std::unique_lock<std::mutex> lock(mutex);
    ++wakeups;
    changed.notify_all();
    changed.wait(lock, [] { return released; });
}

/* Wait for a specific signal count with a deadline; observing a wakeup does not dispatch callbacks. */
extern "C" int query_test_wait_for_wakeup(unsigned count) {
    std::unique_lock<std::mutex> lock(mutex);
    return changed.wait_for(lock, std::chrono::seconds(5), [count] { return wakeups >= count; });
}

/* Sample the signal count under its mutex so a case can wait relative to its own starting point. */
extern "C" unsigned query_test_wakeup_count() {
    std::lock_guard<std::mutex> lock(mutex);
    return wakeups;
}

/* Release the producer hook after the test has reached its intended synchronization point. */
extern "C" void query_test_release_wakeup(void) {
    std::lock_guard<std::mutex> lock(mutex);
    released = true;
    changed.notify_all();
}

/* Drain callbacks until an operation predicate holds, waiting on wakeups without polling or lost rearm
   signals. */
bool query_test_dispatch_until(mc_context* context, const std::function<bool()>& ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!ready()) {
        const auto observed = query_test_wakeup_count();
        if (mc_context_dispatch(context) != MC_OK) return false;
        if (ready()) return true;
        std::unique_lock<std::mutex> lock(mutex);
        if (!changed.wait_until(lock, deadline, [&] { return wakeups != observed; })) return false;
    }
    return true;
}
