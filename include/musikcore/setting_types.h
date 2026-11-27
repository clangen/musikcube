#ifndef MUSIKCORE_SETTING_TYPES_H
#define MUSIKCORE_SETTING_TYPES_H

#include <musikcore/abi.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* identifies the scalar type stored in a settings value. */
typedef uint32_t mc_setting_type;
enum {
    MC_SETTING_BOOLEAN,
    MC_SETTING_INTEGER,
    MC_SETTING_NUMBER,
    MC_SETTING_TEXT
};

/* scalar settings data; the value type selects the active member. */
typedef union mc_setting_data {
    /* Value for MC_SETTING_BOOLEAN. */
    bool boolean;
    /* Signed value for MC_SETTING_INTEGER. */
    int32_t integer;
    /* Finite value for MC_SETTING_NUMBER. */
    double number;
    /* UTF-8 bytes for MC_SETTING_TEXT, borrowed for the call or callback. */
    mc_string_view text;
} mc_setting_data;

/* one typed settings value, used by application and plugin stores. */
typedef struct mc_setting_value {
    /* Selects the active member of data. */
    mc_setting_type type;
    /* Scalar value or borrowed text of the specified type. */
    mc_setting_data data;
} mc_setting_value;

/* one borrowed key/value pair in a settings update batch. */
typedef struct mc_setting_entry {
    /* Nonempty key scoped to the setting store. */
    mc_string_view key;
    /* Value to copy into the store. */
    mc_setting_value value;
} mc_setting_entry;

#ifdef __cplusplus
}
#endif

#endif
