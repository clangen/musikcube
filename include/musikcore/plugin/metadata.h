#ifndef MUSIKCORE_PLUGIN_METADATA_H
#define MUSIKCORE_PLUGIN_METADATA_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* replay gain values as stored by the current library. gain is in decibels,
peak is a linear amplitude. the current store uses 1.0 as its unset sentinel.
all four values are supplied together. */
typedef struct mc_replay_gain_values {
    /* available bytes. initialize with MC_REPLAY_GAIN_VALUES_DEFAULT. */
    uint32_t struct_size;

    /* track gain in decibels; 1.0 means unset. */
    float track_gain;

    /* linear track peak; 1.0 means unset. */
    float track_peak;

    /* album gain in decibels; 1.0 means unset. */
    float album_gain;

    /* linear album peak; 1.0 means unset. */
    float album_peak;
} mc_replay_gain_values;

#define MC_REPLAY_GAIN_VALUES_DEFAULT { sizeof(mc_replay_gain_values), 1.0f, 1.0f, 1.0f, 1.0f }

/* lets a plugin update a track while it is being indexed.
the writer and its context are valid only during the host call. */
typedef struct mc_metadata_writer {
    /* available bytes. initialize with MC_METADATA_WRITER_DEFAULT. */
    uint32_t struct_size;

    /* host-owned track state. pass unchanged to each callback. */
    void* context;

    /* append a nonempty UTF-8 value for the given nonempty UTF-8 key. */
    mc_result (MC_CALL* set_value)(void* context, mc_string_view key, mc_string_view value);

    /* remove every value for the key. */
    mc_result (MC_CALL* clear)(void* context, mc_string_view key);

    /* write one if the track has a value for the key, otherwise zero. */
    mc_result (MC_CALL* contains)(void* context, mc_string_view key, uint32_t* present);

    /* copy nonempty artwork bytes into the track. */
    mc_result (MC_CALL* set_artwork)(void* context, mc_byte_view bytes);

    /* write one when the track has artwork, otherwise zero. */
    mc_result (MC_CALL* contains_artwork)(void* context, uint32_t* present);

    /* replace all four replay-gain factors. */
    mc_result (MC_CALL* set_replay_gain)(void* context, const mc_replay_gain_values* gain);
} mc_metadata_writer;

#define MC_METADATA_WRITER_DEFAULT { sizeof(mc_metadata_writer), NULL, NULL, NULL, NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
