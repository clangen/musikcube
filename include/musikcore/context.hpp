#ifndef MUSIKCORE_CONTEXT_HPP
#define MUSIKCORE_CONTEXT_HPP

#include <musikcore/context.h>

#include <stdexcept>
#include <optional>
#include <string>
#include <string_view>

namespace musik { namespace core { namespace api {

    /* Owns a selection snapshot and its diagnostic strings. Copyable and movable. */
    struct LibraryStatus {
        mc_library_kind kind = MC_LIBRARY_NONE;
        mc_library_connection_state state = MC_LIBRARY_DISCONNECTED;
        mc_library_connection_error error = MC_LIBRARY_CONNECTION_OK;
        bool configured = false;
        std::string endpoint;
        std::string serverVersion;
    };

    class Library;
    class Playback;
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

    /* owns engine services, request processing, and callback dispatch. select a
    fixed subset through mc_context_options::services, or MC_SERVICE_NONE for
    caller-owned catalogs. keep a Context in the host event loop and call
    Dispatch after wakeups; it must outlive its users. neither copyable nor
    movable. dispatch, shutdown, and destruction use the dispatcher thread.
    destruction drains callbacks, so their state must still be alive. */
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
        mc_result Close() { return mc_context_destroy(&context); }
        mc_result Shutdown() { return mc_context_shutdown(context); }

        void SetLibrary(const Library& library);
        void ClearLibrary();
        std::optional<Library> GetLibrary() const;
        /* includes automatically selected prerequisites; fixed at creation. */
        mc_services Services() const {
            mc_services services = MC_SERVICE_NONE;
            const auto result = mc_context_get_services(this->context, &services);
            if (result != MC_OK) {
                throw Error(result);
            }
            return services;
        }

        mc_result SelectLibrary(mc_library_kind kind) { return mc_context_select_library(this->context, kind); }
        mc_result ReconnectRemoteLibrary() { return mc_context_reconnect_remote_library(this->context); }
        LibraryStatus GetLibraryStatus() const {
            LibraryStatus status;
            const auto result = mc_context_read_library_status(this->context,
                [](void* data, const mc_library_status* value) {
                    auto& status = *static_cast<LibraryStatus*>(data);
                    status.kind = value->kind;
                    status.state = value->state;
                    status.error = value->error;
                    status.configured = value->configured;
                    status.endpoint = std::string(AsStringView(value->endpoint));
                    status.serverVersion = std::string(AsStringView(value->server_version));
                }, &status);
            if (result != MC_OK) {
                throw Error(result);
            }
            return status;
        }
        mc_result SetLibraryChangedCallback(mc_library_changed_fn callback, void* data = nullptr) {
            return mc_context_set_library_changed_callback(this->context, callback, data);
        }
        Playback GetPlayback() const;

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
