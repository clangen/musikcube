#ifndef MUSIKCORE_MUSIKCORE_H
#define MUSIKCORE_MUSIKCORE_H

#include <musikcore/abi.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mc_context mc_context;

typedef struct mc_context_options {
    uint32_t struct_size;
    uint32_t requested_abi_version;
    mc_string_view data_directory;
    mc_string_view plugin_directory;
    uint32_t flags;
} mc_context_options;

#define MC_CONTEXT_OPTIONS_DEFAULT { sizeof(mc_context_options), 0, { NULL, 0 }, { NULL, 0 }, 0 }

/* Null options use defaults. Directory views are copied during creation. */
/* Output slots must not already own handles. A second context returns MC_ERROR_BUSY. */
MC_API mc_result MC_CALL mc_context_create(
    const mc_context_options *options, mc_context **out_context, mc_error **out_error);

/* Consumes the context and clears the slot. Null is accepted. */
MC_API void MC_CALL mc_context_destroy(mc_context **context);

#ifdef __cplusplus
}
#endif

#endif
