#include "GmeIndexerSource.h"
#include <musikcore/plugin.h>
#include <musikcore/plugin/registration.hpp>
#include <limits>

const mc_configuration_api& GmeIndexerConfiguration() {
    static const mc_boolean_setting booleans[] {
        { { "always_loop_forever", 19 }, false },
        { { "ignore_embedded_track_length", 28 }, false },
        { { "exclude_tracks_shorter_than_minimum_length", 42 }, false },
        { { "enable_m3u_support", 18 }, false },
    };
    constexpr double bound = std::numeric_limits<double>::max();
    static const mc_number_setting numbers[] {
        { { "default_track_length_secs", 25 }, 180.0, -bound, bound, 2 },
        { { "track_fade_out_length_secs", 26 }, 3.0, -bound, bound, 2 },
        { { "minimum_track_length_secs", 25 }, 0.0, -bound, bound, 2 },
    };
    static const mc_configuration_api schema {
        .struct_size = sizeof(mc_configuration_api),
        .booleans = booleans,
        .boolean_count = sizeof(booleans) / sizeof(*booleans),
        .numbers = numbers,
        .number_count = sizeof(numbers) / sizeof(*numbers),
    };
    return schema;
}
