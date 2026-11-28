# musikcore C ABI and C SDK design

Status: proposed

The sections below describe the target design, including future surfaces. The
implemented experimental contract and foundation gate are recorded in the
[low-level plan](musikcore-c-abi-lld.md#current-experimental-core-contracts).
The [high-level plan](musikcore-c-abi-hld.md) owns delivery sequencing and links
the architectural decision records in `doc/adr/`. These distinguish supported
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
typedef struct mc_audio_scratch_buffer mc_audio_scratch_buffer;
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
metadata pages. Playback and queue, indexing, settings, and advanced audio
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
storage is copied during creation. The first validated directory selection is
retained across sessions; omitted directories reuse it, and changing it within
the process is rejected. Provider images still unload at session shutdown;
persisted directory selection does not extend their lifetime.
[`context.h`](../../include/musikcore/context.h) defines the complete options layout.

The context owns all engine services by default. Set `options.services` to the
services the host needs; prerequisite expansion is automatic and acyclic:

| Requested service          | Automatic prerequisites | What starts                                                  |
| -------------------------- | ----------------------- | ------------------------------------------------------------ |
| `MC_SERVICE_NONE`          | None                    | Request processing, cancellation, wakeups, and dispatch only |
| `MC_SERVICE_LIBRARY`       | None                    | Configured local/remote catalogs and their query work        |
| `MC_SERVICE_PLUGINS`       | None                    | Provider discovery, settings, and a provider session      |
| `MC_SERVICE_INDEXER`       | Library, Plugins        | Local scanner and its providers                              |
| `MC_SERVICE_PLAYBACK`      | Library, Plugins        | Player and its providers; no scanner                         |
| `MC_SERVICE_ALL` (default) | All above               | Full engine used by musikcube and musikcubed                 |

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
buffers, codecs, diagnostics). Metadata/indexer bindings require Library,
and playback remotes require Playback. They do not implicitly start
missing host services. Service selection controls context-owned startup; it does
not unload individual plugins during an active session. Provider modules retire
only after their session has quiesced. Explicit caller-owned remote sessions
remain independent of the configured catalog service.

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

Startup restores local playback settings and the saved queue before exposing
the context, so a later dispatch cannot overwrite newly submitted commands.
Remote restoration is asynchronous because its transport requires host dispatch.
Provider caches retire after their users stop, including when visualizers acquired
providers without catalog Playback. Concurrent factory use retains a synchronized
cache snapshot. Shutdown drains callbacks and joins workers before provider
cleanup and native unload; a failed provider cleanup cannot be treated as a
recoverable status permitting unload. Recoverable runtime errors may be reported
only when ownership and quiescence remain valid.

Migration-required indexing is catalog state, not a process-local startup flag.
Schema upgrades record the requirement transactionally even without Indexer.
A later indexing session resumes after service bindings and subscriptions are
ready. Completion retires the marker with its scan transaction, so failed writes,
shutdown, or a later process cannot lose required indexing.

Working C and header-only C++ examples:

- [`core-engine.c`](../../examples/core-engine.c) and
  [`core-engine.cpp`](../../examples/core-engine.cpp): context-owned startup and shutdown.
- [`core-query.c`](../../examples/core-query.c) and
  [`core-query.cpp`](../../examples/core-query.cpp): no engine, explicitly opened catalog.
- [`core-remote.c`](../../examples/core-remote.c): no engine, caller-owned remote session.

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
callback state must remain alive through removal or shutdown. Settings and
provider discovery remain process-wide; distinct players do not promise mixing
or isolated hardware clocks.

Stream factories create owned, unopened resources without blocking I/O. The
caller publishes the resource before opening it so interruption can reach startup,
including nested provider streams. Interruption latches before or during open;
cleanup must also support an instance that never opened. These contracts apply
through the C plugin SDK; legacy C++ SDK migration is recorded in the LLD.

[`core-player.c`](../../examples/core-player.c) and
[`core-player.cpp`](../../examples/core-player.cpp) demonstrate an event-driven host.
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

`mc_context_dispatch` never waits for work to become ready. Engine creation binds
the dispatcher thread; with no services, first dispatch or shutdown binds it.
Dispatch invokes ready completion callbacks inline. Worker threads enqueue results.
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

#### Library scope and source-owned playlists

Scope is attached to the library instance; aliases share it and independent
instances do not. Filtered requests copy immutable scope at submission. Effective
changes cancel pending requests immediately, without waiting for a remote reply,
and discard pending transport work so it does not occupy a slot until timeout.
Equivalent source sets ignore ordering and duplicates and publish no change.
Completion admission and replacement are ordered together; an admitted completion
may finish after a change, but user callbacks never run under the admission lock.
Delivered results and their derived reads retain original scope and membership.

A null source filter is unrestricted and includes newly discovered sources.
A present empty filter matches no scope-filtered tracks and lists only global
playlists. The C scope applies every non-null typed filter; absent sized fields
default to null. Unknown larger scope/filter layouts are rejected so unsupported
filters cannot silently disappear. Wire scope objects use null-versus-array
semantics and reject unknown filter keys. Internal typed queries own a shared
read-only scope value, independent of the public header-only C++ wrapper.

Playlist discovery includes all playlists when unrestricted, and global plus
selected-source playlists otherwise. Contents and paging never filter saved
entries: order and duplicates survive selection changes. Creation explicitly
specifies global or single-source ownership. Replacement and append preserve it
and validate an entire batch before writes. Source-owned playlists reject every
track whose source ID differs from the owner. Ownership does not imply a
provider-managed or read-only playlist.

Persist ownership as nullable `playlists.source_id`: NULL is global, zero is
ordinary files. Existing playlists migrate to global. Database constraints guard
entry inserts, updates, and ownership changes; public C and WebSocket operations
report invalid input without partial mutation. Discovery requests follow scope
invalidation; playlist-content operations preserve the saved contents.

Consumers have no raw database or SQL extension, including a separately unstable
one. They use typed local/remote operations as established in
[ADR-0003](../adr/0003-origin-bound-results-and-paged-metadata.md).

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

#### Remote metadata and playback controls

Initial remote playback and next-track preparation fetch metadata asynchronously.
Getters read cached metadata and never wait for remote I/O. A successful play
submission may precede stream startup; observe playback events rather than
assuming audio started on return. Metadata and stream buffering are separate,
so responsiveness does not guarantee gapless progress on a slow server.

The native track list fetches metadata into its existing cache through
`TrackList::GetAsync()`. Each list owns one pending lookup. Replacement,
cancellation, cache clearing, or destruction suppresses late delivery and cache
updates. Playback cancels on Stop, replacement selection, library change,
disconnect, or shutdown. It checks queue revision, track ID, and originating
session before acting. The default `PlaybackTrackQueryTimeoutMs` is 30 seconds;
a delayed owner-queue message retires the continuation without blocking audio.
A late successful preparation can start a successor after a short track ends.

During a crossfade, the selected track determines whether Pause or Resume
succeeded. If its device refuses, the outgoing audio keeps its prior state.
Once it accepts, an outgoing device that cannot follow is stopped. Losing a
short fade tail on a device error is preferable to rolling back a successful
command on the track the listener selected.

Volume changes keep only the latest requested gain. If the device is busy,
the caller returns immediately and a later write applies that gain. This lets
manual controls and fade ticks share one output operation. The crossfader
stops the outgoing track if it cannot confirm silence at the end of a fade.

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

### Settings

Settings expose typed getters and setters for booleans, fixed-width
integers, doubles, and UTF-8 strings. A setting namespace is an owned or
context-borrowed handle with documented lifetime.

Batch updates are preferred so a wrapper can make several changes and save
once. String reads use either caller buffers with required-size semantics or an
immutable snapshot owned by the setting result.

### Advanced audio API

Applications use opaque `mc_media_stream`, `mc_media_decoder`, `mc_media_encoder`,
and `mc_media_pcm` handles. They are context-owned session children with public C
operations and header-only C++ owners; plugin service tables and plugin PCM types
remain separate. They require Plugins, not Library or catalog Playback. See
[ADR-0006](../adr/0006-explicit-uri-player-resources.md) for the ownership rationale.

