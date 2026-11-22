# musikcore C ABI and C SDK design

Status: proposed

The sections below describe the target design, including future surfaces. The
implemented experimental contract and foundation gate are recorded in the
[low-level plan](musikcore-c-abi-lld.md#current-experimental-core-contracts).
The [high-level plan](musikcore-c-abi-hld.md) owns delivery sequencing and links
the architectural decision records in `docs/adr/`. These distinguish supported
operations from proposals. The core foundation gate takes precedence over the
original sequence: finish and stabilize the core application C API before
beginning plugin/provider ABI work. Legacy `mcsdk_*` declarations are already
removed; references to their limitations below describe the migration baseline.

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
  lower 16 bits. Keep the experimental version fixed during development.
  Signatures and layouts can change before the ABI freeze.
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

Document every public struct field where it is declared. Provide an
`MC_*_DEFAULT` initializer for every public size-tagged struct, including
the sizes of any nested size-tagged structs.

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
Error objects are created only on failure and expose a code and UTF-8 message.
Async messages name the submitted action and preserve specific failure details
when available. Clients use the callback, result code, and typed state for logic;
message wording is intended for people rather than parsing. Async callbacks
borrow the error handle for the duration of the callback. Diagnostics may be
absent on allocation failure; the result code remains authoritative. Errors do
not depend on thread-local state.

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
after the terminal callback.

## Public C API

`musikcore.h` includes the application-facing subsystem headers. Each
subsystem header declares its own dependencies. `context.h` covers runtime
lifecycle and request dispatch, and `library.h` covers library queries and
metadata pages. Playback and queue, indexing, preferences, and advanced audio
can get their own headers as those APIs are introduced. The provider SDK has
its own entry point in `plugin.h`.

### Purpose

The public C API allows an application to embed musikcore as a shared library.
It exposes stable domain operations rather than mirroring internal C++ classes.
The initial API should cover the operations required by musikcube and common
embedding scenarios. Advanced facilities can be added in later ABI minor
versions.

### Context and runtime lifecycle

The public entry point is an owned context:

Initialize `mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;`, then override
fields. Null options and omitted sized fields use defaults. A zero requested
ABI version selects the current version; explicit requests must match the
current major and specify a minor no newer than the library's. Plugin directory
storage is copied during creation; discovery fixes that directory for the process.
[`context.h`](../include/musikcore/context.h) defines the complete options layout.

The context owns all engine services by default. Set `options.services` to the
services the host needs; prerequisite expansion is automatic and acyclic:

| Requested service | Automatic prerequisites | What starts |
| --- | --- | --- |
| `MC_SERVICE_NONE` | None | Request processing, cancellation, wakeups, and dispatch only |
| `MC_SERVICE_LIBRARY` | None | Configured local/remote catalogs and their query work |
| `MC_SERVICE_PLUGINS` | None | Provider discovery, preferences, and a provider session |
| `MC_SERVICE_INDEXER` | Library, Plugins | Local scanner and its providers |
| `MC_SERVICE_PLAYBACK` | Library, Plugins | Player and its providers; no scanner |
| `MC_SERVICE_ALL` (default) | All above | Full engine used by musikcube and musikcubed |

Combine bits with `|`. `mc_context_get_services` / C++ `Context::Services()` reports
the expanded immutable configuration, including after shutdown. It is safe to
read from another thread while the context remains alive. It is not a readiness
or running-state query. Services cannot be added or removed during a context's
lifetime; destroy and create a context to choose a different set. Playback and
scan commands operate normally within the selected services.

For example, `options.services = MC_SERVICE_PLAYBACK;` starts playback, catalogs,
and providers, but indexing calls return `MC_ERROR_UNSUPPORTED`. A library-only
context opens the writable configured catalog without initiating provider
discovery or constructing an indexer. Its configured status still reflects saved scan roots.
If an upgrade requires reindexing, the catalog records that requirement durably;
a later context with Indexer enabled resumes the scan automatically. This does
not enable Indexer implicitly in a library-only or playback-only context.
Plugins alone initialize discovery and the generic provider environment (paths,
buffers, codecs, diagnostics). Legacy metadata/indexer bindings require Library,
and playback remotes require Playback. They do not implicitly start
missing host services. Service selection controls context-owned startup; it does
not disable existing process-wide configuration/discovery functions or unload
previously discovered modules. Explicit caller-owned remote sessions remain independent
of the configured catalog service.

`MC_CONTEXT_START_LOCAL` starts with the local catalog without changing the
saved selection or connecting the configured remote. Selecting Remote or
explicitly reconnecting authorizes that connection. Without the flag, normal
configured-remote startup remains unchanged.

Explicit sessions offer `mc_remote_session_set_callback` for current-state
notifications even when another catalog is selected. Registration/removal and
delivery use the context dispatcher thread. The initial snapshot and later
states may coalesce to the latest state. Removing/replacing the observer,
destroying the session, or shutting down the context discards queued delivery;
the session may safely outlive its observing context.

`MC_SERVICE_NONE` allows callers to open an existing read-only `mc_library` or
`mc_remote_session` and attach a library with `mc_context_set_library`. A context
owning Library uses `mc_context_select_library` instead. Without Library, managed
catalog selection and reconnect return `MC_ERROR_UNSUPPORTED`; without Playback,
opening a playback handle does likewise. Logging requires at least one service.
`MC_CONTEXT_START_LOCAL` requires Library after expansion, selects the local
catalog initially, and leaves the saved catalog selection unchanged.

The default macro and a zero-filled full options struct intentionally differ:
the macro selects all services; an explicit zero `services` field selects none.
An older sized prefix that omits `services` selects all, just as null options do.
The experimental `MC_CONTEXT_MANAGED_RUNTIME` flag is removed; rebuild consumers
against these headers. The experimental ABI version remains unchanged.

Creation with services binds the calling thread as dispatcher. With no services,
the first dispatch or shutdown binds the thread. Engine creation is transactional:
a failure publishes no context and rolls back started services. Shutdown stops
producers, settles accepted requests, and invalidates playback handles. Service
selection never authorizes concurrent lifecycle calls: dispatch/shutdown/destruction
still run on the dispatcher thread. Only one context may exist per process; a
second create returns `MC_ERROR_BUSY` until successful destruction releases it.
The ABI-only test library supports `MC_SERVICE_NONE`; engine requests there return
`MC_ERROR_UNSUPPORTED` rather than silently degrading.

Working C and header-only C++ examples:

- [`core-engine.c`](../examples/core-engine.c) and
  [`core-engine.cpp`](../examples/core-engine.cpp): context-owned startup and shutdown.
- [`core-query.c`](../examples/core-query.c) and
  [`core-query.cpp`](../examples/core-query.cpp): no engine, explicitly opened catalog.
- [`core-remote.c`](../examples/core-remote.c): no engine, caller-owned remote session.

### Explicit URI players

`mc_player_create(context, &player)` constructs an owned player with a separate
transport/output. It requires Plugins and never enables Library, Indexer, or the
catalog Playback service. A Plugins-only context therefore supports playback
without creating a database. `mc_player_play_uri` copies a filesystem path or
provider URI and accepts an asynchronous open; sample state or observe changes
through context dispatch. State includes buffering, playing, paused, finished,
and failed. Snapshot position/duration use seconds, and volume is normalized. Seeking
returns BUSY during buffering and resumes paused playback, following the existing
transport semantics.

Local `file://` URIs support an empty or `localhost` authority and percent-encoded
paths. Provider schemes such as HTTP/HTTPS require the corresponding plugin.
Internal credential-bearing `musikcore:` catalog URIs are rejected: catalog
playback retains its origin/session checks. URI players carry no library track
identity, play queue, metadata indexing, or scrobbling behavior.

Unlike releasing a `mc_playback` handle, `mc_player_destroy` stops the owned
resource and waits for its workers. It requires the creating thread outside its
callback; reentrant destruction returns BUSY without consuming the handle.
Destroying one player does not stop or wait on other players. Context shutdown
stops all children before provider teardown, unregisters callbacks, and leaves
retained handles safely invalidated (NOT_FOUND) but still destroyable. Callbacks
are coalesced current-state notifications, dispatched only on the host thread;
callback state must remain alive through removal or shutdown. Preferences and
provider discovery remain process-wide; distinct players do not promise mixing
or isolated hardware clocks.

The current C++ plugin SDK requires `IDataStreamFactory::Create()` and
`IEnvironment::CreateDataStream(uri)` to return owned, unopened streams without
blocking I/O. The caller retains/publishes the resource before calling `Open`,
so shutdown can call `Interrupt` during startup. Stream implementations must
handle interruption before and during `Open`, and release safely without opening.
SDK 23 replaces the old factory `Open` and environment `GetDataStream` contracts;
plugins built against SDK 22 must be rebuilt. There is no optional creation export
or compatibility fallback. The experimental public C ABI version is unchanged.

[`core-player.c`](../examples/core-player.c) and
[`core-player.cpp`](../examples/core-player.cpp) demonstrate an event-driven host.
Build the normal example targets `musikcore_example_player_c` or
`musikcore_example_player_cpp`, then run `<example> <URI> [plugin-directory]`.
The installed-SDK test also compiles and runs both examples after relocation.

### Async requests and dispatch

Accepted operations return a nonzero request ID and deliver one terminal
callback with the supplied `user_data`. Rejected submissions return an error
immediately and have no callback. Inputs are copied or retained before
submission returns. The caller keeps `user_data` valid until the callback
returns.

```c
typedef uint64_t mc_request_id;

MC_API mc_result MC_CALL mc_request_cancel(mc_context *context, mc_request_id request);
MC_API mc_result MC_CALL mc_context_dispatch(mc_context *context);
MC_API mc_result MC_CALL mc_context_shutdown(mc_context *context);
```

`mc_context_dispatch` never waits for work to become ready. The first dispatch
or notification poll establishes the dispatcher thread. Dispatch invokes ready
completion callbacks inline. Worker threads enqueue results.
Callbacks run outside internal locks and may submit or cancel requests.
Nested dispatch returns `MC_ERROR_BUSY`; dispatch from another thread returns
`MC_ERROR_WRONG_THREAD`. Shutdown and
destruction must be called outside a callback, on the dispatcher thread once
one is established.

Cancellation is best effort. If cancellation wins the race to a terminal
outcome, the callback receives `MC_ERROR_CANCELLED`. The worker may still
finish its work after cancellation. If a terminal result was already selected
or the request ID is unknown, cancellation returns `MC_ERROR_NOT_FOUND`.
Admission reserves space for each accepted request's
terminal completion. Queue pressure can reject new submissions before they
become accepted.

A periodic host loop can call dispatch on each tick. A sleeping host loop
can supply a wakeup function in the context options. The hook may run on
a worker thread, outside internal locks. It must signal or post work to the
host loop and return promptly. It must not call the context API. The host
loop then runs dispatch and drains notifications on the dispatcher thread.
Wakeups may be coalesced, but work arriving as the dispatcher drains its
queue must either be observed by that drain or trigger another wakeup.

Shutdown stops admission, cancels or settles accepted work, and delivers all
remaining terminal callbacks on the dispatcher thread before returning. It
also stops wakeups and waits for any in-flight wakeup hook to return. Before
destroying the context or releasing host wakeup state, the host cancels or
drains dispatch tasks it already posted. Destruction of a live context
performs shutdown first. Queued notifications that the host did not consume
are released during destruction.

### Notifications

Caller-owned event sources in `subscriptions.h` and the typed C++
`Subscriptions` helper deliver synchronously on the publisher's thread.
Core services continue to use context dispatch for application notifications.
Disconnect waits for admitted callbacks when called outside delivery.
Disconnect during delivery only closes admission so callbacks on different
threads can disconnect each other without waiting in a cycle.

The context queues unsolicited playback, library, and indexer notifications.
The dispatcher thread polls them separately from per-request completions:

```c
MC_API mc_result MC_CALL mc_context_poll_event(mc_context *context, mc_event **out_event);
MC_API mc_event_type MC_CALL mc_event_get_type(const mc_event *event);
MC_API void MC_CALL mc_event_destroy(mc_event **event);
```

An empty queue returns `MC_OK` with a null event. A delivered event is owned
by the caller. Destroying it releases any payload the caller has not taken.
Ordered playback transitions and terminal indexer outcomes are lossless.
Current-state remote notifications and documented progress or invalidation
notifications may be coalesced. A mutation is
committed before either its completion or its change notification is
published. The two queues have no shared delivery order.

### Library queries and metadata snapshots

Queries return immutable result pages rather than requiring many calls through
live track objects. A query specifies the desired metadata fields and paging
parameters:

A library handle identifies a local database or one remote connection
generation. Hosts configure a remote session with endpoint settings and
receive the connect completion through context dispatch. The session opens
the same library handle used by local callers. Disconnect and reconnect
invalidate the old generation. Requests, results, and track references retain
their origin after the caller releases its library handle. The public C example
`examples/core-remote.c` connects and runs an ordered track search.

The context retains its active catalog. A query can use that selection or an
explicit library handle. Both capture the library identity at submission.
Reading the selection returns an owned handle. Switching or clearing the
selection leaves previously accepted requests and results bound to their
original catalog.

An application context supplies playback on the engine's owner thread.
Playback handles borrow that engine and have independent notification
subscriptions. The app keeps the engine alive until the context is destroyed.

The standalone local reader opens `musik.db` through a read-only SQLite
connection. Its direct query pages contain visible tracks in ascending ID order. Search
matches a literal case-sensitive title substring. By default, request up to 64 field
names in result order. An empty field list requests `title`. Core fields
use their existing metadata names and take precedence over tags of the same
name. Other keys are looked up in the normalized tag tables. Track selection
and tag lookup share a single read transaction. The default page holds up to
100 rows, and callers can request up to 1000. Set
`track_query_default_limit`, `track_query_max_limit`, and
`track_query_max_fields` on the context options to override these limits.
Nonpositive or omitted settings use the built-in values. If the default row
limit exceeds the maximum, the maximum applies.

ABI0 exposes values as UTF-8 text, including numbers. The page exposes field
names and a value count for each row and field. `mc_track_page_get_string`
returns the first value for common single-value reads.
`mc_track_page_get_string_at` reads additional tag values in stored sort order,
with value ID breaking ties. Missing values return a null view. A present
empty string has non-null data and zero length.

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

typedef void (MC_CALL *mc_track_query_completion_fn)(
    mc_result result, mc_track_page *page, const mc_error *error, void *user_data);

MC_API mc_result MC_CALL mc_library_query_tracks_async(
    mc_context *context,
    const mc_library *library,
    const mc_track_query *query,
    mc_track_query_completion_fn completion,
    void *user_data,
    mc_request_id *out_request);
```

On success, the callback takes ownership of the page and destroys it with
`mc_track_page_destroy`. On failure, the page is null and the error, if
present, is borrowed until the callback returns. A page stores all requested strings in page-owned storage:

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
- Receive completion callbacks and invalidation notifications.
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

`mc_indexer_cancel` cooperatively cancels the active local scan and drops scans
queued before the call. It is idempotent when idle and leaves the indexer
available for later scans, including scans scheduled while cancellation drains.
It does not join workers; an active provider receives its existing `Interrupt`
request. An in-progress filesystem or tag-reader operation may finish first.
Processed changes remain committed according to the existing scan transaction
rules; cancellation is not a scan-wide rollback. Remaining cleanup is skipped
so unvisited tracks remain available. An active cancelled scan emits one
`MC_INDEXER_CANCELLED` terminal event instead of `MC_INDEXER_FINISHED`, with the
processed-item count. Queued scans removed before starting emit no events.
State returns to `MC_INDEXER_IDLE` before terminal publication, though a later
scan may already be running when the dispatcher delivers the event. Shutdown
remains a separate operation that stops and joins the indexer.

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

The header-only C++ wrapper lives in `musik::core::api`.

`musikcore.hpp` provides move-only or reference-counted RAII types, translates
error results into an explicit C++ result type or documented exceptions, and
maps request callbacks and notifications to the application's event loop.

The wrapper should provide `std::string`, `std::string_view`, spans, and scoped
ownership without accessing internal musikcore C++ classes. musikcube should
depend on this wrapper rather than call the C functions throughout UI code.

The wrapper is strictly header-only. Its implementations live in public
headers and link against the C ABI. Only the C ABI is promised to be binary
stable.

`musikcore.hpp` includes the C++ subsystem headers. `context.hpp` owns the
context and exposes dispatch, cancellation, and shutdown. `library.hpp`
provides track queries and owns returned pages. The caller keeps captured
callback state valid through shutdown, which drains pending callbacks.

`TrackPage::Track(row)` returns a read-only `TrackView` with named accessors
such as `Title()`, `Artist()`, `Album()`, and `Duration()`. Text accessors
return `std::string_view` valid while the page storage is owned. Copy it
to a `std::string` to keep it independently. Numeric accessors return
`std::optional`, and duration is in `std::chrono::seconds`. Use
`Value(key)` or `Values(key)` for arbitrary requested tags.

C++ range reads and the short `ReadTrack(context, track, completion)`
overload request title, artist, album, and duration by default. Pass a
field list to choose a different projection.

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
MC_PLUGIN_API mc_result MC_CALL mc_plugin_get_api(
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
    uint32_t abi_version;
    mc_string_view id;
    mc_string_view name;
    mc_string_view version;
    mc_string_view author;
    mc_result (MC_CALL *initialize)(
        const mc_host_api *host, void **out_plugin_context, mc_error **out_error);
    void (MC_CALL *shutdown)(void *plugin_context);
} mc_plugin_api;
```

The load sequence is:

1. Discover a candidate shared library.
2. Load it and resolve `mc_plugin_get_api`.
3. Exchange ABI versions and sized function tables.
4. Validate identity, ABI requirements, and required lifecycle callbacks.
5. Call plugin initialization, during which the plugin registers factories
   through the host API.
6. Make registered factories available for instance creation.
7. Create and use provider instances.
8. Stop new instance creation during shutdown.
9. Destroy all provider instances.
10. Call plugin shutdown and unload the library.

The core must not unload a plugin while any of its function tables, string
views, factories, instances, or admitted calls can still be used. Internal
capability leases retain the module across worker calls. Shutdown closes new
acquisition and invocation before waiting for those references and calls to
finish. Lifecycle reentry from a provider call and wrong-thread lifecycle calls
are rejected; the registry is not a real-time dispatch mechanism.

Entry must leave no persistent engine state requiring cleanup if negotiation
fails. Once initialize is entered, shutdown must run exactly once, even after
initialization failure, and clean partially initialized state. Initialize
receives the host table and returns an opaque per-session context, assigning it
before further fallible work. Shutdown receives that context (possibly null).
This allows provider-owned allocation without an entry-time allocation or a
mutable global host pointer. Failed registrations are never published. Shutdown must join provider workers before
returning, because the host context and native image may then be released.
See [ADR-0008](../docs/adr/0008-c-provider-bootstrap-and-unload-lifetime.md).

A sidecar manifest may be added for discovery without executing plugin code.
The entry point remains authoritative for ABI validation.

### Host API

The bootstrap host table in
[`plugin.h`](../include/musikcore/plugin.h) exposes logging, host-owned error
creation/destruction, and capability registration. It contains a byte size,
provider ABI version, and opaque host context passed to every callback.
Providers use these function pointers without linking private core targets or
importing application-facing C functions.

`mc_plugin_capability` carries an identifier, an immutable typed table pointer,
and its byte size. The table begins with its own `uint32_t struct_size`, which
must match the registered byte count. The host copies registration metadata;
the plugin keeps table storage alive until shutdown. A typed capability belongs
to the negotiated provider ABI, without another contract identifier or version.
Only initialization on the owner thread may register capabilities. Publication
happens atomically after initialization succeeds; duplicate IDs are rejected.

Audio-buffer, stream, decoder, and preference services are defined with their
typed provider families. They are not required bootstrap callbacks. Future
host services use sized optional tails/sub-tables so a plugin can check support
before accessing them. The application umbrella header does not include the
provider header.

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

The public API delivers request completions through per-request callbacks on
the host dispatcher. The notification queue carries unsolicited changes.

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
- Implement context creation, shutdown, version negotiation, and dispatch.
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
