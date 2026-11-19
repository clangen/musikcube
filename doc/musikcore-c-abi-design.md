# musikcore C ABI and C SDK design

Status: proposed

Related documents:

- [High-level architecture and sequencing](musikcore-c-abi-hld.md)
- [Low-level implementation design and task list](musikcore-c-abi-lld.md)
- [Unit and integration testing design](testing-design.md)

## Summary

musikcore should expose a stable C ABI that supports two related use cases:

1. Applications and language bindings consuming musikcore as a shared library.
2. Plugins providing decoders, encoders, data streams, audio outputs, DSPs, and
   other extensions to musikcore.

The first use case is the **public C API**. The second is the **C SDK**. They
share primitive types, error handling, ownership rules, audio buffer formats,
and ABI versioning, but they are separate interfaces because calls travel in
opposite directions.

The C ABI is the binary contract. Idiomatic C++, Rust, Swift, C#, Python, and
other APIs should be wrappers over that contract. musikcube should use a C++
wrapper over the public C API so that the ABI is exercised by a production
consumer without spreading C-style resource management throughout the UI.

This proposal replaces the existing C API and C++ SDK ABI. Compatibility with
the existing `mcsdk_*` API and C++ plugin interfaces is not a goal.

## Motivation

The current C API is a thin projection of C++ interfaces. It demonstrates that
musikcore can be consumed through C, but it has several limitations as a
long-lived public ABI:

- Handles, ownership, and borrowed lifetimes are not consistently expressed.
- Fallible operations use inconsistent return conventions.
- C++ exceptions are not contained at every exported boundary.
- Callbacks do not consistently carry user context and may execute on internal
  threads.
- The metadata API encourages repeated lookups and string copies.
- The track-list interface does not expose the async cache behavior used by
  musikcube.
- Version negotiation cannot reliably detect a header/library mismatch.

The current plugin entry points use C linkage, but return C++ interface
pointers such as `IDecoderFactory*` and `IOutput*`. Consequently, plugins still
depend on C++ vtable layout, compiler ABI, exception behavior, and compatible
toolchains. A plugin cannot be implemented naturally in C or another language
that can implement a C ABI.

The new design should make `libmusikcore` a practical shared library and make
the plugin system genuinely language-neutral.

## Goals

- Provide a stable ABI for Windows, macOS, and Linux shared libraries.
- Permit consumers and plugins to be implemented in any language that can call
  or export C functions.
- Prevent C++ types, exceptions, allocators, and standard-library objects from
  crossing the ABI.
- Make object ownership, string lifetime, and thread behavior unambiguous.
- Support async library queries and track metadata loading without blocking a
  UI thread.
- Avoid per-field allocations and redundant string copies for metadata pages.
- Keep PCM audio transfer zero-copy in performance-sensitive paths.
- Support compatible ABI evolution within a major ABI version.
- Make an idiomatic C++ wrapper suitable for migrating musikcube.
- Allow the existing C++ implementation to be adapted incrementally rather
  than rewritten.

## Non-goals

- Preserving source or binary compatibility with the existing C API.
- Preserving the current C++ plugin ABI.
- Exposing every internal C++ class or query type.
- Making untrusted in-process plugins safe. Plugins loaded into the musikcore
  process remain trusted native code.
- Defining a network protocol. A future musikcore daemon can be layered over
  the public API, but it is not the shared-library ABI.
- Guaranteeing that every operation is real-time safe. Real-time-safe entry
  points will be identified explicitly.

## Architecture

The proposed layers are:

```text
                       musikcore C++ implementation
                                  |
                     internal C++/C adapter layer
                                  |
             +--------------------+--------------------+
             |                                         |
       public C API                                C plugin SDK
   (applications call core)                    (core calls plugins)
             |                                         |
     language wrappers                       plugin language wrappers
             |
          musikcube
```

The public API and SDK share the definitions in `abi.h`, but neither exposes
the other's internal handles or function tables.

Proposed installed headers:

```text
include/musikcore/abi.h
include/musikcore/musikcore.h
include/musikcore/plugin.h
include/musikcore/musikcore.hpp
include/musikcore/plugin.hpp
```

- `abi.h` contains calling conventions, symbol visibility, result codes,
  strings, slices, audio formats, and version helpers.
- `musikcore.h` contains the application-facing API.
- `plugin.h` contains host and provider function tables.
- The C++ headers are optional, header-only ownership and type-safety wrappers.