Stream creation selects a provider without blocking I/O on the context owner.
Opening is a separate worker operation so the caller can publish the handle before
blocking and cancel it from a controller. Decoder/encoder creation consumes an
opened stream only on success; separate codec open permits the same interruption
sequence. Always destroy the codec before its stream. Factories do not probe by
performing I/O, and a claiming provider's failure is returned rather than trying
another decoder on potentially consumed input.

Serialize ordinary operations on each resource; concurrent attempts return BUSY.
Interruption can race an operation and latches cancellation. Decoders interrupt
both codec and input. Encoders interrupt output I/O; CPU work must finish before
cleanup because the encoder SDK has no interrupt callback. Destroy and ownership
transfer require synchronization with callers using the same handle. Blocking
operations belong off event and real-time audio threads.

A context media session retains ownership-chain roots and admits calls. Shutdown
closes admission, interrupts roots, waits for admitted calls, and destroys codecs
before streams and provider unload. Replacing a stream root with a codec root is
atomic with shutdown. Retained handles own stopped state and return NOT_FOUND
without touching provider tables. Each decoded PCM block owns copied interleaved
float samples; its views remain valid until block destruction, including across
later reads or context shutdown. Stream metadata strings similarly live with the
application handle. Playback and plugin scratch retain their separate borrowed
buffer contracts; application PCM does not promise zero-copy transfer.

Streaming encoders write returned blocks to their owned output and account for
partial writes; file encoders receive that output directly. Explicit finish flushes,
finalizes, destroys the codec, and closes output, reporting the first failure.
Finish caches its result and never implicitly retries a failed write. Destruction
without finish aborts without flushing/finalization. Expected I/O failures are
ordinary results; provider exceptions crossing C or escaped native cleanup violate
the ownership contract and terminate the process.

Raw database connections and arbitrary SQL are excluded, including a separately
unstable advanced module. New storage needs require typed library operations.

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

Plan the C shapes for visualizers, playback remotes, analyzers, and
configuration alongside the foundational media contracts.
Implement one representative per family before backfilling remaining plugins;
the engine replacement order is tracked in the LLD.

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
`SetSettings`, `SetDebug`, and similar exported symbols.

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

Plugin entry implementations call `mc_plugin_validate_host_api` before using
the host table. This header-only C helper centralizes ABI, mandatory callback,
and struct-capacity checks without allocating, invoking callbacks, or mutating
the plugin table. After success, assign `MC_PLUGIN_API_DEFAULT` and fill the
plugin's metadata and hooks.

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

The registry uniquely owns loaded plugins for the engine session. Typed factory
views are non-owning table views, not leases; dropping or retaining them does
not affect DLL lifetime. They may be called only during the live session.
Before registry shutdown, the engine stops new work, interrupts/drains instances,
finishes callbacks, joins workers, and destroys instances. Registry shutdown
then invokes plugin cleanup and unloads each DLL on its owner thread without
waiting for handles or calls. Individual plugin unloading during a session is
not supported. Lifecycle reentry into the same active session is fatal before
mutation or unload; wrong-thread lifecycle calls return an error without
changing state. Destruction cannot return that status: destroying a registry or
plugin on the wrong thread or during an active provider call is a fatal ownership
violation before cleanup or unload. Typed factory invocation detects owner-thread reentry and
propagates host callback exceptions; it does not schedule, serialize, count
workers, or extend calls beyond the engine session. Actual plugin entry/initialization
exceptions become contextual plugin errors. Throwing shutdown is fatal before
unload because worker quiescence can no longer be established.

`PluginCallState` belongs to one owner, rather than the whole thread. Nested
scopes save and restore an owner-only flag; workers never access that flag.
Independent registries can initialize and shut down inside another session's
call. Empty/completed cleanup is harmless, but reentry that invalidates a live
transaction or unloads executing code terminates with a diagnostic in Debug
and Release. This is neither a lock nor worker accounting; the engine must
still quiesce users before shutdown. Completion is published only after actual
cleanup. Catch policies stay separate: host factory callbacks
propagate exceptions, selection failures are host errors, and logging failures
are contained without recursively reporting through the same failing sink.

Registry callers use `Initialize(directory, selection)`, `ListPlugins()`,
`Find<T>(pluginId)`, and `Shutdown()`. Initialization scans once; a new directory
or retry after a failed scan requires a new registry. Invalid arguments and
wrong-thread calls return errors before changing session state. Fatal reentry
terminates before that state changes.
Missing directories create empty sessions. Candidate errors are logged and
skipped, while directory errors fail initialization and close the session.
Scanning and single-file loading are private, with no public intermediate
result type. Factory lookup accepts only supported C table types (currently
`mc_output_factory`), maps each to its registered ID internally, and returns a
typed non-owning view. Arbitrary table types cannot be requested or supplied
as callback parameters. Adding a family requires its table validator and type
mapping together; C registration remains string-based for extensibility.

Entry must leave no persistent engine state requiring cleanup if negotiation
fails. Once initialize is entered, shutdown must run exactly once, even after
initialization failure, and clean partially initialized state. Initialize
receives the host table and returns an opaque per-session context, assigning it
before further fallible work. Shutdown receives that context (possibly null).
This allows provider-owned allocation without an entry-time allocation or a
mutable global host pointer. Failed registrations are never published. Shutdown must join provider workers before
returning, because the host context and native image may then be released.
See [ADR-0007](../adr/0007-c-provider-bootstrap-and-unload-lifetime.md).

A sidecar manifest may be added for discovery without executing plugin code.
The entry point remains authoritative for ABI validation.

### Host API

The bootstrap host table in
[`plugin.h`](../../include/musikcore/plugin.h) exposes logging, host-owned error
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

Audio-buffer, stream, decoder, and setting services are defined with their
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

### Planned family contracts and C++ SDK mapping

This section is the provisional shape plan for PLG-CONTRACT-001/002/003. The
existing C++ SDK is the behavioral inventory, not a requirement to preserve
inheritance, allocators, ambiguous return values, or compiler-specific identity.
Private engine types need not mirror the old SDK hierarchy either. Remove
unnecessary wrappers, virtual interfaces, or provider layers when concrete
callers become simpler. For example, an opaque audio-buffer handle can refer
directly to core-owned storage without recreating `IBuffer`/`IBufferProvider`
internally. Keep indirection where it serves ownership, lifetime, or actual
implementation variability; do not refactor unrelated types just for symmetry.
The bootstrap in `plugin.h` and the audio/output headers under
`include/musikcore/plugin/` are installed declarations. Other declarations below
remain design sketches. The buffer host and loadable `nullout` C plugin are
implemented and boundary-tested; the engine still uses the former SDK pending
cutover. Family integration and acceptance tasks remain open in the LLD.
Representative implementations may refine these shapes before the experimental
ABI is frozen.

Plan family headers under `include/musikcore/plugin/`, included as needed by
plugins. Keep the application umbrella separate. Reuse `abi.h` primitives and
existing public library/playback/setting value types instead of defining a
second model of a track, playlist, or playback state.

#### Existing interfaces and their disposition

