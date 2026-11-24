#include <pulse/thread-mainloop.h>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <dlfcn.h>

static std::atomic<bool> forbidden { false };

extern "C" void mc_test_pulse_forbid_wait(int value) {
    forbidden.store(value != 0);
}

extern "C" void pa_threaded_mainloop_wait(pa_threaded_mainloop* loop) {
    if (forbidden.load()) {
        std::fputs("PulseAudio control waited for a server response\n", stderr);
        std::abort();
    }
    using Method = void (*)(pa_threaded_mainloop*);
    static void* library = dlopen("libpulse.so.0", RTLD_NOW | RTLD_NOLOAD);
    auto method = library ? reinterpret_cast<Method>(dlsym(library, "pa_threaded_mainloop_wait")) : nullptr;
    if (!method) {
        std::abort();
    }
    method(loop);
}
