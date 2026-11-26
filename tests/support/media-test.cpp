#include "media-test.h"
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {
    int (*control)(int) = nullptr;
    std::thread worker;
    std::mutex mutex;
    std::condition_variable changed;
    bool done = false;
    int result = -1;
}

extern "C" int media_fixture_load(const char* path) {
#ifdef _WIN32
    auto module = LoadLibraryA(path);
    control = module ? reinterpret_cast<int (*)(int)>(GetProcAddress(module, "test_media_control")) : nullptr;
#else
    auto module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    control = module ? reinterpret_cast<int (*)(int)>(dlsym(module, "test_media_control")) : nullptr;
#endif
    /* keep the test controller loaded so counters remain inspectable after
    runtime shutdown; provider shutdown separately asserts no instances remain. */
    return control != nullptr;
}

extern "C" int media_fixture_control(int operation) {
    return control(operation);
}

extern "C" int media_worker_start(int (*work)(void*), void* data) {
    if (worker.joinable()) {
        return 0;
    }
    done = false;
    worker = std::thread([=] {
        const auto value = work(data);
        std::lock_guard<std::mutex> lock(mutex);
        result = value;
        done = true;
        changed.notify_all();
    });
    return 1;
}

extern "C" int media_worker_join() {
    std::unique_lock<std::mutex> lock(mutex);
    if (!changed.wait_for(lock, std::chrono::seconds(5),
            [] {
                return done;
            }))
    {
        std::abort();
    }
    lock.unlock();
    worker.join();
    return result;
}