| Existing C++ interface/helper                                                                                  | Planned C representation                                                                  | Behavioral mapping / deliberate change                                                                                                                  |
| -------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `IPlugin`, exported `GetPlugin`, SDK version                                                                   | Existing `mc_plugin_api` entry/lifecycle table                                            | Identity and per-plugin initialization/state; one C entry point.                                                                                        |
| `IResource::Release`, `Retain`, `IAllocator`                                                                   | Typed object destruction; explicit clones only for shared core handles                    | Allocation is freed by its owner. No generic resource hierarchy or caller-selected allocator crosses the boundary.                                      |
| `IBuffer`, `IBufferProvider`                                                                                   | `mc_audio_scratch_buffer`, private fixed playback pool, PCM views, output completion sink | Replace interleaved sample counts with frames; output returns accepted buffers exactly once.                                                            |
| `IDataStream`, `IDataStreamFactory`                                                                            | Stream object/vtable and stream factory                                                   | Keep create/open/read/write/seek/info/interrupt/close; explicit byte counts and EOF.                                                                    |
| `IDecoder`, `IDecoderFactory`                                                                                  | Decoder object/vtable and decoder factory                                                 | Keep open/decode/seek/duration/preferred rate; return exhaustion with the decoded block.                                                                |
| `IEncoder`, `IBlockingEncoder`, `IStreamingEncoder`, `IEncoderFactory`                                         | Separate blocking and streaming encoder tables in one factory                             | Replace `AsBlocking`/`AsStreaming` with advertised modes; streaming bytes use a synchronous sink rather than an ownership-ambiguous `char**`.           |
| `IOutput`, `IDevice`, `IDeviceList`                                                                            | Output object/vtable, output factory, device callbacks                                    | Keep pause/resume/volume/stop/drain/latency/devices; make accepted/full/error outcomes explicit.                                                        |
| `IDSP`                                                                                                         | DSP factory and per-stream processor                                                      | Keep in-place processing; prepare/reset/configure outside real-time processing.                                                                         |
| `ITagReader`, `ITagStore`, `ReplayGain`                                                                        | Tag reader, host metadata writer, replay-gain values with presence flags                  | Keep text/artwork/gain updates and contains/clear; preserve missing versus zero/empty.                                                                  |
| `IIndexerSource`, `IIndexerWriter`, `IIndexerNotifier`                                                         | Source factory/scan instance, source-scoped writer, rescan host operation                 | Keep scan hooks, validation, item writes/deletes/progress, interruption. Stable text source keys replace C++ hashes; checkpoint semantics are explicit. |
| `IAnalyzer` (`GetAudioAnalyzer`)                                                                               | Analyzer factory and begin/process/finish object                                          | Preserve the core's analysis hook even though a production implementation may be absent; writes use the metadata writer.                                |
| `IVisualizer`, `IPcmVisualizer`, `ISpectrumVisualizer`                                                         | Visualizer factory/object and bounded PCM/spectrum views                                  | Keep show/hide/visible/name plus frame delivery; no transient audio pointer retained.                                                                   |
| `IPlaybackRemote`                                                                                              | Playback remote factory and instance                                                      | Keep track/state/time/volume/mode/queue notifications and typed host commands; stop and join workers owned by the remote.                               |
| `ISettings`, `ISchema`, `Configure`/`Reload`                                                                | Plugin-scoped setting table and data-driven configuration capability                   | Keep typed values/defaults/ranges/choices/persistence; activation/restart effects are explicit.                                                         |
| `IMetadataProxy`, `ITrack`, `ITrackList`, editor/map/value interfaces                                          | Existing C identities/results/pages and typed catalog/playback host calls                 | Preserve useful operations without recreating the C++ object graph. Identify missing typed operations before server conversion.                         |
| `IEnvironment`, `IDebug`, `SetEnvironment`/`SetDebug`/`SetSettings`/`SetMetadataProxy`/`SetIndexerNotifier` | Bootstrap diagnostics and scoped, typed instance services                                 | No global setter protocol. Factories register first; runtime services are supplied when an instance is created.                                         |
| `String`, `DataBuffer`, `Filesystem`, `HttpClient`, constants/version helpers                                  | Implementation helpers or shared C primitives, according to actual callers                | Header-only C++ helpers are not binary interfaces to transliterate. Audit their engine dependencies before removing the old SDK directory.              |

`SendRawQuery` needs special care in this inventory: despite its name, the
current method parses JSON and selects a named query through `QueryRegistry`.
The authenticated WebSocket protocol does not pass arbitrary SQL to SQLite.
Preserve that typed-query transport where needed. No new provider/remote API may
execute caller-supplied SQL or expose a SQLite connection. This is not a claim
that every existing query implementation has been security-audited here.

Existing indexer sources derived numeric IDs from a standard-library hash and
stored modification times as milliseconds in the platform's `file_clock` epoch.
A schema migration rewrites the historical built-in source IDs using their
virtual URI prefixes. The new source key and plugin ID derive a stable numeric ID. Modification times
remain in each source's existing units and epoch; they are not Unix timestamps.
The lookup reports absence separately, preserving valid negative values.

#### Shared object, factory, and call rules

An owned instance has a typed table and opaque state. The caller does not cast
or free the state. Copying this C struct copies bits, not ownership: move/reset
it explicitly, or use a language wrapper. Destroy it once, after concurrent
calls have finished. A default object has its size set and both pointers null.
Each real header must define its own `MC_*_DEFAULT` initializer and document
every field, including units and ownership. Zero/unset values are the default
unless a family explicitly defines another default, such as initial volume 1.

Each object has an explicit C declaration:

```c
typedef struct mc_stream_vtable mc_stream_vtable;
typedef struct mc_stream {
    uint32_t struct_size;
    void *instance;
    const mc_stream_vtable *api;
} mc_stream;

typedef struct mc_decoder_vtable mc_decoder_vtable;
typedef struct mc_decoder {
    uint32_t struct_size;
    void *instance;
    const mc_decoder_vtable *api;
} mc_decoder;

typedef struct mc_blocking_encoder_vtable mc_blocking_encoder_vtable;
typedef struct mc_blocking_encoder {
    uint32_t struct_size;
    void *instance;
    const mc_blocking_encoder_vtable *api;
} mc_blocking_encoder;

typedef struct mc_streaming_encoder_vtable mc_streaming_encoder_vtable;
typedef struct mc_streaming_encoder {
    uint32_t struct_size;
    void *instance;
    const mc_streaming_encoder_vtable *api;
} mc_streaming_encoder;

typedef struct mc_dsp_vtable mc_dsp_vtable;
typedef struct mc_dsp {
    uint32_t struct_size;
    void *instance;
    const mc_dsp_vtable *api;
} mc_dsp;

/* tag reader and indexer source instances use their factory's vtable directly;
see plugin/tag.h and plugin/indexer.h for their current declarations. */

typedef struct mc_analyzer_vtable mc_analyzer_vtable;
typedef struct mc_analyzer {
    uint32_t struct_size;
    void *instance;
    const mc_analyzer_vtable *api;
} mc_analyzer;

typedef struct mc_visualizer_vtable mc_visualizer_vtable;
typedef struct mc_visualizer {
    uint32_t struct_size;
    void *instance;
    const mc_visualizer_vtable *api;
} mc_visualizer;

typedef struct mc_playback_remote_vtable mc_playback_remote_vtable;
typedef struct mc_playback_remote {
    uint32_t struct_size;
    void *instance;
    const mc_playback_remote_vtable *api;
} mc_playback_remote;

typedef struct mc_audio_scratch_buffer mc_audio_scratch_buffer;
typedef struct mc_metadata_record mc_metadata_record;
typedef struct mc_plugin_services mc_plugin_services;
```

A factory advertises the instance vtable before creation. The core validates
its required prefix and destruction function before allocating any instance.
`create` receives a null output slot and publishes opaque state on success.
A stream or decoder factory owns cleanup if creation fails and must leave the
slot null. A nonnull slot on failure is a fatal ownership violation because
the host cannot establish whether it owns the reported instance. An output
factory can return partial state on failure. Its stop and destroy callbacks
must accept that state so the host can stop workers before releasing it.
Destruction must never throw. The core keeps successfully created objects
within the engine session.

Factory tables are immutable after registration and valid through plugin
shutdown. Their `context` is plugin-owned state. Register one factory per role
per plugin; a factory may support multiple types/devices. Role identifiers are
the family headers' `MC_PLUGIN_CAPABILITY_*` constants, scoped by plugin ID.
For example, output uses `MC_PLUGIN_CAPABILITY_OUTPUT` (`org.musikcube.output`).
Instance creation may allocate but must return promptly without opening files/devices or starting network work. Publish
the instance to its owner before invoking blocking open/prepare operations.

These are direct function calls; the SDK does not put every call into a queue.
The engine may call a shared factory from different workers at the same time,
so factory callbacks must be thread-safe. Objects created by that factory have
separate state: two decoders can run in parallel on two playback workers.

For one instance, the caller sequences ordinary operations. For example, its
worker finishes `open` before `decode`, and finishes `decode` before `seek` or
another `decode`. The provider does not need to create a call queue merely to
satisfy that rule. A synchronous call may block its worker where the family
contract permits blocking; it does not thereby become an asynchronous API.

