#pragma once

#include <musikcore/plugin.h>
#include <musikcore/plugin/playback-remote.h>
#include <musikcore/plugin/registration.hpp>

#include <type_traits>
#include <utility>

namespace musik { namespace core { namespace plugin {

    /* registers a C++ playback remote without requiring a virtual base class.
    Remote receives instance services and implements Changed, QueueChanged,
    Start, RequestStop, and Join. The host stops and joins before destruction. */
    template<typename Remote> mc_result RegisterPlaybackRemote(const mc_host_api& host) {
        static_assert(std::is_nothrow_destructible_v<Remote>, "playback remote destruction must not throw");
        static_assert(noexcept(std::declval<Remote&>().Changed(std::declval<const mc_plugin_playback_event&>())),
            "playback remote notification must not throw");
        static_assert(noexcept(std::declval<Remote&>().QueueChanged()), "queue notification must not throw");
        static_assert(noexcept(std::declval<Remote&>().RequestStop()), "playback remote stop must not throw");

        static const mc_playback_remote_vtable methods {
            sizeof(mc_playback_remote_vtable),
            [](void* instance) { delete static_cast<Remote*>(instance); },
            [](void* instance, const mc_plugin_playback_event* event) {
                static_cast<Remote*>(instance)->Changed(*event);
            },
            [](void* instance) { static_cast<Remote*>(instance)->QueueChanged(); },
            [](void* instance, mc_error**) -> mc_result {
                try {
                    return static_cast<Remote*>(instance)->Start();
                }
                catch (...) {
                    return MC_ERROR_PLUGIN;
                }
            },
            [](void* instance) { static_cast<Remote*>(instance)->RequestStop(); },
            [](void* instance, mc_error**) -> mc_result {
                try {
                    return static_cast<Remote*>(instance)->Join();
                }
                catch (...) {
                    return MC_ERROR_PLUGIN;
                }
            },
        };

        static const mc_playback_remote_factory factory {
            sizeof(mc_playback_remote_factory),
            nullptr,
            &methods,
            [](void*, const mc_plugin_services* services, void** instance, mc_error**) -> mc_result {
                try {
                    *instance = new Remote(*services);
                    return MC_OK;
                }
                catch (...) {
                    return MC_ERROR_PLUGIN;
                }
            },
        };

        return RegisterPlaybackRemote(host, factory);
    }

} } }
