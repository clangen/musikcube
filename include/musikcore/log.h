#ifndef MUSIKCORE_LOG_H
#define MUSIKCORE_LOG_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mc_log_level;
enum {
    MC_LOG_VERBOSE,
    MC_LOG_INFO,
    MC_LOG_WARNING,
    MC_LOG_ERROR
};

/* Delivered on the context dispatcher. Text is borrowed until the callback returns. */
typedef void (MC_CALL *mc_log_fn)(void* user_data, mc_log_level level, mc_string_view tag, mc_string_view text);

/* Queue a diagnostic from any thread. The logger copies both UTF-8 views. */
MC_API mc_result MC_CALL mc_log_write(mc_log_level level, mc_string_view tag, mc_string_view text);

#ifdef __cplusplus
}
#endif

#endif