Only explicitly identified operations may overlap: a controller can call
`interrupt` while that instance is blocked in `open`/`read`/`decode`; output
completions may arrive from a backend thread; timing reads may overlap rendering.
Those implementations must synchronize their own shared state. The engine
interrupts/stops work, waits for calls and callbacks to finish, then destroys
the instance. A real-time callback never waits for this cleanup.

An interrupted stream/decoder is terminal for that operation; initial
implementations may create a fresh instance rather than implicitly resetting
cancellation during `open`. Document reset/reuse behavior before optimizing it.
Async host requests below are a different contract: submission queues work for
the existing context dispatcher and completion arrives later on its owner thread.

`mc_result` describes success/failure; output parameters describe data and
normal EOF/queue state. Initialize outputs before fallible work. Failures may
return host-created `mc_error` details on non-real-time calls. Real-time calls
return status only and may not allocate an error, log, block, or invoke arbitrary
application callbacks. UTF-8 views expire at return unless a longer lifetime is
explicitly stated. Counts use bytes or frames as named; seconds are finite
`double`, rates are Hz, and bitrates are bits/second. Source modification times
are signed values in the source's own units and epoch, with an explicit presence
flag; negative values are distinct from an absent timestamp.

All PCM crossing these audio interfaces is interleaved native-endian float32.
There is no sample-encoding selector or per-block encoding check. Writable
views use `float*`; outputs convert to a device format internally if needed.
The negotiated SDK contract requires host-provided call structs (including
nested views/formats) to contain the known prefix. Plugins may trust those
sizes; the host must not pass truncated storage. Dynamic values such as rates,
channels, frame counts, and capacity still need validation. Public application
C APIs retain their normal size/argument validation responsibilities.

#### Buffers, hints, and interruption

`mc_audio_scratch_buffer` is growable decoder scratch storage, accessed through
`mc_plugin_audio_scratch_service`. `reserve` ensures capacity in frames; it may allocate on
a producer worker but never waits for output space. Requests within capacity
are no-ops; failed growth preserves existing storage and populated frames.
The decoder ends each write before returning to core.

Playback storage is separate and private to core: allocate a contiguous pool
when preparing the pipeline, divide it into fixed-capacity slices, and recycle
slices only after output returns their completion tokens. Outputs receive
`mc_pcm_view` values through `queue_audio_buffer`, not scratch-buffer handles.
There is no public pool allocation/reserve API. A decoded block larger than a
slice is processed across slices; pool exhaustion applies backpressure rather
than allocating. Rebuilding a pool requires quiescing output and reclaiming all
outstanding slices first. Decoder scratch reuse is measured by PERF-002.

The canonical cancellation token, read-only PCM view, media hint, and buffer
host declarations are in [plugin/audio.h](../../include/musikcore/plugin/audio.h),
including field documentation and default initializers. `mc_audio_scratch_buffer` stays
opaque; only the core stores its format, capacity, and populated frame count.
Changing a returned view cannot change that bookkeeping.

`begin_write` returns a writable view without copying audio. Until `end_write`,
the buffer cannot be resized, reformatted, recycled, destroyed, or accessed by
another operation. This is an access-lifetime contract, not a requirement to
hold a mutex. Nested writes are rejected. A failed `begin_write` grants no access.

Every successful `begin_write` must be paired with `end_write`, including error
paths. `end_write` publishes the supplied frame count and ends access; the view's
pointer must no longer be used. Pass zero frames when abandoning a write so
partially written audio is not exposed. The count must not exceed the returned
capacity; an invalid count ends access, publishes zero frames, and returns an
error. Validation and state tracking cannot protect against a plugin writing
beyond the returned capacity.

The consumer calls `read_block` once per decode. It clears the published frame
count and leaves the returned samples in place until the next mutation. If a
provider returns success without publishing its next block, the previous PCM
cannot appear a second time.

Buffer operations never transfer ownership of a buffer passed into `decode`.
A plugin releases only buffers it explicitly creates/owns. Multiplications of
frames, channels, and element size are checked before accessing storage.
Interruption is latched, safe before/during open, and wakes blocked I/O. Polling
the cancellation token is bounded and allocation-free; it does not replace an
interrupt operation that actually wakes a blocked read.

#### Streams and decoders

The SDK defines the byte-stream operations and factory in
[plugin/stream.h](../../include/musikcore/plugin/stream.h) and the decoder operations
and factory in [plugin/decoder.h](../../include/musikcore/plugin/decoder.h).
An opened stream supplies the MIME type or normalized filename extension when known.
A decoder receives the host's scratch operations through
[plugin/services.h](../../include/musikcore/plugin/services.h).
The host keeps the stream alive until the decoder is destroyed.

The open flags distinguish read, write, create, truncate, and append. No seek is attempted when unsupported. Reads may
be short; nonzero blocking reads return data, EOF, cancellation, or failure,
not an indefinite sequence of zero-byte successes. EOF may accompany a final
nonempty read. Writes report actual transferred bytes even on failure, so callers
do not assume rollback or duplicate partially written data.

The host can suggest an output rate before the first decode. This lets playback
use the preferred device rate after opening the input. Unknown duration reports unavailable, not a fabricated
zero. `decode` may return final frames with `MC_DECODE_END`; consume those frames
before stopping. Empty `MORE` successes are invalid for the initial blocking
contract. Seek reports the decoder's logical playback position. A demuxer that only confirms
acceptance can report the requested time; its first decoded frame can fall on
a different packet boundary. Factory probes perform no I/O;
selection/fallback must account for nonseekable inputs rather than silently
consuming their initial bytes during trial opens.

#### Encoders

```c
typedef struct mc_encoder_options {
    uint32_t struct_size;
    mc_audio_format input_format;
    uint32_t bitrate_bps;          /* zero selects codec default. */
    mc_string_view format;         /* retain established names/aliases. */
} mc_encoder_options;

typedef struct mc_encoded_sink {
    uint32_t struct_size;
    void *context;
    mc_result (MC_CALL *write)(void *, mc_byte_view);
} mc_encoded_sink;

struct mc_blocking_encoder_vtable {
    uint32_t struct_size;
    void (MC_CALL *destroy)(void *);
    mc_result (MC_CALL *initialize)(void *, const mc_encoder_options *, const mc_stream *, mc_error **);
    mc_result (MC_CALL *encode)(void *, const mc_pcm_view *, mc_error **);
    mc_result (MC_CALL *finalize)(void *, mc_error **);
    void (MC_CALL *interrupt)(void *);
};

struct mc_streaming_encoder_vtable {
    uint32_t struct_size;
    void (MC_CALL *destroy)(void *);
    mc_result (MC_CALL *initialize)(void *, const mc_encoder_options *, mc_error **);
    mc_result (MC_CALL *encode)(void *, const mc_pcm_view *, const mc_encoded_sink *, mc_error **);
    mc_result (MC_CALL *flush)(void *, const mc_encoded_sink *, mc_error **);
    mc_result (MC_CALL *finalize)(void *, mc_string_view destination_uri, mc_error **);
    void (MC_CALL *interrupt)(void *);
};

typedef struct mc_encoder_factory {
    uint32_t struct_size;
    void *context;
    const mc_blocking_encoder_vtable *blocking_api;    /* null if unsupported. */
    const mc_streaming_encoder_vtable *streaming_api;  /* null if unsupported. */
    mc_result (MC_CALL *can_handle)(void *, mc_string_view format, uint32_t *mode_flags);
    mc_result (MC_CALL *create)(void *, const mc_plugin_services *, mc_string_view format,
        uint32_t mode, void **, mc_error **);
} mc_encoder_factory;
```

The blocking encoder uses the supplied stream until finalization/destruction;
its owner closes/destroys the stream afterward. Streaming sink calls are
synchronous on an encoder worker: the sink consumes/copies all bytes before
returning success. It may perform bounded cancellable worker I/O. Sink failure
is terminal for that encoding attempt; the caller must not re-encode the same
PCM assuming no bytes were consumed. Flush emits final codec bytes before
finalize writes destination-dependent metadata. A failed/cancelled encoder is
destroyable without pretending finalization succeeded. Destruction releases
resources, not successful-file metadata. Convert existing bitrate settings at
the protocol/configuration edge rather than changing their wire units silently.

