#include <pulse/pulseaudio.h>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <dlfcn.h>
#include <unistd.h>

namespace {
    std::atomic<bool> forbidden { false };
    std::atomic<int> drainGate { -1 };
    std::atomic<pa_operation*> stalledDrain { nullptr };
    std::atomic<pa_volume_t> requestedVolume { PA_VOLUME_INVALID };

    template<typename Method>
    Method pulseMethod(const char* name) {
        static void* library = dlopen("libpulse.so.0", RTLD_NOW | RTLD_NOLOAD);
        auto method = library ? reinterpret_cast<Method>(dlsym(library, name)) : nullptr;
        if (!method) {
            std::abort();
        }
        return method;
    }
}

extern "C" void mc_test_pulse_forbid_wait(int value) {
    forbidden.store(value != 0);
}

extern "C" void pa_threaded_mainloop_wait(pa_threaded_mainloop* loop) {
    if (forbidden.load()) {
        std::fputs("PulseAudio control waited for a server response\n", stderr);
        std::abort();
    }
    static const auto method = pulseMethod<decltype(&pa_threaded_mainloop_wait)>("pa_threaded_mainloop_wait");
    method(loop);
}

extern "C" unsigned mc_test_pulse_requested_volume() {
    return requestedVolume.load();
}

extern "C" pa_operation* pa_context_set_sink_input_volume(
    pa_context* context, uint32_t index, const pa_cvolume* volume, pa_context_success_cb_t callback, void* user)
{
    static const auto method =
        pulseMethod<decltype(&pa_context_set_sink_input_volume)>("pa_context_set_sink_input_volume");
    requestedVolume.store(volume->values[0]);
    return method(context, index, volume, callback, user);
}

extern "C" void mc_test_pulse_stall_drain(int gate) {
    drainGate.store(gate);
    if (gate < 0) {
        stalledDrain.store(nullptr);
    }
}

extern "C" pa_operation* pa_stream_drain(pa_stream* stream, pa_stream_success_cb_t callback, void* user) {
    static const auto method = pulseMethod<decltype(&pa_stream_drain)>("pa_stream_drain");
    if (const int gate = drainGate.load(); gate >= 0) {
        auto* operation = method(stream, nullptr, nullptr);
        stalledDrain.store(operation);
        if (!operation || write(gate, "x", 1) != 1) {
            std::abort();
        }
        return operation;
    }
    return method(stream, callback, user);
}

extern "C" pa_operation_state_t pa_operation_get_state(const pa_operation* operation) {
    static const auto method = pulseMethod<decltype(&pa_operation_get_state)>("pa_operation_get_state");
    return operation == stalledDrain.load() ? PA_OPERATION_RUNNING : method(operation);
}