Private implementation lives under `src/musikcore/ffi`, divided into `common`,
`consumer`, `provider`, and `cpp`. This hierarchy is intentionally not mirrored
in the installed headers.

## Shared ABI foundation

### Language and platform baseline

The installed C headers should compile as C11 and C++ without configuration
macros supplied by the consumer. Public declarations always use C linkage when
compiled as C++. Export/import visibility is independent from language
linkage.

The ABI should use:

- Fixed-width integers from `<stdint.h>`.
- `size_t` for in-memory buffer and array sizes.
- ABI enums represented as fixed-width integer typedefs plus named constants;
  the size of a C `enum` is implementation-defined.
- `uint8_t` or `int32_t` for ABI booleans rather than compiler-specific C++
  `bool` layouts in extensible structures.
- UTF-8 for all text.
- Seconds as `double` for playback positions and durations unless a function
  explicitly uses sample frames.
- Natural platform alignment with no public bitfields, packed structures, or
  flexible array members.

`long`, C++ references, templates, STL types, exceptions, and compiler-defined
class layouts must not appear in installed C headers.

### Names and symbols

The new API uses the `mc_` prefix. Public library functions are exported with
`MC_API`; plugin entry points use `MC_PLUGIN_API`. A single `MC_CALL` macro
defines the calling convention for exported functions and all cross-boundary
function pointers. The initial ABI uses the platform C calling convention.

The shared library exports only documented C ABI symbols. Internal C++ symbols
use hidden visibility where supported.

Platform packaging should encode the ABI major version:

- Linux: `libmusikcore.so.1` with an appropriate SONAME.
- macOS: `libmusikcore.1.dylib` with compatibility/current versions.
- Windows: `musikcore-1.dll` plus an import library.

### ABI versioning

The ABI uses an independent major and minor version, rather than the musikcube
application version.

- Encode the major version in the upper 16 bits and the minor version in the
  lower 16 bits. The initial experimental version is 0.1.
- A major version change permits incompatible changes.
- A minor version may only add functions, enum values where documented, or
  fields at the end of extensible structures.
- Patch releases do not change the ABI contract.

The public library exports runtime version and feature queries:

```c
MC_API uint32_t mc_abi_version(void);
MC_API mc_result mc_has_feature(
    mc_context *context,
    mc_feature feature,
    uint8_t *out_supported);
```

Every caller-initialized options structure and every cross-boundary function
table begins with a `uint32_t struct_size`. Callers zero-initialize the
structure and set `struct_size` to its size in bytes. Implementations read
only fields that fit entirely within the supplied size. Newly added fields
must have safe zero defaults.

Opaque library-owned objects do not expose layouts and therefore do not need a
size field.

### Results and errors

Every fallible operation returns `mc_result`:

```c
typedef int32_t mc_result;

enum {
    MC_OK = 0,
    MC_ERROR_INVALID_ARGUMENT = 1,
    MC_ERROR_NOT_FOUND = 2,
    MC_ERROR_UNSUPPORTED = 3,
    MC_ERROR_BUSY = 4,
    MC_ERROR_CANCELLED = 5,
    MC_ERROR_IO = 6,
    MC_ERROR_PLUGIN = 7,
    MC_ERROR_INTERNAL = 8,
    MC_ERROR_BUFFER_TOO_SMALL = 9
};
```

Public functions validate handles and required pointers. No C++ exception may
cross an exported function or plugin function-table call. Exceptions are
translated into `mc_result` values at the boundary.

Detailed failures are represented by an optional owned `mc_error` handle.
Error objects are created only on failure and expose a code, UTF-8 message, and
optional plugin or subsystem identifier. Async completion events carry their
own error object; they do not depend on thread-local state.

```c
MC_API mc_result MC_CALL mc_error_get_code(const mc_error *error);
MC_API mc_string_view MC_CALL mc_error_get_message(const mc_error *error);
MC_API void MC_CALL mc_error_destroy(mc_error **error);
```

An API may omit an error output when its result code is sufficiently specific.
The convention must be consistent within each API family.

An error output slot must not already own an error. If error details cannot
be allocated, the function still returns its result code and clears the slot.
The message view stays valid until the caller destroys the error.

### Handles and ownership

Library-owned objects are incomplete types referenced by pointer:

```c
typedef struct mc_context mc_context;
typedef struct mc_event mc_event;
typedef struct mc_track_page mc_track_page;
typedef struct mc_track_list mc_track_list;
typedef struct mc_audio_buffer mc_audio_buffer;
```

Naming communicates ownership:

- `create`, `open`, `clone`, and successful async result transfer create owned
  handles.
- `get` returns a borrowed handle or value unless explicitly documented.
- `retain` creates another owned reference when shared ownership is supported.
- `destroy` consumes an owned reference and accepts a pointer-to-pointer so it
  can clear the caller's handle.

Every borrowed handle states which parent keeps it alive. Public documentation
must not rely on callers knowing the underlying C++ ownership model.

### Memory allocation

Memory must be released by the component that allocated it.

- Applications destroy library objects through `mc_*_destroy`.
- Plugin-owned instances are destroyed through their plugin function table.
- Core-owned buffers given to plugins are released or recycled through host
  functions.
- Plugins do not return memory that the core must release with the process
  allocator.

If custom allocation is required, the host supplies an allocator function
table with matching allocate, reallocate, and free operations. Allocator calls
are not real-time safe unless explicitly stated.

### Strings and slices

Non-owning text uses an explicit UTF-8 view:

```c
typedef struct mc_string_view {
    const char *data;
    size_t length;
} mc_string_view;
```

Views are not required to be null terminated. Every function documents whether
a view is valid for the duration of a call, an owning parent object, or a
longer-lived static table.

Caller-buffer functions return the required capacity, including a null
terminator, even when the supplied buffer is too small. This permits a single
call with a stack or reusable buffer in the common case and an exact retry for
unusually long values.

A null destination with zero capacity queries the required size. A non-null
destination with insufficient capacity returns `MC_ERROR_BUFFER_TOO_SMALL`
and leaves the buffer intact. Byte copies do not add a terminator.

Collections use typed pointer/count pairs or accessor functions. Raw arrays do
not transfer ownership unless explicitly documented.

### Threading and reentrancy

Every API family is classified as one of:

- Thread-safe.
- Confined to the context owner thread.
- Confined to an internal worker thread.
- Real-time safe.

Application code is never invoked while an internal musikcore mutex is held.
The core copies or retains the necessary state, releases its locks, and then
delivers an event or invokes a provider function.

Destruction specifies whether it waits for in-flight work. Where waiting would
be dangerous, shutdown is a separate async operation followed by destruction
after a terminal event.

## Public C API

### Purpose

The public C API allows an application to embed musikcore as a shared library.
It exposes stable domain operations rather than mirroring internal C++ classes.
The initial API should cover the operations required by musikcube and common
embedding scenarios. Advanced facilities can be added in later ABI minor
versions.

### Context and runtime lifecycle

The public entry point is an owned context:

```c
typedef struct mc_context_options {
    uint32_t struct_size;
    uint32_t requested_abi_version;
    mc_string_view data_directory;
    mc_string_view plugin_directory;
    uint32_t flags;
} mc_context_options;

MC_API mc_result MC_CALL mc_context_create(
    const mc_context_options *options,
    mc_context **out_context,
    mc_error **out_error);

MC_API void MC_CALL mc_context_destroy(mc_context **context);
```

Global initialization should be hidden behind context reference counting. A
consumer should not need to coordinate a separate process-wide environment
lifecycle. Multiple contexts are supported only if the implementation can
define their database, plugin, audio-device, and global preference isolation.
Otherwise, creation of a second context returns `MC_ERROR_BUSY` until true
multi-context semantics are implemented.

Initialize explicit options with `mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;`
before overriding fields. ABI0 accepts null options and defaults omitted fields. A zero requested
version selects the current ABI. Explicit requests must match the current
major and specify a minor no newer than the library's. Directory views are
copied during creation. The first lifecycle slice stores these roots for
later runtime initialization.

### Async operations and events

Async operations return a request ID immediately:

```c
typedef uint64_t mc_request_id;

MC_API mc_result mc_request_cancel(
    mc_context *context,
    mc_request_id request);
```

The canonical completion mechanism is a context event queue rather than a
callback from an arbitrary internal thread:

```c
MC_API mc_result mc_context_next_event(
    mc_context *context,
    uint32_t timeout_ms,
    mc_event **out_event);

MC_API mc_event_type mc_event_get_type(const mc_event *event);
MC_API mc_request_id mc_event_get_request_id(const mc_event *event);
MC_API void mc_event_destroy(mc_event **event);
```

