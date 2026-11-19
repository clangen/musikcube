#ifndef MUSIKCORE_FFI_COMMON_ERROR_H
#define MUSIKCORE_FFI_COMMON_ERROR_H

#include <musikcore/abi.h>
#include <new>

namespace musik { namespace core { namespace ffi {

    mc_result fail(mc_result code, const char *message, mc_error **out_error) noexcept;

    /* The output slot must not already own an error. */
    template <typename Operation>
    mc_result exception_barrier(mc_error **out_error, Operation &&operation) noexcept {
        if (out_error) *out_error = nullptr;
        try {
            return operation();
        } catch (const std::bad_alloc &) {
            return MC_ERROR_INTERNAL;
        } catch (...) {
            return fail(MC_ERROR_INTERNAL, "Unexpected internal error", out_error);
        }
    }

} } }

#endif
