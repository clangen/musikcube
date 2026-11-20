#ifndef MUSIKCORE_CONTEXT_HPP
#define MUSIKCORE_CONTEXT_HPP

#include <musikcore/context.h>

#include <stdexcept>
#include <string>
#include <string_view>

namespace musik { namespace core { namespace api {

    /* Adapt a C string view for C++ readers without copying. Use only while the
       supplying API's owner is alive; this conversion does not retain storage. */
    inline std::string_view AsStringView(mc_string_view value) noexcept {
        return value.data ? std::string_view(value.data, value.length) : std::string_view{};
    }

    /* Exception for wrapper construction or rejected request submission. Catch
       this for synchronous failures; accepted requests report status in their
       callbacks. Copyable and movable, with an owned diagnostic message. */
    class Error : public std::runtime_error {
    public:
        explicit Error(mc_result result, std::string_view message = {})
            : std::runtime_error(message.empty()
                ? "musikcore error " + std::to_string(result) : std::string(message)),
              resultCode(result) {}

        Error(const Error &) = default;
        Error &operator=(const Error &) = default;
        Error(Error &&) noexcept = default;
        Error &operator=(Error &&) noexcept = default;

        mc_result Result() const noexcept { return resultCode; }

    private:
        mc_result resultCode;
    };

    /* Owns request processing and callback dispatch. Keep a Context in the host
       event loop and call Dispatch after wakeups; it must outlive its users.
       Neither copyable nor movable. Dispatch, shutdown, and destruction use the
       dispatcher thread. Destruction drains callbacks, so their state must
       still be alive when the Context is destroyed. */
    class Context {
    public:
        Context() : Context(nullptr) {}
        explicit Context(const mc_context_options &options) : Context(&options) {}
        ~Context() { mc_context_destroy(&context); }

        Context(const Context &) = delete;
        Context &operator=(const Context &) = delete;
        Context(Context &&) = delete;
        Context &operator=(Context &&) = delete;

        /* Wrapper-owned handle. Do not destroy it; valid until wrapper destruction. */
        mc_context *Get() noexcept { return context; }
        const mc_context *Get() const noexcept { return context; }
        mc_result Dispatch() { return mc_context_dispatch(context); }
        mc_result Cancel(mc_request_id request) { return mc_request_cancel(context, request); }
        mc_result Shutdown() { return mc_context_shutdown(context); }

    private:
        explicit Context(const mc_context_options *options) {
            mc_error *error = nullptr;
            const auto result = mc_context_create(options, &context, &error);
            if (result != MC_OK) {
                /* Copy details before releasing the C error, including if construction throws. */
                struct OwnedError {
                    mc_error *value;
                    ~OwnedError() { mc_error_destroy(&value); }
                } owned{ error };
                throw Error(result, AsStringView(mc_error_get_message(owned.value)));
            }
        }

        mc_context *context = nullptr;
    };

} } }

#endif