Calling `mc_context_next_event` delivers events on the caller's thread. A
language wrapper can run an event-pump thread and translate events into native
futures, tasks, channels, signals, or async streams.

The initial contract permits one active event consumer per context. Other
documented thread-safe API calls may execute concurrently with that consumer.
Context shutdown wakes a blocked event consumer before releasing its resources.
Inputs to accepted async operations are copied or retained before the initiating
function returns; callers do not need to keep query string views alive.

The event contract must define:

- Exactly one terminal completion event for each accepted async request.
- Best-effort cancellation and how cancellation races with completion.
- Which progress and state events may be coalesced.
- Queue capacity and backpressure behavior.
- Ordering within a request and between playback state events.
- Context destruction behavior when events remain queued.

An optional wakeup handle or scheduler hook may be added for native event-loop
integration, but must not replace the portable queue API.

### Library queries and metadata snapshots

Queries return immutable result pages rather than requiring many calls through
live track objects. A query specifies the desired metadata fields and paging
parameters:

```c
typedef struct mc_track_query {
    uint32_t struct_size;
    mc_string_view search;
    const mc_string_view *fields;
    size_t field_count;
    size_t offset;
    size_t limit;
    uint32_t flags;
} mc_track_query;

MC_API mc_result mc_library_query_tracks_async(
    mc_context *context,
    const mc_track_query *query,
    mc_request_id *out_request);
```

The completion event initially owns an `mc_track_page`. The consumer explicitly
takes ownership; destroying an event without taking the page destroys the page
as well:

```c
MC_API mc_result mc_event_take_track_page(
    mc_event *event,
    mc_track_page **out_page);
```

The same take-or-destroy convention applies to detailed async errors and other
owned event payloads. A page stores all requested strings in page-owned storage:

```c
MC_API size_t mc_track_page_get_count(const mc_track_page *page);
MC_API int64_t mc_track_page_get_id(
    const mc_track_page *page,
    size_t row);
MC_API mc_string_view mc_track_page_get_string(
    const mc_track_page *page,
    size_t row,
    size_t field);
MC_API int64_t mc_track_page_get_int64(
    const mc_track_page *page,
    size_t row,
    size_t field,
    int64_t default_value);
MC_API void mc_track_page_destroy(mc_track_page **page);
```

String views remain valid until the page is destroyed. The implementation can
use one arena or compact backing allocation for the entire page. This avoids
two-call string retrieval and per-field ownership management.

Standard metadata names remain UTF-8 strings so plugins and future versions can
add fields without extending an ABI enum. Wrappers may provide enums or typed
constants for common fields.

### Track lists and UI cache windows

Persistent track lists represent ordered track identities. Metadata loading is
separate from identity and ordering.

The API must support the behavior currently required by musikcube:

- Retrieve IDs without loading metadata.
- Request an async metadata page around a visible range.
- Set a suggested cache window size.
- Receive completion or invalidation events.
- Observe loading, loaded, missing, and failed metadata states.
- Edit a list through a transactional editor or batch operation.

A visible-range request should produce a snapshot page. It must not cause
individual `get_track` calls to block on database work.

Track-list editing should accept a batch of operations and commit atomically
where possible. It should not expose an editor object whose lifetime implicitly
locks an internal container.

### Playback and queue

Playback operations are command-oriented:

- Play a list at an index.
- Pause, resume, stop, next, previous, and seek.
- Set volume, mute, repeat, shuffle, replay gain, and transport mode.
- Read a coherent playback snapshot.
- Edit or replace the queue in batches.

Playback state changes, playing-track changes, position discontinuities, queue
changes, and failures are delivered through the event queue. Frequently polled
position can be read synchronously as a small value; it does not require an
event for every timer tick.

A playback snapshot groups related fields so wrappers do not observe a state,
track, duration, and queue index from different transitions.

### Indexer

The indexer API supports:

- Listing, adding, and removing indexed paths.
- Starting full, local, rebuild, and source scans.
- Stopping an active scan.
- Reading a coherent indexer snapshot.
- Receiving state, progress, source, and terminal events.

Progress events may be coalesced. State transitions and terminal outcomes may
not be dropped.

### Preferences