#### Outputs and devices

The canonical output instance, callbacks, options, audio buffer, timing, device,
and factory tables are in
[plugin/output.h](../../include/musikcore/plugin/output.h). They include explicit
`MC_OUTPUT_TIMING_*` validity bits and default initializers.

Audio-buffer acceptance is all-or-nothing. `MC_OK` transfers use of the PCM until exactly one
completion; `MC_ERROR_QUEUE_FULL` retains it with the caller and generates no
completion. No positive integer secretly means a retry delay.
`MC_ERROR_BUFFER_TOO_SMALL` identifies a audio buffer that cannot fit the prepared
frame capacity even when empty; callers must split it or reprepare rather than
wait for a capacity notification. The core publishes
the token before calling queue_audio_buffer: completion may occur inline or concurrently.
After reporting completion the provider never touches that PCM again. A backend
failure after acceptance is reported through completion, not as an ambiguous
failed queue_audio_buffer. `ready` is a coalesced producer wakeup, not an application callback.
Both callbacks must use preallocated nonblocking host wiring and be real-time
safe; their storage remains alive through stop/destruction.

Prepare/device enumeration/control and audio-buffer delivery run on workers,
away from hardware callbacks. `queue_audio_buffer` may pace or wait on the producer
worker; it is not a real-time callback interface. Stop may overlap a blocked
queue call or drain, must interrupt it, and returns accepted tokens before
returning. MC_OK still means acceptance: cancellation after acceptance is a
completion outcome, not a failed queue call. The host joins producer calls before
destroying instances. Normal prepare/control calls remain serialized; pause may
take effect between producer blocks. Concurrent timing snapshots remain bounded
and do not take the control mutex.

The host stops the old instance before reprepare and cleans partial state after
failed prepare. It never submits audio before prepare succeeds. Drain waits for
accepted audio to finish and is cancellable. Paused asynchronous output with
pending PCM reports in-flight rather than waiting for a serialized resume call.
Completion/ready callbacks cannot throw, block, or call stop/destroy. Device views
are callback-scoped snapshots copied by the core. Timing flags distinguish
unknown/estimated latency from device measurements. Visualization follows the
output timing snapshot independently of buffer storage completion.

The bundled `nullout` shared library paces and completes audio synchronously on
the calling producer thread. It owns no worker, pending queue, retained token,
or playback-progress state. One mutex/condition variable and an active-call flag
let Stop interrupt the wait and finish its callback before returning. Pause
rejects delivery without retaining PCM; Resume signals a retry. Timing is
unknown. There is no separate built-in null backend. Missing outputs leave
startup/query/idle usable while playback reports unavailable.

C++ output authors may include
[plugin/output.hpp](../../include/musikcore/plugin/output.hpp) and call
`musik::core::plugin::RegisterOutput<T>(host)` during initialization. This optional
header-only adapter generates the immutable C factory/vtable, construction,
destruction, pointer conversion, and exception mapping.
It depends only on public C headers and the standard library, with no base class,
compiled glue, private headers, or core imports. The engine owns input validation
and lifecycle sequencing. The adapter trusts documented host preconditions and
does not add value checks, implicit Stop calls, or prepare rollback. Its first
form supports stateless factories with static device methods; instances may receive host-owned services
through their constructor. Stateful factory helpers can follow a concrete caller.

Implementations use references for prepare/PCM, a plain double volume getter,
and mc_result operations. Stop is noexcept/idempotent and must interrupt queue/
drain and finish callbacks. QueueAudioBuffer must be noexcept to prevent an
exception becoming a failed return after PCM ownership was accepted. The owning
engine must establish successful Stop and callback/producer quiescence before
destruction or native unload; cleanup failure cannot be ignored. Other implementation exceptions become MC_ERROR_INTERNAL with optional error outputs
left null. Optional const/noexcept GetTiming reports unknown when absent.
Only mc_plugin_get_api is exported; the ABI remains C regardless of implementation
language. Add other family helpers as their C contracts and implementations exist.

Output-buffer completion permits storage reuse; it is not an audible device
position. `AudiblePlaybackPosition` owns accepted/completed frame accounting and
normalizes the backend's `mc_output_timing` to one optional audible frame
cursor. It applies validity flags, device latency, clock generations and
queue/completion fallback once. A busy snapshot stays unavailable instead of
falling back to an early storage completion. An unexpected native counter reset
uses approximate fallback until reprepare; unrelated frame generations are never
compared directly.

Each writer owns track position data tying its first accepted output frame to media
seconds. Displayed position and mix points use the same audible cursor through
that mapping, clipped to the track's accepted range; software queue latency is
not subtracted from a completed buffer's starting media position. Mix points
compare unrounded seconds, while displayed position retains whole-second
rounding. Without a measured native clock, progress remains approximate.

`VisualizationAudioQueue` owns copied PCM independently of decoder storage.
Output passes accepted PCM to `Enqueue()` unconditionally; the queue checks for
a visible visualizer or application spectrum-meter consumer before copying or
starting its worker. It receives only the normalized optional frame cursor,
not raw plugin timing fields. A worker selects the latest audible window of at
most 512 frames every 10 ms. Windows skipped between readings are discarded
rather than replayed. Visualization aims for perceptual alignment with music,
not sample-accurate presentation: backend timing may be estimated, and brief
transients can be missed when scheduling falls behind. Capture allocation or
worker-start failure skips visualization and emits at most one diagnostic per
reset; already accepted audio remains successful.

The queue retains at most 1,048,576 float samples (4 MiB). Excess backlog drops
oldest visual frames, and a single block exceeding that budget is skipped.
Pause holds queued visualization; seek, stop, and format/device replacement
clear it before frame numbering changes. Reset waits for at most the current
plugin delivery; each delivery is followed by a timed wait that releases the
queue mutex. The worker sleeps while empty or paused, otherwise samples
progress every 10 ms because outputs do not signal individual audible sample
boundaries. Output destruction joins the worker before plugin services are
released. FFT computation and plugin delivery run there, never on an output's
real-time callback.

CoreAudio estimates the audible cursor from scheduled AudioQueue buffer times;
PortAudio uses callback DAC timestamps. Their private `OutputTimingEstimator`
converts scheduled PCM deadlines to `mc_output_timing`, marks estimates, and
includes hardware delay in the deadline rather than adding it again. It retains
already-audible progress through gaps so old PCM cannot become pending again.
When device lead grows while a tail is still in flight, the estimate may hold
that tail conservatively; it does not retain a history of all scheduled spans.
Startup uses queued-duration estimates until the native timeline is available.
Existing backends with native played-frame counters need not use the estimator.
Targets without lock-free 64-bit atomics leave these optional estimates unknown
rather than introduce a hidden callback lock. sndio retains cumulative counts
through draining pause/resume and drain; flush/reprepare start fresh generations.

#### DSP, metadata writers, tag readers, and analyzers

The implemented DSP and analyzer contracts are in `plugin/dsp.h` and
`plugin/analyzer.h`. DSP instances process one stream on its worker and
receive settings reload notifications. Analyzers receive call-scoped PCM and
metadata writers on the indexer worker. The current metadata store uses 1.0
as the unset replay-gain sentinel and does not preserve empty text values.

Analyzer metadata is saved for a track only when every participating analyzer
succeeds. Failure or cancellation discards all analyzer changes for that track;
other tracks continue unless indexing was cancelled. If an enabled analyzer
cannot be created, skip analysis for the scan and report the failure. A factory
reporting success without an instance violates its contract. Cancellation waits
for the current analyzer call to return and for an already-started track save;
it prevents later analyzer saves. All-or-nothing
results avoid isolating or selectively rolling back analyzers that share one
mutable track, at the cost of recomputing successful analysis after another fails.

If in-place DSP processing fails, discard that buffer and stop playback with the
plugin error. Audio already accepted follows the normal playback failure path.
Partially modified samples cannot be played safely; maintaining backup buffers
solely to recover from plugin failure would add copies to every successful call.

