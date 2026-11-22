#include "error.h"

#include <cstdlib>
#include <cstring>
#include <string>

namespace {
    bool fail_next_allocation = false;
    int allocations_until_failure = -1;

    /* inspect borrowed message bytes without allocating during fault injection. */
    bool hasMessage(const mc_error* error, const char* expected) {
        const auto message = mc_error_get_message(error);
        return message.length == std::strlen(expected) && message.data &&
            std::memcmp(message.data, expected, message.length) == 0;
    }
}

/* Inject one failure in the statically linked test executable. */
void *operator new(size_t size) {
    if (fail_next_allocation || allocations_until_failure == 0) {
        fail_next_allocation = false;
        allocations_until_failure = -1;
        throw std::bad_alloc();
    }
    if (allocations_until_failure > 0) {
        --allocations_until_failure;
    }
    if (void *data = std::malloc(size ? size : 1)) {
        return data;
    }
    throw std::bad_alloc();
}

/* Release the test allocator's storage through the unsized delete path. */
void operator delete(void *data) noexcept {
    std::free(data);
}

/* Release the test allocator's storage through the sized delete path. */
void operator delete(void *data, size_t) noexcept {
    std::free(data);
}

/* Select success, detailed failure, allocation failure, or an unknown exception to probe C error
   translation. */
extern "C" mc_result MC_CALL check_error_operation(int mode, mc_error **error) {
    using musik::core::ffi::exception_barrier;
    using musik::core::ffi::fail;

    return exception_barrier(error, [&]() -> mc_result {
        switch (mode) {
            case 0:
                return MC_OK;
            case 1: {
                const std::string message = "Input is missing";
                return fail(MC_ERROR_NOT_FOUND, message.c_str(), error);
            }
            case 2:
                /* Allocation failure itself must not need an allocated error. */
                throw std::bad_alloc();
            case 4: {
                /* Preserve NOT_FOUND even when allocating its message fails. */
                fail_next_allocation = true;
                const auto result = fail(MC_ERROR_NOT_FOUND, "Input is missing", error);
                const bool allocation_attempted = !fail_next_allocation;
                fail_next_allocation = false;
                return allocation_attempted ? result : MC_ERROR_INTERNAL;
            }
            default:
                /* No unrecognized C++ exception may escape a C entry point. */
                throw mode;
        }
    });
}

/* asynchronous failures must identify their status reason instead of hiding it
behind one generic message shared by unrelated operations. */
extern "C" int MC_CALL check_completion_errors(void) {
    using musik::core::ffi::CompletionError;
    {
        const CompletionError error(MC_ERROR_UNSUPPORTED);
        if (!error.Get() || mc_error_get_code(error.Get()) != MC_ERROR_UNSUPPORTED ||
            !hasMessage(error.Get(), "The requested operation is not supported"))
        {
            return 0;
        }
    }
    /* operation names and status reasons must survive together through borrowed delivery. */
    {
        const CompletionError error(MC_ERROR_NOT_FOUND, "Read track details");
        if (!error.Get() || mc_error_get_code(error.Get()) != MC_ERROR_NOT_FOUND ||
            !hasMessage(error.Get(), "Read track details: The requested item was not found"))
        {
            return 0;
        }
    }
    /* specific backend details replace the generic reason and own both input strings. */
    {
        char operation[] = "Connect to server";
        char detail[] = "The server version is incompatible";
        const CompletionError error(MC_ERROR_IO, operation, detail);
        operation[0] = 'X';
        detail[0] = 'X';
        if (!error.Get() || mc_error_get_code(error.Get()) != MC_ERROR_IO ||
            !hasMessage(error.Get(), "Connect to server: The server version is incompatible"))
        {
            return 0;
        }
    }
    /* queue pressure and uncertain writes retain retry guidance despite custom details. */
    {
        const CompletionError queued(MC_ERROR_QUEUE_FULL, "Search library", "Try again later");
        const CompletionError uncertain(MC_ERROR_OUTCOME_UNKNOWN, "Save playlist", "Connection lost");
        if (!hasMessage(queued.Get(), "Search library: The context request queue is full. Try again later") ||
            !hasMessage(uncertain.Get(), "Save playlist: The backend may have applied the mutation. Refresh before retrying. Connection lost"))
        {
            return 0;
        }
    }
    /* success must not allocate diagnostic storage, even with operation and detail supplied. */
    {
        fail_next_allocation = true;
        const CompletionError error(MC_OK, "Read track details", "This detail must be ignored");
        const bool valid = !error.Get() && fail_next_allocation;
        fail_next_allocation = false;
        if (!valid) {
            return 0;
        }
    }
    /* sweep string construction and owned-error allocation; diagnostics may be
    absent under allocation failure, but must never replace the original status. */
    unsigned failedAllocations = 0;
    bool completedAllocation = false;
    for (int budget = 0; budget < 8; ++budget) {
        allocations_until_failure = budget;
        const CompletionError error(MC_ERROR_IO,
            "Read track details from the selected music library",
            "The remote server could not return the requested track details over the connection");
        const bool failed = allocations_until_failure < 0;
        allocations_until_failure = -1;
        if (failed) {
            ++failedAllocations;
            if (error.Get()) {
                return 0;
            }
        }
        else {
            completedAllocation = true;
            if (!error.Get() || mc_error_get_code(error.Get()) != MC_ERROR_IO) {
                return 0;
            }
        }
    }
    return failedAllocations >= 2 && completedAllocation;
}