Preferences expose typed getters and setters for booleans, fixed-width
integers, doubles, and UTF-8 strings. A preference namespace is an owned or
context-borrowed handle with documented lifetime.

Batch updates are preferred so a wrapper can make several changes and save
once. String reads use either caller buffers with required-size semantics or an
immutable snapshot owned by the preference result.

### Advanced audio API

The public API may expose data streams, decoders, encoders, audio outputs, and
buffers for advanced consumers. These types use the same formats as the plugin
SDK but remain opaque application handles.

PCM transfer must be frame-oriented and zero-copy:

```c
typedef int32_t mc_sample_format;

enum {
    MC_SAMPLE_F32_NATIVE = 1
};

typedef struct mc_audio_format {
    uint32_t struct_size;
    uint32_t sample_rate;
    uint32_t channels;
    mc_sample_format sample_format;
    uint32_t channel_layout;
} mc_audio_format;

typedef struct mc_audio_buffer_view {
    uint32_t struct_size;
    void *data;
    size_t frame_count;
    size_t frame_capacity;
    mc_audio_format format;
} mc_audio_buffer_view;
```

Mapping a buffer pins its storage until it is unmapped or recycled. The API
must distinguish frame count from samples-per-channel and byte capacity.

Raw database connections and statements should not be part of the initial
stable surface. They expose implementation details and create a long-term ABI
commitment. If retained, they belong in an explicitly advanced module with
separate stability guarantees.

### Public C++ wrapper

`musikcore.hpp` provides move-only or reference-counted RAII types, translates
error results into an explicit C++ result type or documented exceptions, and
maps the event queue to the application's preferred dispatch mechanism.

The wrapper should provide `std::string`, `std::string_view`, spans, and scoped
ownership without accessing internal musikcore C++ classes. musikcube should
depend on this wrapper rather than call the C functions throughout UI code.

The wrapper is source-level convenience. Only the C ABI is promised to be
binary stable.

## C SDK

### Purpose

The C SDK allows a shared library to provide capabilities to musikcore without
implementing C++ virtual interfaces. It is a reverse FFI: the plugin supplies
function tables that musikcore invokes, and musikcore supplies a host function
table that the plugin invokes.

The SDK should initially support:

- Decoder factories and decoders.
- Encoder factories and blocking or streaming encoders.
- Data-stream factories and streams.
- Audio outputs and output devices.
- DSP processors.
- Tag readers and tag stores.
- Indexer sources.

Visualizer, playback-remote, server, and configuration-schema capabilities can
be added after the foundational media interfaces are proven.

### Single plugin entry point

Each plugin exports one well-known symbol:

```c
MC_PLUGIN_API mc_result mc_plugin_get_api(
    uint32_t host_abi_version,
    const mc_host_api *host,
    mc_plugin_api *out_plugin,
    mc_error **out_error);
```

The entry point negotiates the ABI and returns plugin metadata plus lifecycle
functions. It replaces `GetPlugin`, `GetDecoderFactory`, `SetEnvironment`,
`SetPreferences`, `SetDebug`, and similar exported symbols.

Before calling the entry point, the host zero-initializes `out_plugin` and sets
its `struct_size` to the size understood by the host. The plugin writes no more
than that size and reports its own ABI requirements through the fields that fit.

Only diagnostic host functions such as error construction and bootstrap logging
may be called from the entry point. The plugin must not call other host services
before initialization begins. The host table remains valid until plugin
shutdown returns.

### Plugin descriptor and lifecycle

The plugin API contains stable identity and lifecycle operations:

```c
typedef struct mc_plugin_api {
    uint32_t struct_size;
    uint32_t plugin_abi_version;
    mc_string_view id;
    mc_string_view name;
    mc_string_view version;
    mc_string_view author;
    uint64_t capabilities;
    void *plugin_context;

    mc_result (*initialize)(void *plugin_context, mc_error **out_error);
    void (*shutdown)(void *plugin_context);
} mc_plugin_api;
```

The proposed load sequence is:

1. Discover a candidate shared library.
2. Load it and resolve `mc_plugin_get_api`.
3. Exchange ABI versions and sized function tables.
4. Validate identity and requested capabilities.
5. Call plugin initialization, during which the plugin registers factories
   through the host API.
6. Make registered factories available for instance creation.
7. Create and use provider instances.
8. Stop new instance creation during shutdown.
9. Destroy all provider instances.
10. Call plugin shutdown and unload the library.

