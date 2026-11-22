#ifndef MUSIKCORE_FFI_COMMON_ERROR_H
#define MUSIKCORE_FFI_COMMON_ERROR_H

#include <musikcore/abi.h>
#include <new>

namespace musik { namespace core { namespace ffi {

    mc_result fail(mc_result code, const char *message, mc_error **out_error) noexcept;

    /* stack-scoped details shared by all asynchronous result adapters. */
    class CompletionError {
    public:
        /* copy optional operation/detail text during construction; diagnostics
        may be absent under allocation failure without changing the result. */
        explicit CompletionError(mc_result result, const char* operation = nullptr,
            const char* detail = nullptr) noexcept;
        CompletionError(const CompletionError&) = delete;

        ~CompletionError() { mc_error_destroy(&this->error); }

        CompletionError& operator=(const CompletionError&) = delete;

        const mc_error* Get() const noexcept { return this->error; }

    private:
        mc_error* error = nullptr;
    };

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
