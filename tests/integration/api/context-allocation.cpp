#include <musikcore/context.h>
#include "request.h"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

namespace {
    /* Only the creating thread is faulted; test harness allocations stay live. */
    thread_local long allocationsUntilFailure = -1;
}

/* Fail the selected allocation on the creating thread while leaving other threads unaffected. */
void* operator new(std::size_t size) {
    if (allocationsUntilFailure == 0) throw std::bad_alloc();
    if (allocationsUntilFailure > 0) --allocationsUntilFailure;
    if (auto* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
/* Route array allocation through the same deterministic allocation-failure gate. */
void* operator new[](std::size_t size) { return ::operator new(size); }
/* Release storage obtained by the test's malloc-backed allocation override. */
void operator delete(void* memory) noexcept { std::free(memory); }
/* Release array storage obtained through the same malloc-backed override. */
void operator delete[](void* memory) noexcept { std::free(memory); }

/* Fail each initialization allocation in turn and verify no handle or singleton slot leaks. */
bool context_test_allocation_failures() {
    const std::string directory(512, 'x');
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_NONE;
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
            if (!context) {
                std::fprintf(stderr, "allocation %ld: successful create returned no context\n", allocation);
                return false;
            }
            ++successes;
        }
        else {
            if (result != MC_ERROR_INTERNAL || context) {
                std::fprintf(stderr, "allocation %ld: unexpected create result %d\n", allocation, (int) result);
                return false;
            }
            ++failures;
        }
        mc_context_destroy(&context);
        /* A failed constructor must never consume the process context slot. */
        if (mc_context_create(&options, &context, nullptr) != MC_OK) {
            std::fprintf(stderr, "allocation %ld: could not recreate context\n", allocation);
            return false;
        }
        mc_context_destroy(&context);
    }
    if (failures < 3 || successes == 0) {
        std::fprintf(stderr, "allocation fault sweep: %u failures, %u successes\n", failures, successes);
        return false;
    }
    return true;
}

/* ready notifications must remain deliverable under allocation pressure; dispatch
must not strand the context after detaching callbacks from its pending batch. */
bool context_test_notification_dispatch_without_allocations() {
    mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
    options.services = MC_SERVICE_NONE;
    mc_context* context = nullptr;
    if (mc_context_create(&options, &context, nullptr) != MC_OK) {
        return false;
    }
    unsigned calls = 0;
    std::shared_ptr<musik::core::ffi::ContextNotification> notification;
    const auto registered = musik::core::ffi::create_context_notification(context, [&] { ++calls; }, notification);
    if (registered != MC_OK) {
        mc_context_destroy(&context);
        return false;
    }
    notification->Notify();
    allocationsUntilFailure = 0;
    const auto status = mc_context_dispatch(context);
    allocationsUntilFailure = -1;
    const bool delivered = status == MC_OK && calls == 1 && mc_context_dispatch(context) == MC_OK;
    const auto destroyed = mc_context_destroy(&context);
    if (!delivered || destroyed != MC_OK) {
        std::fprintf(stderr, "notification dispatch allocated or lost its callback: status=%d calls=%u destroy=%d\n",
            status, calls, destroyed);
        return false;
    }
    return true;
}
