#include "host-wakeup.h"
#include <chrono>
#include <condition_variable>
#include <mutex>

struct test_host_event {
    std::mutex mutex;
    std::condition_variable changed;
    bool pending = false;
};

/* Allocate a private coalescing host event; return null instead of throwing across its C interface. */
test_host_event* test_host_event_create() {
    try {
        return new test_host_event;
    }
    catch (...) {
        return nullptr;
    }
}
/* Release the event after its context has stopped invoking the wakeup hook. */
void test_host_event_destroy(test_host_event* event) { delete event; }
/* Latch one pending wakeup and notify the sleeping host; multiple producer signals may coalesce. */
void MC_CALL test_host_event_signal(void* data) {
    auto& event = *static_cast<test_host_event*>(data);
    std::lock_guard<std::mutex> lock(event.mutex);
    event.pending = true;
    event.changed.notify_one();
}
/* Consume a latched wakeup so a producer signal arriving before the wait is not lost and tests need no
   polling sleeps. */
int test_host_event_wait(test_host_event* event) {
    constexpr unsigned defaultTimeoutMs = 5000;
    return test_host_event_wait_for(event, defaultTimeoutMs);
}

int test_host_event_wait_for(test_host_event* event, unsigned timeout_ms) {
    std::unique_lock<std::mutex> lock(event->mutex);
    if (!event->changed.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] { return event->pending; })) return 0;
    event->pending = false;
    return 1;
}
