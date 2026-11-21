#ifndef MUSIKCORE_PREFERENCES_H
#define MUSIKCORE_PREFERENCES_H

#include <musikcore/abi.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mc_preference_scope;
enum {
    MC_PREFERENCE_SETTINGS,
    MC_PREFERENCE_SESSION,
    MC_PREFERENCE_HOTKEYS
};

typedef uint32_t mc_preference_type;
enum {
    MC_PREFERENCE_BOOLEAN,
    MC_PREFERENCE_INTEGER,
    MC_PREFERENCE_NUMBER,
    MC_PREFERENCE_TEXT
};

typedef union mc_preference_data {
    /* Value for MC_PREFERENCE_BOOLEAN. */
    bool boolean;
    /* Signed value for MC_PREFERENCE_INTEGER. */
    int32_t integer;
    /* Finite value for MC_PREFERENCE_NUMBER. */
    double number;
    /* UTF-8 bytes for MC_PREFERENCE_TEXT, borrowed for the call or callback. */
    mc_string_view text;
} mc_preference_data;

typedef struct mc_preference_value {
    /* Selects the active member of data. */
    mc_preference_type type;
    /* Scalar value or borrowed text of the specified type. */
    mc_preference_data data;
} mc_preference_value;

typedef struct mc_preference_entry {
    /* Nonempty key scoped to the preference store. */
    mc_string_view key;
    /* Value to copy into the store. */
    mc_preference_value value;
} mc_preference_entry;

typedef void (MC_CALL *mc_preference_read_fn)(void* user_data, const mc_preference_value* value);
typedef void (MC_CALL *mc_preference_key_fn)(void* user_data, mc_string_view key);

/* Read a value of the fallback's type. Missing or incompatible values use the
   fallback. Empty stored text uses a nonempty fallback. The callback runs
   inline with a borrowed value. These operations are thread-safe. */
MC_API mc_result MC_CALL mc_preferences_read(mc_preference_scope scope, mc_string_view key,
    const mc_preference_value* fallback, mc_preference_read_fn read, void* user_data);
/* Copy a batch into memory as one update. Later duplicate keys take precedence.
   A zero count is valid. Invalid entries leave the store unchanged. */
MC_API mc_result MC_CALL mc_preferences_write(mc_preference_scope scope,
    const mc_preference_entry* entries, size_t count);
/* Persist the current store. Returns MC_ERROR_IO on a write failure. */
MC_API mc_result MC_CALL mc_preferences_save(mc_preference_scope scope);
/* Visit a snapshot of the store's keys. Each key is valid during its callback. */
MC_API mc_result MC_CALL mc_preferences_list_keys(mc_preference_scope scope, mc_preference_key_fn read, void* user_data);

#ifdef __cplusplus
}
#endif

#endif
