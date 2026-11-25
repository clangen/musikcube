#pragma once

#include <musikcore/plugin/registration.hpp>
#include <new>
#include <string_view>
#include <type_traits>

namespace musik { namespace core { namespace plugin {

    /* T supplies static Name() (with process-lifetime storage) and Inputs, a services constructor, Show,
    Hide, IsVisible, and the WritePcm/WriteSpectrum methods selected by Inputs. */
    template<typename T> mc_result RegisterVisualizer(const mc_host_api& host) {
        static_assert(std::is_nothrow_destructible_v<T>, "visualizer destruction must not throw");
        static constexpr mc_plugin_visualizer_vtable methods {
            sizeof(mc_plugin_visualizer_vtable),
            [](void* instance) { delete static_cast<T*>(instance); },
            [](void* instance) -> mc_result {
                try { return static_cast<T*>(instance)->Show(); } catch (...) { return MC_ERROR_INTERNAL; }
            },
            [](void* instance) -> mc_result {
                try { return static_cast<T*>(instance)->Hide(); } catch (...) { return MC_ERROR_INTERNAL; }
            },
            [](void* instance, bool* visible) -> mc_result {
                try { return static_cast<T*>(instance)->IsVisible(*visible); } catch (...) { return MC_ERROR_INTERNAL; }
            },
            [](void* instance, const mc_pcm_view* pcm) -> mc_result {
                if constexpr (T::Inputs & MC_VISUALIZER_INPUT_PCM) {
                    try { return static_cast<T*>(instance)->WritePcm(*pcm); } catch (...) { return MC_ERROR_INTERNAL; }
                }
                return MC_ERROR_UNSUPPORTED;
            },
            [](void* instance, const float* bins, size_t count, uint32_t rate) -> mc_result {
                if constexpr (T::Inputs & MC_VISUALIZER_INPUT_SPECTRUM) {
                    try { return static_cast<T*>(instance)->WriteSpectrum(bins, count, rate); }
                    catch (...) { return MC_ERROR_INTERNAL; }
                }
                return MC_ERROR_UNSUPPORTED;
            },
        };
        static const std::string_view name(T::Name());
        static const mc_visualizer_factory factory {
            sizeof(mc_visualizer_factory), nullptr, &methods, { name.data(), name.size() }, T::Inputs,
            [](void*, const mc_plugin_services* services, void** instance, mc_error**) -> mc_result {
                try { *instance = new T(*services); return MC_OK; }
                catch (...) { *instance = nullptr; return MC_ERROR_INTERNAL; }
            },
        };
        return RegisterVisualizer(host, factory);
    }

} } }
