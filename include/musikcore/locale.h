#ifndef MUSIKCORE_LOCALE_H
#define MUSIKCORE_LOCALE_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (MC_CALL *mc_locale_text_fn)(void* user_data, mc_string_view text);

/* Load locale resources and restore the selected locale. Call before rendering
   the UI. Remaining operations use the currently loaded resources. */
MC_API mc_result MC_CALL mc_locale_load(mc_string_view directory);

/* Callbacks run inline. Text is valid until the callback returns. */
MC_API mc_result MC_CALL mc_locale_list(mc_locale_text_fn read, void* user_data);
MC_API mc_result MC_CALL mc_locale_read_selected(mc_locale_text_fn read, void* user_data);
MC_API mc_result MC_CALL mc_locale_select(mc_string_view locale);
/* Use the key itself when neither the selected nor default locale defines it. */
MC_API mc_result MC_CALL mc_locale_translate(mc_string_view key, mc_locale_text_fn read, void* user_data);
/* Use fallback when neither the selected nor default locale defines the key. */
MC_API int MC_CALL mc_locale_dimension(mc_string_view key, int fallback);

#ifdef __cplusplus
}
#endif

#endif