Tag readers still need a C contract and can reuse the metadata writer as their
real requirements are established.

#### Indexer sources and source-scoped writes

The source, writer, metadata-record, and rescan contracts are declared in
[`indexer.h`](../../include/musikcore/plugin/indexer.h). A source instance remains
alive for the indexer session and resets its cancellation state before each scan.
Paths and writers are valid only for their calls; writes are serialized on the
scan worker. Writer operations cannot address another source or arbitrary SQL.
Owned metadata records are released before the scan scope ends. The core derives a stable numeric ID from the plugin ID and source key. A
catalog migration updates historical built-in source IDs and playlist references
using their virtual URI prefixes.

Begin/end bracket the whole operation, including validation of existing tracks.
End runs after begin, including on cancellation or failure. Cancellation and
errors roll back the current uncommitted batch. A checkpoint commits a batch;
earlier checkpoints are not undone by a later rollback. Modification-time
lookups distinguish an absent record from a stored negative timestamp. Track
validation receives the record's stored modification time in the source's
units, avoiding a separate lookup for each track. This changes the experimental
indexer callback signature, so existing indexer plugins must rebuild. A
source-scoped host operation requests a later rescan without exposing a C++
object across the boundary.

Source discovery combines loaded provider descriptions and catalog references
without a sources table. Loaded sources appear even when empty. Referenced IDs
remain selectable when their provider is absent, with `Source N` fallback labels;
unreferenced absent sources and historical display names are not persisted.
Provider display names are nonempty UTF-8 copied by the host before publication
and independent of stable source keys or decoder metadata. Labels are presentation
data, not architectural identifiers; the current built-in file label is
"Music files". Descriptions remain immutable library data and do not borrow the
indexer during shutdown.

Built-in GME/OpenMPT identity migration uses virtual URI prefixes to rewrite track
and playlist IDs. Unknown legacy IDs have no universal migration guarantee.
Modification times keep the source's units and epoch; a missing value is distinct
from a negative timestamp. A source's scoped rescan callback cannot schedule other
sources. Stop its worker and destroy its instance before provider unload.

Checkpointing ends the transaction and releases the write lock; the next mutation
begins a fresh batch. Built-in sources checkpoint between completed files using
an elapsed interval or record bound. One slow file can exceed the interval.
Validation releases each reader before provider filesystem checks, passes stored
modification time to avoid another lookup, and checkpoints writes between entries.
Catalog connections use private SQLite caches, letting the busy handler wait on
competing writers rather than shared-cache table locks. Do not introduce an
application lock spanning plugin or network reads.

#### Visualizers and playback remotes

```c
typedef struct mc_spectrum_frame {
    uint32_t struct_size;
    const float *levels;
    size_t bin_count;
    uint32_t sample_rate_hz;
    uint32_t fft_size;
    float display_floor_dbfs;
    uint64_t media_epoch;
    uint64_t first_frame;
    uint32_t position_valid;
} mc_spectrum_frame;

struct mc_visualizer_vtable {
    uint32_t struct_size;
    void (MC_CALL *destroy)(void *);
    mc_result (MC_CALL *show)(void *);
    mc_result (MC_CALL *hide)(void *);
    mc_result (MC_CALL *is_visible)(void *, uint32_t *visible);
    mc_result (MC_CALL *write_pcm)(void *, const mc_pcm_view *);
    mc_result (MC_CALL *write_spectrum)(void *, const mc_spectrum_frame *);
};

typedef struct mc_plugin_playback_event {
    uint32_t struct_size;
    mc_playback_event event;
    mc_playback_snapshot playback;
    const mc_track_ref *track;     /* callback-only identity. */
} mc_plugin_playback_event;

struct mc_playback_remote_vtable {
    uint32_t struct_size;
    void (MC_CALL *destroy)(void *);
    void (MC_CALL *changed)(void *, const mc_plugin_playback_event *);
    void (MC_CALL *queue_changed)(void *);
    mc_result (MC_CALL *start)(void *, mc_error **);
    void (MC_CALL *request_stop)(void *);
    mc_result (MC_CALL *join)(void *, mc_error **);
};
```

Visualizer format flags in its factory identify PCM and/or spectrum support;
only those delivery callbacks may be null. Show/hide window controls run on
the runtime owner/control thread; providers marshal to their own platform UI
threads and join them before destruction. `is_visible()` is a bounded,
thread-safe snapshot callable by playback workers concurrently with show/hide
and user-initiated window closes. It must not wait for rendering, dispatch to
the UI thread, or call host playback controls. Frame delivery is off the hardware callback,
bounded, and droppable when busy. Views are read-only and expire at return.
For the initial spectrum scale preserve existing behavior: values are dB above
an -80 dBFS display floor, clamped to [0,80], with N/2 bins omitting Nyquist and
spacing sample_rate/N. The explicit media position is optional and does not
promise presentation alignment until output-clock integration is implemented.

Playback events preserve the old track/state/time/volume/mode/queue meanings,
using the existing C event, snapshot and identity types. Delivery is owner-
thread serialized; use typed host commands for actions. Remotes cache copies
for their worker threads. No `IPlaybackService*` or mutable track editor is stored
across the boundary.

Playback remotes cover the server, media keys, hotkeys, and MPRIS. Their
instances start only after playback and its required services are ready. A
context without playback leaves them inactive; loading a plugin does not expand
context service selection. Workers belong to the remote instance, not plugin
negotiation or initialization.

Shutdown first stops new work, requests stop on all active integrations, and
interrupts pending media I/O. Cancel/drain plugin-scoped host requests and owner-
thread callbacks while state/code still live; do not block the owner on a worker
that is waiting for its next dispatch. Join workers and destroy instances,
including peer-provider objects they own, before final registry teardown
and plugin shutdown/unload. That ordering avoids cross-plugin reference
cycles and owner-thread callback/join deadlocks. It needs a dedicated gated
integration test before the first asynchronous playback remote is migrated.

The remaining factory structures are explicit as well. Immutable configuration
is registered directly; it does not require an instance factory.

```c
typedef struct mc_dsp_factory {
    uint32_t struct_size;
    void *context;
    const mc_dsp_vtable *instance_api;
    mc_result (MC_CALL *create)(void *, const mc_plugin_services *, void **, mc_error **);
} mc_dsp_factory;

typedef struct mc_tag_reader_factory {
    uint32_t struct_size;
    void *context;
    const mc_tag_reader_vtable *instance_api;
    mc_result (MC_CALL *can_read)(void *, mc_string_view extension, uint32_t *supported);
    mc_result (MC_CALL *create)(void *, const mc_plugin_services *, void **, mc_error **);
} mc_tag_reader_factory;

typedef struct mc_indexer_source_factory {
    uint32_t struct_size;
    void *context;
    const mc_indexer_source_vtable *instance_api;
    mc_result (MC_CALL *create)(void *, const mc_plugin_services *, void **, mc_error **);
} mc_indexer_source_factory;

typedef struct mc_analyzer_factory {
    uint32_t struct_size;
    void *context;
    const mc_analyzer_vtable *instance_api;
    mc_result (MC_CALL *create)(void *, const mc_plugin_services *, void **, mc_error **);
} mc_analyzer_factory;

typedef struct mc_visualizer_factory {
    uint32_t struct_size;
    void *context;
    const mc_visualizer_vtable *instance_api;
    mc_string_view name;
    uint32_t frame_kinds;          /* PCM and/or spectrum; declares nullable delivery methods. */
    mc_result (MC_CALL *create)(void *, const mc_plugin_services *, void **, mc_error **);
} mc_visualizer_factory;

typedef struct mc_playback_remote_factory {
    uint32_t struct_size;
    void *context;
    const mc_playback_remote_vtable *instance_api;
    mc_result (MC_CALL *create)(void *, const mc_plugin_services *, void **, mc_error **);
} mc_playback_remote_factory;

```

Playback remotes submit bounded commands to the owner dispatcher. Rejection
produces no callback; accepted commands keep completion state until delivery even
if their client disconnects. Disconnect discards a response rather than retracting
a command already accepted. A user may therefore hear its effect after the
controlling connection disappears. This plugin-command policy is separate from
application request cancellation.