The core must not unload a plugin while any of its function tables, string
views, factories, or provider instances can still be used.

A sidecar manifest may be added for discovery without executing plugin code.
The entry point remains authoritative for ABI validation.

### Host API

The host function table exposes narrowly scoped services:

```c
typedef struct mc_host_api {
    uint32_t struct_size;
    uint32_t abi_version;
    void *host_context;

    void (*log)(
        void *host_context,
        mc_log_level level,
        mc_string_view component,
        mc_string_view message);

    mc_result (*create_error)(
        void *host_context,
        mc_error_code code,
        mc_string_view message,
        mc_error **out_error);

    mc_result (*create_audio_buffer)(
        void *host_context,
        const mc_audio_format *format,
        size_t frame_capacity,
        mc_audio_buffer **out_buffer);

    mc_result (*map_audio_buffer)(
        void *host_context,
        mc_audio_buffer *buffer,
        mc_audio_buffer_view *out_view);

    void (*unmap_audio_buffer)(
        void *host_context,
        mc_audio_buffer *buffer);

    mc_result (*open_preferences)(
        void *host_context,
        mc_string_view plugin_id,
        mc_preferences **out_preferences);

    mc_result (*register_decoder_factory)(
        void *host_context,
        const mc_decoder_factory *factory);
} mc_host_api;
```

Additional registration functions are appended in ABI minor versions. A
plugin checks `struct_size` and feature support before using optional host
functions.

When a provider returns detailed failure information, it creates the
`mc_error` through the host table. The resulting error is core-owned and can be
passed safely to public consumers. Plugins must not allocate their own
`mc_error` objects or assume their layout.

The host should provide capability-specific sub-tables if the single table
becomes large. Plugins receive only supported services and must handle optional
services being absent.

### Provider objects and function tables

Each provider object consists of plugin-owned instance state and a static or
long-lived function table:

```c
typedef struct mc_decoder_vtable {
    uint32_t struct_size;

    void (*destroy)(void *instance);

    mc_result (*open)(
        void *instance,
        const mc_stream *stream,
        mc_error **out_error);

    mc_result (*decode)(
        void *instance,
        mc_audio_buffer *buffer,
        uint8_t *out_exhausted,
        mc_error **out_error);

    mc_result (*seek)(
        void *instance,
        double requested_seconds,
        double *out_actual_seconds,
        mc_error **out_error);

    mc_result (*get_duration)(
        void *instance,
        double *out_seconds);

    mc_result (*set_preferred_sample_rate)(
        void *instance,
        uint32_t sample_rate);
} mc_decoder_vtable;

typedef struct mc_decoder {
    void *instance;
    const mc_decoder_vtable *vtable;
} mc_decoder;
```

The plugin allocates and destroys `instance`. The vtable remains valid until
the instance is destroyed. musikcore never casts or frees plugin state.

Factories follow the same pattern and advertise supported MIME types, file
extensions, URI schemes, sample formats, or other capabilities without
constructing an instance.

Function-table calls are wrapped by internal C++ adapters such as
`CDecoderAdapter`. This permits the existing playback implementation to
continue using internal C++ interfaces during migration.

### Streams

Streams are bidirectional interfaces because a plugin decoder may consume a
core-provided stream while a plugin data-stream factory returns a stream to the
core.

Stream operations use `uint64_t` positions and explicit byte counts. Reads and
writes return both a result code and the number of bytes transferred. EOF is a
defined outcome rather than an overloaded negative length.

The stream contract defines:

- Whether reads may be partial.
- Interrupt and cancellation behavior.
- Seekability and length availability.
- Thread confinement.
- URI and content-type view lifetimes.
- Whether prefetching is useful and who schedules it.

### Decoders and encoders

Decoder operations consume streams and fill host-owned audio buffers. A decoder
must not retain a mapped pointer after returning from `decode` unless the buffer
contract explicitly grants that lifetime.

Encoder input is also a buffer view. Encoded output should use one of two
models:

- A blocking encoder writes to a host-provided stream.
- A streaming encoder writes into a host-provided byte sink or returns a
  plugin-owned immutable packet with an explicit destroy operation.

Returning a raw `char **` without ownership metadata is not permitted.

Encoder and decoder factories report capabilities before creation. Capability
queries must not require opening a media stream when MIME type, extension, or
container metadata is already sufficient.

