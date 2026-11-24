#pragma once

#include <musikcore/plugin.h>
#include <musikcore/plugin/dsp.h>

#include <type_traits>

namespace musik { namespace core { namespace plugin {

    /* registers a C++ processor as a C plugin. each stream receives a fresh
    Processor, and Reload notifies existing instances of preference changes. */
    template<typename Processor> mc_result RegisterDsp(const mc_host_api& host) {
        static_assert(std::is_nothrow_destructible_v<Processor>, "DSP destruction must not throw");
        static_assert(noexcept(Processor::NotifyChanged()), "DSP notification must not throw");
        static const mc_dsp_vtable methods {
            sizeof(mc_dsp_vtable),
            [](void* instance) {
                delete static_cast<Processor*>(instance);
            },
            [](void* instance, mc_audio_buffer_view* audio) -> mc_result {
                try {
                    return static_cast<Processor*>(instance)->Process(*audio);
                }
                catch (...) {
                    return MC_ERROR_INTERNAL;
                }
            },
        };
        static const mc_dsp_factory factory {
            sizeof(mc_dsp_factory),
            nullptr,
            &methods,
            [](void*, const mc_plugin_services* services, void** instance, mc_error**) -> mc_result {
                try {
                    *instance = new Processor(*services);
                    return MC_OK;
                }
                catch (...) {
                    return MC_ERROR_INTERNAL;
                }
            },
            [](void*) {
                Processor::NotifyChanged();
            },
        };
        mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
        capability.id = { MC_PLUGIN_CAPABILITY_DSP, sizeof(MC_PLUGIN_CAPABILITY_DSP) - 1 };
        capability.api = &factory;
        capability.api_size = sizeof(factory);
        return host.register_capability(host.host_context, &capability);
    }

} } }