Shutdown rejects new commands and cancels queued ones before joining remote
workers and unloading code. A remote may acquire an owned playback handle through
the same bounded dispatch, releasing it afterward; cancelled acquisition yields
no handle. Completion runs on the owner and can race the submitting worker.
Callbacks must not wait on another owner completion. Keep network threads free
while commands execute, and correlate responses that can arrive out of order.
Owned clients wait for queue snapshot creation before using it. Asynchronous
metadata preparation does not relax this nonblocking network-thread rule.

If one remote fails startup, stop and join that remote independently while leaving
healthy remotes running. It must detach external callbacks and wake its workers
without depending on another remote stopping. Workers waiting on its accepted
host commands can finish because shutdown cancels those before joining. Per-client
execution cancellation would require additional identities and coordination with
disconnect/execution races; the accepted-command policy avoids that machinery.

#### Scoped services and configuration

Host-provided per-instance tables use `mc_plugin_library_service`,
`mc_plugin_playback_service`, `mc_plugin_indexer_service`,
`mc_plugin_media_service`, `mc_plugin_settings_service`, and
`mc_plugin_audio_scratch_service`. The service bundle exposes `playback` and
`indexer`; bootstrap logging/registration remains `mc_host_api`. General scans
use `indexer.schedule_async`; source plugins receive only their own
`indexer.request_rescan` callback. Application wrappers and plugin services are
separate ownership boundaries. The existing private C++ adapter owners are
`PluginLibraryService` and `PluginMediaService`; there is no C++ class solely to
mirror every C table. Bootstrap remains `mc_host_api`, while capability IDs and
provider factory names identify provider roles. General scheduling is absent for
source-only services and uses the same owner-command queue, completion, and
shutdown cancellation when granted to playback remotes. Experimental SDK renames
and layout changes require rebuilding plugins, without compatibility aliases;
the LLD records the migration.

The instance service bundle holds pointers to narrow, sized, typed tables. It
is immutable and valid until that instance is destroyed; absent optional tables
are null. Each table has its own opaque core context. Factory creation receives
the bundle, so bootstrap registration need not mutate a global environment or
read services before they exist. All instances and their scoped host callbacks
remain inside the engine session. Core owns and destroys peer-provider objects
before registry shutdown; these objects do not carry per-plugin lifetime leases.

The implemented service bundle, media operations, and plugin settings are
defined in [services.h](../../include/musikcore/plugin/services.h),
[media.h](../../include/musikcore/plugin/media.h), and
[settings.h](../../include/musikcore/plugin/settings.h). A worker creates a stream
from a URI, opens it, then selects its decoder from the opened stream's metadata.
Both playback and server transcoding follow this sequence. The server passes
the decoded PCM view to the encoder before reading another block.

The settings names are consistent across both boundaries: application operations
use `mc_settings_*` in `settings.h`, shared values use `mc_setting_*` in
`setting_types.h`, and plugins receive `mc_plugin_settings_service` through
`services.settings`. The header-only application owner is `api::Settings`.
Shared value types contain no application entry points, so plugin headers do not
depend on the application API. Existing on-disk file names and keys are preserved.

Applications can write declared plugin configuration through
`mc_plugin_write_settings`. A plugin with its own controls can use the scoped
service's `write_values` callback. It validates the entire batch and persists
before publishing the in-memory update; rejected input and I/O failure leave
in-memory values unchanged. Physical file replacement is not guaranteed atomic
on an I/O failure. These calls are thread-safe, perform disk I/O off real-time
callbacks, and do not implicitly reload active instances. Only the owning plugin's
store is accessible through this service.

The settings API names and appended plugin write callback change experimental
source names, application exports, and the plugin table layout. Rebuild clients
and plugins; there are no old-name aliases or ABI version increments before
freeze. This changes naming and write access, not stored configuration keys.

A plugin receives its own setting store and the application data directory.
The HTTP source registers its integer settings with defaults and bounds. Its
display name still selects the original setting file, preserving saved
values. Each new transfer reads the current settings. GME and OpenMPT retain
their existing component settings and register their decoder, stream,
indexer source, and settings together from one plugin each.

The remaining design calls for media encoder factories, setting writes and
saves, richer schema types and apply/restart semantics, and system, catalog,
playback and indexer services. Add these tables with their first concrete users.
A system service should supply file modification times and other paths when
needed. System time shared with output timing must use the host's monotonic clock.

Future schema operations should describe type, enum choices, explicit bounds,
decimal precision and restart requirements. The host owns copied descriptors
and renders controls. Apply and save run outside the audio callback.

The proposed asynchronous services below use a call-scoped completion error:

```c
typedef void (MC_CALL *mc_plugin_completion_fn)(mc_result status, const mc_error *error, void *user_data);
```

Catalog and playback services reuse the application API's C value types and
operation semantics. The opaque scope replaces an application-owned context;
plugins cannot dispatch or destroy the application's context. Synchronous reads
are owner-thread operations unless explicitly documented otherwise. Async
submissions copy inputs and are available to workers, with owner-thread terminal
callbacks. Returned views/owned results keep the existing public API lifetimes.