### Audio outputs and real-time behavior

Audio outputs are the most restrictive provider interface. The SDK must mark
the callback or submission functions that execute on a real-time audio thread.

Within a real-time call, a plugin and the core must not:

- Allocate or free general heap memory.
- Perform filesystem or network I/O.
- Wait on an unbounded mutex or condition variable.
- Log synchronously.
- Invoke arbitrary application callbacks.
- Call host functions not explicitly marked real-time safe.

Buffer ownership uses an explicit state machine such as available, submitted,
playing, and recyclable. Completion is reported through a real-time-safe sink
or lock-free notification path, then converted to a normal context event away
from the audio thread.

Device enumeration returns an immutable snapshot. Device IDs are stable opaque
UTF-8 values when the underlying platform permits it.

### DSP and analyzers

DSP processing receives mutable, mapped audio frames with an explicit format.
The process function is real-time safe and does not change buffer capacity.
Format negotiation and parameter changes occur outside the real-time process
call.

Analyzers and visualizers should consume snapshots or bounded frame windows.
They must not retain transient audio pointers. Dropping analyzer updates under
load is preferable to blocking playback.

### Tag readers and indexer sources

Tag readers populate a host-provided metadata writer rather than returning a
plugin-specific map. Metadata writes accept UTF-8 key/value views and binary
artwork with explicit lengths.

Indexer sources receive a writer function table and cancellation token. They
must tolerate cancellation between items and complete with an explicit commit
or rollback result. No database connection or C++ query object crosses the
plugin boundary.

### Plugin preferences and configuration

Plugins receive preferences scoped to their stable plugin ID. Preferences are
accessed through the shared C preference API or a restricted host sub-table.

Configuration UI should be described as data rather than a plugin-created UI
object. A future schema capability can return immutable field descriptors,
defaults, constraints, and localized keys. Applications remain responsible for
rendering the configuration interface.

### C++ SDK wrapper

`plugin.hpp` provides optional C++ base classes or adapters that construct the
C function tables and translate calls to C++ methods. It must catch exceptions
inside every generated trampoline.

The wrapper reduces boilerplate for existing C++ plugins, but plugins link to
the C ABI and do not exchange C++ objects with musikcore. Its source API may
evolve independently; only the generated C function tables are ABI stable.

## Events versus callbacks

The public API uses a context event queue because application runtimes often
require callbacks to be delivered on a runtime-owned thread.

The plugin SDK uses function tables because the core must invoke provider
implementations directly, including in performance-sensitive paths. Provider
function tables always carry an instance or context pointer and have explicit
threading contracts.

Convenience wrappers may present callbacks, futures, signals, or coroutines,
but those are not additional binary contracts.

## Performance requirements

The new ABI should preserve current playback and UI behavior. In particular:

- Scrolling must not synchronously wait for metadata queries.
- A visible metadata page should require one query and one page allocation, not
  one allocation per field.
- Metadata string access from a page is a borrowed pointer/length operation.
- Playback commands and small state reads add only constant wrapper overhead.
- PCM data is not copied merely to cross the ABI.
- No FFI call is made per audio sample.
- Event delivery does not occur while an internal mutex is held.

Benchmarks should compare the current musikcube implementation with the C++
wrapper implementation for search, initial list display, rapid scrolling,
queue editing, playback startup, and sustained audio output.

## Compatibility and feature discovery

The ABI major version is the primary compatibility boundary. Minor-version
features are discovered through structure sizes and `mc_has_feature`.

Unknown event types and appended enum values must be safely ignorable where the
contract permits. Callers must not assume a closed enum unless documented.

Plugin compatibility is checked before initialization. A plugin compiled for
an older compatible minor version receives only the host table size it
understands. A plugin requiring a newer feature rejects initialization with a
specific error.

Public ABI and plugin ABI versions may eventually advance independently. The
first implementation may use one shared version while the contracts are
developed together.

## Testing and validation

The ABI requires tests beyond normal C++ unit coverage:

- Compile and run a pure-C consumer on Windows, macOS, and Linux.
- Compile a consumer against an older minor header and run it with a newer
  compatible library.
- Load an older compatible plugin with a newer host.
- Reject incompatible ABI majors without invoking plugin lifecycle methods.
- Exercise null, invalid, stale, and double-destroyed handles under sanitizers.
- Test context teardown with queued events and in-flight requests.
- Test cancellation races and exactly-once terminal completion.
- Test plugin unload with factories and instances in flight.
- Run callback and event paths under ThreadSanitizer where supported.
- Fuzz sized structures, plugin function tables, metadata pages, and malformed
  provider results.
