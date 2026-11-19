#include <musikcore/musikcore.h>

#include "../common/error.h"
#include "../common/primitives.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <string>

struct mc_context {
    std::string data_directory;
    std::string plugin_directory;
};

namespace {
    std::atomic<bool> context_active{ false };

    bool valid_directory(mc_string_view directory) {
        return !directory.length ||
            (directory.data && !std::memchr(directory.data, '\0', directory.length));
    }

    template <typename T>
    T option(const mc_context_options *options, T mc_context_options::*field, size_t offset) {
        using musik::core::ffi::has_field;
        return options && has_field(options->struct_size, offset, sizeof(T)) ? options->*field : T{};
    }
}

mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error) {
    using musik::core::ffi::exception_barrier;
    using musik::core::ffi::fail;

    return exception_barrier(out_error, [&]() -> mc_result {
        /* Clear the output before any validation can fail. */
        if (!out_context) return fail(MC_ERROR_INVALID_ARGUMENT, "Missing context output", out_error);
        *out_context = nullptr;

        /* Validate the options prefix and negotiate the ABI version. */
        if (options && options->struct_size < sizeof(options->struct_size)) {
            return fail(MC_ERROR_INVALID_ARGUMENT, "Context options are too small", out_error);
        }

        const auto version = option(options, &mc_context_options::requested_abi_version,
            offsetof(mc_context_options, requested_abi_version));
        if (version && (MC_ABI_VERSION_MAJOR(version) != MC_ABI_VERSION_MAJOR(MC_ABI_VERSION) ||
            MC_ABI_VERSION_MINOR(version) > MC_ABI_VERSION_MINOR(MC_ABI_VERSION))) {
            return fail(MC_ERROR_UNSUPPORTED, "Unsupported ABI version", out_error);
        }
        if (option(options, &mc_context_options::flags, offsetof(mc_context_options, flags))) {
            return fail(MC_ERROR_UNSUPPORTED, "Unsupported context flags", out_error);
        }

        /* Default omitted roots and check that supplied paths can be copied. */
        const auto data = option(options, &mc_context_options::data_directory,
            offsetof(mc_context_options, data_directory));
        const auto plugins = option(options, &mc_context_options::plugin_directory,
            offsetof(mc_context_options, plugin_directory));
        if (!valid_directory(data) || !valid_directory(plugins)) {
            return fail(MC_ERROR_INVALID_ARGUMENT, "Invalid directory view", out_error);
        }
        /* Avoid allocating a context when the process slot is already occupied. */
        if (context_active.load()) {
            return fail(MC_ERROR_BUSY, "A context is already active", out_error);
        }

        /* Claim the process slot after allocations succeed. */
        std::unique_ptr<mc_context> context(new mc_context{
            std::string(data.length ? data.data : "", data.length),
            std::string(plugins.length ? plugins.data : "", plugins.length)
        });
        if (context_active.exchange(true)) {
            return fail(MC_ERROR_BUSY, "A context is already active", out_error);
        }
        *out_context = context.release();
        return MC_OK;
    });
}

void MC_CALL mc_context_destroy(mc_context **context) {
    if (!context || !*context) return;
    delete *context;
    *context = nullptr;
    context_active.store(false);
}
