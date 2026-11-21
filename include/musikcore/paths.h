#ifndef MUSIKCORE_PATHS_H
#define MUSIKCORE_PATHS_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t mc_path_kind;
enum {
    MC_PATH_APPLICATION,
    MC_PATH_HOME,
    MC_PATH_DATA
};

typedef void (MC_CALL *mc_path_fn)(void* user_data, mc_string_view path);

/* Visit a UTF-8 directory path inline. The view expires on return from the
   callback. Reading the data directory attempts to create it if needed. */
MC_API mc_result MC_CALL mc_path_read(mc_path_kind kind, mc_path_fn read, void* user_data);

#ifdef __cplusplus
}
#endif

#endif