- Verify that no C++ symbols are part of the documented export surface.
- Verify shared-library SONAME/install-name/import-library metadata in release
  packaging.

At least one sample decoder plugin should be implemented in pure C. A second
sample in a non-C++ language is desirable after the ABI stabilizes because it
will expose assumptions hidden by the C++ adapters.

## Migration plan

### Phase 0: inventory and contract tests

- Inventory every musikcube dependency on internal musikcore classes, signals,
  and query types.
- Inventory all current plugin interfaces and exported setup functions.
- Define behavioral tests for lifecycle, playback state, async track loading,
  and plugin discovery before replacing interfaces.

### Phase 1: shared ABI and context

- Add `abi.h`, result codes, errors, strings, sized structures, and visibility
  rules.
- Implement context creation, shutdown, version negotiation, and event queues.
- Establish shared-library ABI naming and export checks on all platforms.

### Phase 2: library queries and a musikcube vertical slice

- Implement async track queries, immutable pages, track-list cache windows, and
  metadata invalidation events.
- Implement the C++ consumer wrapper.
- Migrate one representative musikcube search/list view to validate query,
  string, event, ownership, and scrolling behavior.

### Phase 3: application feature coverage

- Add playback, queue, preferences, and indexer APIs.
- Migrate musikcube feature by feature.
- Add advanced audio consumer APIs only where an identified consumer needs
  them.

### Phase 4: decoder SDK pilot

- Implement host and plugin entry tables.
- Implement streams, audio buffers, decoder factories, and decoders.
- Add internal C++ adapters.
- Convert one decoder plugin and validate it on all three platforms.

### Phase 5: remaining provider SDK

- Add encoders and data-stream factories.
- Add audio outputs and real-time contract tests.
- Add DSP, tag, and indexer source providers.
- Convert existing plugins incrementally.

### Phase 6: removal and stabilization

- Remove the legacy C API and C++ plugin ABI after all in-tree consumers have
  migrated.
- Freeze ABI major version 1 only after cross-platform lifecycle, compatibility,
  and performance tests pass.
- Publish installed headers, sample consumers, sample plugins, and API
  documentation with release artifacts.

## Estimated scope

A migration-ready public ABI covering musikcube's primary needs is expected to
take roughly three to five focused developer-weeks. A polished cross-platform
public release, including packaging and compatibility tests, is closer to five
to eight weeks.

Adding the provider ABI foundation and proving it with one decoder is expected
to add one to two weeks. Robust decoder, encoder, data-stream, and output SDK
coverage is likely another three to five weeks. Converting every existing
plugin category may add three to six weeks depending on how long internal
compatibility adapters are retained.

These estimates assume reuse of the existing C++ implementation. The dominant
work is lifecycle, concurrency, cancellation, event ordering, compatibility,
and tests rather than forwarding function count.

## Open questions

- Which non-C++ languages should be used to validate the first release?
- Should multiple contexts be supported initially, or explicitly rejected?
- Should public and plugin ABI versions be independent from the beginning?
- Which event-loop integration is needed beyond blocking/nonblocking event
  retrieval?
- Which metadata fields need typed numeric storage rather than UTF-8 values?
- Is a general track snapshot handle useful in addition to page results?
- Should queue edits and track-list edits share one batch operation format?
- Which advanced audio APIs must be public in ABI major version 1?
- Should raw database access be removed entirely or moved to an unstable
  extension header?
- Which existing plugin categories belong in SDK version 1?
- Should plugin discovery use a sidecar manifest, and if so, which fields are
  authoritative before the library is loaded?
- What compatibility guarantees should apply to the optional C++ wrappers?

## Decision summary

The recommended direction is a stable C ABI with two explicit sides:

- A coarse-grained, async, snapshot-oriented public API for applications.
- A function-table-based provider SDK for plugins.

They share one ABI foundation but remain separate contracts. musikcube consumes
the public side through an idiomatic C++ wrapper. Existing core code and plugins
can be bridged with internal C++ adapters during migration. This design makes
musikcore both embeddable across runtimes and extensible by plugins written
outside C++ without putting C++ ABI details into the public contract.
