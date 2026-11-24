#ifndef MUSIKCORE_PLUGIN_TAG_H
#define MUSIKCORE_PLUGIN_TAG_H

#include <musikcore/plugin/metadata.h>
#include <musikcore/plugin/services.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_TAG_READER "org.musikcube.tag_reader"

/* reads file tags into a host-owned metadata writer. each read uses its own
instance and can run concurrently with reads on other indexer workers. */
typedef struct mc_tag_reader_vtable {
    /* available bytes. initialize with MC_TAG_READER_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* release the instance after its read completes. */
    void (MC_CALL* destroy)(void* instance);

    /* read the UTF-8 file path. path and writer expire when this call returns. */
    mc_result (MC_CALL* read)(void* instance, mc_string_view uri,
        const mc_metadata_writer* writer, mc_error** error);
} mc_tag_reader_vtable;

#define MC_TAG_READER_VTABLE_DEFAULT { sizeof(mc_tag_reader_vtable), NULL, NULL }

/* selects a tag reader by extension and creates independent reader instances. */
typedef struct mc_tag_reader_factory {
    /* available bytes. initialize with MC_TAG_READER_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned factory state, or null. */
    void* context;

    /* immutable instance operations, valid until plugin shutdown. */
    const mc_tag_reader_vtable* instance_api;

    /* write one if this reader handles the extension, or zero otherwise. */
    mc_result (MC_CALL* can_read)(void* context, mc_string_view extension, uint32_t* supported);

    /* write the new reader to instance; on failure set it to null. */
    mc_result (MC_CALL* create)(void* context, const mc_plugin_services* services,
        void** instance, mc_error** error);
} mc_tag_reader_factory;

#define MC_TAG_READER_FACTORY_DEFAULT { sizeof(mc_tag_reader_factory), NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