```c
typedef uint32_t mc_plugin_library_kind;
enum { MC_PLUGIN_LIBRARY_LOCAL = 1, MC_PLUGIN_LIBRARY_SELECTED = 2 };
typedef void (MC_CALL *mc_plugin_json_fn)(mc_result, mc_string_view json,
    const mc_error *, void *);

struct mc_catalog_host_api {
    uint32_t struct_size;
    void *context;

    mc_result (MC_CALL *get_library)(void *, mc_plugin_library_kind, mc_library **);

    void (MC_CALL *destroy_library)(mc_library **);

    mc_result (MC_CALL *query_tracks_async)(
        void *context,
        const mc_library *library,
        const mc_track_query *query,
        mc_track_query_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *open_track_search_async)(
        void *context,
        const mc_library *library,
        const mc_track_search *search,
        mc_track_search_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *open_playlist_async)(
        void *context,
        const mc_library *library,
        int64_t playlist_id,
        mc_track_search_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *open_category_async)(
        void *context,
        const mc_library *library,
        mc_string_view category,
        int64_t id,
        mc_track_search_order order,
        mc_track_search_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *open_directory_async)(
        void *context,
        const mc_library *library,
        mc_string_view directory,
        mc_track_search_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *list_category_fields_async)(
        void *context,
        const mc_library *library,
        mc_category_fields_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *filter_categories_by_name_async)(
        void *context,
        const mc_library *library,
        const mc_category_name_query *query,
        mc_category_page_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *get_related_categories_async)(
        void *context,
        const mc_library *library,
        const mc_related_categories_query *query,
        mc_category_page_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *list_albums_async)(
        void *context,
        const mc_library *library,
        mc_string_view filter,
        mc_album_list_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *read_thumbnail_async)(
        void *context,
        const mc_library *library,
        int64_t thumbnail_id,
        mc_thumbnail_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *list_playlists_async)(
        void *context,
        const mc_library *library,
        mc_playlist_list_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *save_playlist_async)(
        void *context,
        const mc_library *library,
        int64_t playlist_id,
        mc_string_view name,
        const int64_t *track_ids,
        size_t track_count,
        mc_playlist_write_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *rename_playlist_async)(
        void *context,
        const mc_library *library,
        int64_t playlist_id,
        mc_string_view name,
        mc_playlist_write_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *delete_playlist_async)(
        void *context,
        const mc_library *library,
        int64_t playlist_id,
        mc_playlist_write_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *append_playlist_async)(
        void *context,
        const mc_library *library,
        int64_t playlist_id,
        const int64_t *track_ids,
        size_t track_count,
        mc_playlist_write_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    mc_result (MC_CALL *make_track_result)(
        const mc_library *library,
        const int64_t *ids,
        size_t count,
        mc_track_result **out_result);

    size_t (MC_CALL *track_result_get_count)(const mc_track_result *tracks);

    int64_t (MC_CALL *track_result_get_id)(const mc_track_result *tracks, size_t row);

    bool (MC_CALL *track_result_starts_album)(const mc_track_result *tracks, size_t row);

    uint64_t (MC_CALL *track_result_get_album_duration)(const mc_track_result *tracks, size_t row);

    mc_result (MC_CALL *track_result_get_ref)(
        const mc_track_result *tracks,
        size_t row,
        mc_track_ref **out_track);

    mc_result (MC_CALL *track_result_read_async)(
        void *context,
        const mc_track_result *tracks,
        const mc_track_range *range,
        mc_track_query_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    void (MC_CALL *track_result_destroy)(mc_track_result **tracks);

    size_t (MC_CALL *track_page_get_count)(const mc_track_page *page);

    size_t (MC_CALL *track_page_get_field_count)(const mc_track_page *page);

    mc_string_view (MC_CALL *track_page_get_field_name)(const mc_track_page *page, size_t field);

    size_t (MC_CALL *track_page_get_value_count)(
        const mc_track_page *page,
        size_t row,
        size_t field);

    mc_string_view (MC_CALL *track_page_get_string_at)(
        const mc_track_page *page,
        size_t row,
        size_t field,
        size_t value);

    void (MC_CALL *track_page_destroy)(mc_track_page **page);

    int64_t (MC_CALL *track_ref_get_id)(const mc_track_ref *track);

    mc_result (MC_CALL *track_ref_get_library)(
        const mc_track_ref *track,
        mc_library **out_library);

    mc_result (MC_CALL *track_ref_read_async)(
        void *context,
        const mc_track_ref *track,
        const mc_string_view *fields,
        size_t field_count,
        mc_track_query_completion_fn completion,
        void *user_data,
        mc_request_id *out_request);

    void (MC_CALL *track_ref_destroy)(mc_track_ref **track);

    mc_result (MC_CALL *clone_track_ref)(const mc_track_ref *, mc_track_ref **);

    mc_result (MC_CALL *lookup_external_ids_async)(
        void *,
        const mc_library *,
        const mc_string_view *,
        size_t count,
        mc_track_search_completion_fn,
        void *user_data,
        mc_request_id *);

    mc_result (MC_CALL *remove_playlist_rows_async)(
        void *,
        const mc_library *,
        int64_t playlist_id,
        const size_t *rows,
        size_t count,
        mc_playlist_write_completion_fn,
        void *user_data,
        mc_request_id *);

    mc_result (MC_CALL *submit_library_request_json)(
        void *,
        mc_string_view json,
        mc_plugin_json_fn,
        void *user_data,
        mc_request_id *);

    mc_result (MC_CALL *cancel_request)(void *, mc_request_id);
};

struct mc_playback_host_api {
    uint32_t struct_size;
    void *context;
    mc_result (MC_CALL *get_snapshot)(void *, mc_playback_snapshot *);
    mc_result (MC_CALL *get_current_track)(void *, mc_track_ref **);
    mc_result (MC_CALL *get_queue)(void *, mc_play_queue **);
    void (MC_CALL *destroy_queue)(mc_play_queue **);
    mc_result (MC_CALL *queue_get_tracks)(const mc_play_queue *, mc_track_result **);
    size_t (MC_CALL *queue_get_current_row)(const mc_play_queue *);
    mc_result (MC_CALL *control_async)(void *, mc_playback_command, double value,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *use_track_async)(void *, const mc_track_ref *, mc_playback_track_action action,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *replace_queue_async)(void *, const mc_track_result *,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *insert_queue_async)(void *, const mc_track_result *, mc_playback_track_action action,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *move_queue_row_async)(void *, const mc_play_queue *, size_t from, size_t to,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *remove_queue_row_async)(void *, const mc_play_queue *, size_t row,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *clear_queue_async)(void *, mc_plugin_completion_fn, void *, mc_request_id *);
    mc_result (MC_CALL *get_equalizer)(void *, mc_equalizer_state *);
    mc_result (MC_CALL *set_equalizer_async)(void *, const mc_equalizer_state *,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *set_replay_gain_async)(void *, mc_replay_gain_mode,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *set_preamp_async)(void *, double decibels,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *cancel_request)(void *, mc_request_id);
};

struct mc_indexer_host_api {
    uint32_t struct_size;
    void *context;
    mc_result (MC_CALL *get_state)(void *, mc_indexer_state *);
    mc_result (MC_CALL *schedule_source_rescan)(void *, mc_string_view source_key,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *schedule_scan)(void *, mc_indexer_scan,
        mc_plugin_completion_fn, void *user_data, mc_request_id *);
    mc_result (MC_CALL *cancel_scan)(void *);
    mc_result (MC_CALL *cancel_request)(void *, mc_request_id);
};
```

Queue actions use existing `mc_playback_track_action` values and snapshot
semantics. Input result/snapshot/track handles are retained by accepted async
commands. Their callbacks report execution, not merely enqueue success. The
server requests the local library explicitly; it must not accidentally serve a
remote selection from the UI. Equalizer/replay-gain/preamp command wrappers use
existing core operations rather than duplicating their implementation.

External-ID lookup and position-specific saved-playlist removal are identified
host-call gaps to resolve against the typed backend before server conversion.
The latter must preserve duplicate entries and define stale-row handling before
accepting concurrent edits; do not implement it by silently replacing a stale
whole-playlist snapshot. Registered query JSON is a transport operation only:
validate the existing named operation/schema and bounds, preserve authentication
at the server, and dispatch typed backend queries. It never evaluates SQL text.

Host request/subscription state belongs to the requesting plugin instance.
Track callback admission, cancellations, and owned result destruction through
terminal completion within the engine session. Inline views expire after the callback;
owned results remain until the matching release. No callback may occur after
instance destruction or plugin unload. Instance teardown must cancel/drain these
callbacks through owner dispatch before joining dependent workers. The detailed
completion/error/cancellation semantics reuse the application C API contracts,
including rejected submissions having no callback and accepted requests settling
once. No new scheduling or callback ABI is needed merely to rename an interface.

#### Shape review and first implementation checks

Before materializing the headers, resolve the persisted source/time/setting
mapping decisions, shared enum types, async wrapper details, and typed host-call
gaps identified above. Compile the sketches
as C11 and C++ using existing public primitive headers; that checks syntax and
name collisions, not functionality or ABI readiness. Actual headers need every
field/default documented and standalone compile checks.

The first implementation path proves unopened cancellable streams, known decoded
frames, frame-count buffers, and the timed null output. Test short reads/writes,
final frames at EOF, failed/partial creation, callback-before-return submissions,
stop/drain token return, and no per-sample allocation/call. Each later family
adds behavior/failure/lifetime tests at its own boundary. Empty optional families
stay valid throughout; old application workflows are restored as their required
families become available. The LLD records explicit temporary test prerequisites
rather than weakening old expectations silently.

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

- Add playback, queue, settings, and indexer APIs.
- Migrate musikcube feature by feature.
- Add advanced audio consumer APIs only where an identified consumer needs
  them.

### Provider replacement sequence

Core consumer acceptance precedes this work. On the provider branch, draft all
family contracts together, disconnect the former C++ SDK path, introduce the
session-owned registry, establish empty/null behavior, prove one representative
per family, and convert the rest. The preserved core-completion branch remains
the working fallback. Do not keep both plugin architectures active in the new
engine or add compatibility adapters to carry the old SDK forward.

The [LLD plugin execution order](musikcore-c-abi-lld.md#approved-execution-order)
owns tasks, acceptance criteria, and current evidence. Freeze ABI v1 only after
the replacement, native platform validation, and installed/archive consumers
are complete.

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
- Which event-loop integration is needed beyond blocking/nonblocking event
  retrieval?
- Which metadata fields need typed numeric storage rather than UTF-8 values?
- Is a general track snapshot handle useful in addition to page results?
- Should queue edits and track-list edits share one batch operation format?
- Which advanced audio APIs must be public in ABI major version 1?
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
use private C++ implementation wrappers where helpful, without keeping the
former C++ plugin SDK active in the replacement engine. This design makes
musikcore both embeddable across runtimes and extensible by plugins written
outside C++ without putting C++ ABI details into the public contract.
