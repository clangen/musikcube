#include <musikcore/musikcore.h>

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace {
    std::mutex mutex;
    std::condition_variable changed;
    unsigned wakeups = 0;
    bool released = false;
}

extern "C" void MC_CALL query_test_wakeup(void *) {
    std::unique_lock<std::mutex> lock(mutex);
    ++wakeups;
    changed.notify_all();
    changed.wait(lock, [] { return released; });
}

extern "C" int query_test_wait_for_wakeup(unsigned count) {
    std::unique_lock<std::mutex> lock(mutex);
    return changed.wait_for(lock, std::chrono::seconds(5), [count] { return wakeups >= count; });
}

extern "C" void query_test_release_wakeup(void) {
    std::lock_guard<std::mutex> lock(mutex);
    released = true;
    changed.notify_all();
}
