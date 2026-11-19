#ifndef MUSIKCORE_CONTEXT_HPP
#define MUSIKCORE_CONTEXT_HPP

#include <musikcore/context.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace musik { namespace core { namespace client {

    inline std::string_view as_string_view(mc_string_view value) noexcept {
        return value.data ? std::string_view(value.data, value.length) : std::string_view{};
    }

    /* Copyable and movable. The exception owns its message. */
    class Error : public std::runtime_error {
    public:
        explicit Error(mc_result result, std::string_view message = {})
            : std::runtime_error(message.empty()
                ? "musikcore error " + std::to_string(result) : std::string(message)),
              result_(result) {}

        Error(const Error &) = default;
        Error &operator=(const Error &) = default;
        Error(Error &&) noexcept = default;
        Error &operator=(Error &&) noexcept = default;

        mc_result result() const noexcept { return result_; }

    private:
        mc_result result_;
    };

    /* Neither copyable nor movable. Dispatch, shutdown, and destruction run on the dispatcher thread. */
    /* Destruction drains callbacks. */
    class Context {
    public:
        Context() : Context(nullptr) {}
        explicit Context(const mc_context_options &options) : Context(&options) {}
        ~Context() { mc_context_destroy(&context_); }

        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;
        Context(Context &&) = delete;
        Context &operator=(Context &&) = delete;

        /* Borrows the handle for the lifetime of this wrapper. */
        mc_context *get() noexcept { return context_; }
        const mc_context *get() const noexcept { return context_; }
        mc_result dispatch() { return mc_context_dispatch(context_); }
        mc_result cancel(mc_request_id request) { return mc_request_cancel(context_, request); }
        mc_result shutdown() { return mc_context_shutdown(context_); }

    private:
        explicit Context(const mc_context_options *options) {
            mc_error *error = nullptr;
            const auto result = mc_context_create(options, &context_, &error);
            if (result != MC_OK) {
                /* Copy details before releasing the C error, including if construction throws. */
                struct OwnedError {
                    mc_error *value;
                    ~OwnedError() { mc_error_destroy(&value); }
                } owned{ error };
                throw Error(result, as_string_view(mc_error_get_message(owned.value)));
            }
        }

        mc_context *context_ = nullptr;
    };

} } }

#endif
