#ifndef MUSIKCORE_EQUALIZER_HPP
#define MUSIKCORE_EQUALIZER_HPP

#include <musikcore/context.hpp>
#include <musikcore/equalizer.h>

#include <array>

namespace musik { namespace core { namespace api {
    using EqualizerGains = std::array<double, MC_EQUALIZER_BAND_COUNT>;

    inline std::optional<mc_equalizer_state> ReadEqualizer() {
        mc_equalizer_state state = MC_EQUALIZER_STATE_DEFAULT;
        const auto status = mc_equalizer_get_state(&state);
        if (status == MC_ERROR_UNSUPPORTED) {
            return std::nullopt;
        }
        if (status != MC_OK) {
            throw Error(status);
        }
        return state;
    }

    inline mc_result SetEqualizerEnabled(bool enabled) {
        return mc_equalizer_set_enabled(enabled);
    }

    inline mc_result SetEqualizerGains(const EqualizerGains& gains) {
        return mc_equalizer_set_gains(gains.data(), gains.size());
    }

    inline mc_result SetEqualizerChangedCallback(
        Context& context, mc_equalizer_changed_fn callback, void* userData = nullptr)
    {
        return mc_context_set_equalizer_changed_callback(context.Get(), callback, userData);
    }
} } }

#endif
