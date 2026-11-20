#include <musikcore/context.h>
#include <cstdlib>
#include <new>
#include <string>

namespace {
    /* Only the creating thread is faulted; test harness allocations stay live. */
    thread_local long allocationsUntilFailure = -1;
}

void* operator new(std::size_t size) {
    if (allocationsUntilFailure == 0) throw std::bad_alloc();
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    if (auto* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }

bool context_test_allocation_failures() {
    const std::string directory(512, 'x');
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.plugin_directory = {directory.data(), directory.size()};
    unsigned failures = 0;
    unsigned successes = 0;
    for (long allocation = 0; allocation < 10; ++allocation) {
        mc_context* context = nullptr;
        mc_error* error = nullptr;
        allocationsUntilFailure = allocation;
        const auto result = mc_context_create(&options, &context, &error);
        allocationsUntilFailure = -1;
        mc_error_destroy(&error);
        if (result == MC_OK) {
            if (!context) return false;
            ++successes;
        }
        else {
            if (result != MC_ERROR_INTERNAL || context) return false;
            ++failures;
        }
        mc_context_destroy(&context);
        /* A failed constructor must never consume the process context slot. */
        if (mc_context_create(nullptr, &context, nullptr) != MC_OK) return false;
        mc_context_destroy(&context);
    }
    return failures >= 3 && successes != 0;
}
