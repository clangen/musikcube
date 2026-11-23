#include <musikcore/plugin.h>
#include <stdexcept>

namespace {
    /* a plugin may have already supplied an error before throwing. the host
    must destroy it when replacing it with contextual exception details. */
    [[maybe_unused]] void createPriorError(const mc_host_api* host, mc_error** error) {
        static constexpr char message[] = "prior plugin error";
        host->create_error(host->host_context, MC_ERROR_PLUGIN, { message, sizeof(message) - 1 }, error);
    }

    /* fault injection: partially entered initialization remains safely cleanable. */
    mc_result MC_CALL initialize(const mc_host_api* host, void** state, mc_error** error) {
        *state = nullptr;
#if defined(INITIALIZE_THROWS)
        createPriorError(host, error);
        throw std::runtime_error("fixture initialization failure");
#else
        return MC_OK;
#endif
    }

    /* fault injection: a throwing shutdown cannot establish worker quiescence. */
    void MC_CALL shutdown(void*) {
#if defined(SHUTDOWN_THROWS)
        throw std::runtime_error("fixture shutdown failure");
#endif
    }
}

/* deliberately broken C++ plugins exercise exception policy at real DLL calls. */
extern "C" MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
    uint32_t, const mc_host_api* host, mc_plugin_api* plugin, mc_error** error) {
#if defined(ENTRY_THROWS)
    createPriorError(host, error);
    throw std::runtime_error("fixture entry failure");
#else
    *plugin = MC_PLUGIN_API_DEFAULT;
    plugin->id = { "org.musikcube.throwing", 22 };
    plugin->name = { "Throwing fixture", 16 };
    plugin->initialize = initialize;
    plugin->shutdown = shutdown;
    return MC_OK;
#endif
}
