#ifndef MUSIKCORE_PLUGIN_STREAM_H
#define MUSIKCORE_PLUGIN_STREAM_H

#include <musikcore/plugin/audio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_PLUGIN_CAPABILITY_STREAM "org.musikcube.stream"

typedef uint32_t mc_stream_open_flags;
enum {
    MC_STREAM_OPEN_READ = 1u,
    MC_STREAM_OPEN_WRITE = 2u,
    MC_STREAM_OPEN_CREATE = 4u,
    MC_STREAM_OPEN_TRUNCATE = 8u,
    MC_STREAM_OPEN_APPEND = 16u
};

enum {
    MC_STREAM_READABLE = 1u,
    MC_STREAM_WRITABLE = 2u,
    MC_STREAM_SEEKABLE = 4u,
    MC_STREAM_LENGTH_KNOWN = 8u,
    MC_STREAM_CAN_PREFETCH = 16u
};

typedef uint32_t mc_read_state;
enum { MC_READ_MORE = 0, MC_READ_END = 1 };

/* describes the opened resource so a decoder can choose its input strategy. */
typedef struct mc_stream_info {
    /* available bytes. initialize with MC_STREAM_INFO_DEFAULT. */
    uint32_t struct_size;

    /* bitmask of MC_STREAM_READABLE and related capabilities. zero by default. */
    uint32_t flags;

    /* total size in bytes when MC_STREAM_LENGTH_KNOWN is set. */
    uint64_t length_bytes;

    /* provider-owned URI. valid until close or destroy. may contain credentials. */
    mc_string_view uri;

    /* provider-owned MIME type. empty when unknown. valid until close or destroy. */
    mc_string_view content_type;

    /* lowercase extension without a dot. same lifetime as content_type. may be empty. */
    mc_string_view extension;
} mc_stream_info;

#define MC_STREAM_INFO_DEFAULT { sizeof(mc_stream_info), 0, 0, { NULL, 0 }, { NULL, 0 }, { NULL, 0 } }

/* one byte stream. ordinary operations run serially on a worker. interrupt
is the only operation that may race them. optional error slots receive host-owned diagnostics. */
typedef struct mc_stream_vtable {
    /* available bytes. initialize with MC_STREAM_VTABLE_DEFAULT. */
    uint32_t struct_size;

    /* release the instance after all calls finish, including after failed open. */
    void (MC_CALL* destroy)(void* instance);

    /* open once. copy any URI bytes needed after the call. create, truncate and append require write. */
    mc_result (MC_CALL* open)(void* instance, mc_string_view uri, mc_stream_open_flags flags, mc_error** error);

    /* finish IO; allows reporting flush failures before destruction. */
    mc_result (MC_CALL* close)(void* instance);

    /* latch cancellation, including before open, and wake blocked IO. repeat calls are safe. */
    void (MC_CALL* interrupt)(void* instance);

    /* fill caller-initialized information for an open stream. */
    mc_result (MC_CALL* get_info)(void* instance, mc_stream_info* info);

    /* report bytes transferred and EOF separately. a positive request must make
    progress, reach EOF, or fail. EOF can accompany the last bytes. */
    mc_result (MC_CALL* read)(void* instance, void* dst, size_t capacity_bytes,
        size_t* read_bytes, mc_read_state* state, mc_error** error);

    /* report bytes transferred even on failure so retries cannot duplicate data. */
    mc_result (MC_CALL* write)(void* instance, const void* src, size_t requested_bytes,
        size_t* written_bytes, mc_error** error);

    /* move to an absolute byte offset. return MC_ERROR_UNSUPPORTED for a nonseekable stream,
    or for a position beyond the available bytes of a live stream with unknown length. */
    mc_result (MC_CALL* seek)(void* instance, uint64_t absolute_byte_offset, mc_error** error);

    /* report the next byte offset. return MC_ERROR_UNSUPPORTED if unavailable. */
    mc_result (MC_CALL* tell)(void* instance, uint64_t* byte_offset);
} mc_stream_vtable;

#define MC_STREAM_VTABLE_DEFAULT { sizeof(mc_stream_vtable), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }

/* pairs a byte source with its operations. decoders use a host-owned stream
from open through destruction; they must leave close and destroy to its owner. */
typedef struct mc_stream {
    /* available bytes. initialize with MC_STREAM_DEFAULT. */
    uint32_t struct_size;

    /* opaque provider state. a default handle has no instance. */
    void* instance;

    /* immutable operations, valid for the instance lifetime. default null. */
    const mc_stream_vtable* api;
} mc_stream;

#define MC_STREAM_DEFAULT { sizeof(mc_stream), NULL, NULL }

/* selects a URI provider without opening the connection. separate creation
allows the host to publish the instance for cancellation before a blocking open. */
typedef struct mc_stream_factory {
    /* available bytes. initialize with MC_STREAM_FACTORY_DEFAULT. */
    uint32_t struct_size;

    /* plugin-owned state shared by concurrent factory calls. may be null. */
    void* context;

    /* immutable operations shared by instances until plugin shutdown. required. */
    const mc_stream_vtable* instance_api;

    /* inspect call-scoped hints without IO. write zero or one to supported. */
    mc_result (MC_CALL* can_handle)(void* context, const mc_media_hint* hint, uint32_t* supported);

    /* create an unopened instance in an empty slot. on failure leave the slot empty. */
    mc_result (MC_CALL* create)(
        void* context, const mc_plugin_services* services, void** out_instance, mc_error** error);
} mc_stream_factory;

#define MC_STREAM_FACTORY_DEFAULT { sizeof(mc_stream_factory), NULL, NULL, NULL, NULL }

#ifdef __cplusplus
}
#endif
#endif
