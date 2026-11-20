#include "host-wakeup.h"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <new>

struct test_host_event {
    std::mutex mutex;
    std::condition_variable changed;
    bool pending = false;
};

/* Allocate a private coalescing host event; return null instead of throwing across its C interface. */
test_host_event* test_host_event_create() { return new (std::nothrow) test_host_event; }
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
    std::unique_lock<std::mutex> lock(event->mutex);
    if (!event->changed.wait_for(lock, std::chrono::seconds(5), [&] { return event->pending; })) return 0;
    event->pending = false;
    return 1;
}
