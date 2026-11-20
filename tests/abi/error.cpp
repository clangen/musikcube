#include "error.h"

#include <cstdlib>
#include <string>

namespace {
    bool fail_next_allocation = false;
}

/* Inject one failure in the statically linked test executable. */
void *operator new(size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc();
    }
    if (void *data = std::malloc(size ? size : 1)) return data;
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
