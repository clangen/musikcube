# musikcore C ABI modernization: low-level design and task plan

Status: core consumer acceptance complete in its recorded scope; clean C plugin replacement is next

The [core C API foundation gate](#core-c-api-foundation-gate) supersedes
the original phase ordering below. Additional application feature migrations
wait for that gate. All provider tasks, including the SDK walking skeleton and
decoder vertical slice, wait for core application API release-candidate quality.
Existing checkboxes describe implementation progress; unchecked platform gates
remain mandatory and are not implied by completed implementation tasks.

## Purpose

This document translates the
[high-level design](musikcore-c-abi-hld.md) into concrete build targets, source
layout, runtime components, state machines, test fixtures, migration steps, and
trackable tasks.

The core application API is consumed by musikcube and has reached the scoped
consumer acceptance recorded under CORE-RC-003. The active body of work is a
clean replacement of the plugin architecture on `clangen/c-plugin-api-foundation`.
This document is the task/status authority; the HLD links architectural decisions,
and public headers define parameter-level contracts. Runtime and provider
sections describing future services are target design, not implemented features.

The task list uses stable identifiers so progress and dependencies can be
discussed without relying on line numbers. Checkboxes represent repository
state, not design intent.

Status legend:

- `[ ]` acceptance remains open; accompanying text identifies partial work,
  deferred scope, or a platform dependency.
- `[x]` implemented and validated in the stated scope. An explicitly `retired`
  task records a superseded migration route, not an implementation claim.
- Platform gates remain open until their native acceptance passes or the
  foundation gate records a maintainer-approved exception.
- A task annotated `blocked by` must not begin until its dependency gate passes.

The active next steps and plugin execution order below govern the current work.
Historical phase numbers group related tasks; they do not require the old
interleaved provider migration order. Previously checked bootstrap tasks record
working building blocks, not completion of the replacement architecture.

## Related documents

- [High-level architecture and sequencing](musikcore-c-abi-hld.md)
- [Detailed public API and C SDK design](musikcore-c-abi-design.md)
- [Unit and integration testing design](testing-design.md)

## Decisions this plan implements

- [C ABI and core-first sequencing](../docs/adr/0001-c-abi-and-core-first-refactor.md):
  interoperability goal, alternatives, header-only wrapper, shared-library rollout,
  application migration, and deferred provider work.
- [Host dispatch and request lifetime](../docs/adr/0002-host-dispatch-and-request-lifetime.md):
  async delivery, cancellation, wakeups, and shutdown guarantees.
- [Origin-bound results and paged metadata](../docs/adr/0003-origin-bound-results-and-paged-metadata.md):
  identity, snapshot/page ownership, backend routing, and copy-cost tradeoffs.
- [Tests at the public boundary](../docs/adr/0004-test-the-public-boundary-first.md):
  contract-first coverage, hermetic fixtures, and expansion alongside app changes.
- [C provider loading and lifetime](../docs/adr/0008-c-provider-bootstrap-and-unload-lifetime.md):
  registry loading/ownership responsibilities, typed registration, scoped services, and unload.

ADRs own the choice and rationale. This plan records implementation mechanisms,
current ABI details, remaining work, and acceptance evidence. Change a decision
through its ADR rather than adding a competing policy to a task description.

## Constraints from the current repository

- Unix-like builds currently define `musikcore` as a shared CMake target.
- Windows builds `musikcore.dll` and shares it between the GUI and console frontends.
- Windows and Unix build the engine from the shared source list in
  `.cmake/MusikcoreSources.cmake`.
- The retired legacy C boundary (`c_context.cpp`, `c_interface_wrappers.cpp`,
  and `musikcore_c.h`) has been removed. The new consumer boundary lives in
  `src/musikcore/ffi/consumer`.
- Existing plugins export C-named entry points that exchange C++ SDK pointers.
- CTest now runs header, lifecycle, dispatch, query, wrapper, backend-switch,
  export, and production/remote integration tests.
- Production binaries and plugins currently share source-tree output
  directories in several configurations.

The core-completion branch preserves the working application baseline. The
provider branch keeps targets buildable and unavailable capabilities explicit
while families are restored; it does not keep the old plugin architecture active
merely to preserve functionality during reconstruction. Record temporary test
prerequisites under PLG-EMPTY-002.

## Proposed source layout

```text
include/
    musikcore/
        abi.h                   shared C ABI primitives
        musikcore.h             public application umbrella
        context.h               runtime lifecycle and request dispatch
        library.h               queries and metadata pages
        plugin.h                provider bootstrap
        plugin/                 typed family/host-service C headers
        musikcore.hpp           public C++ wrapper
        plugin.hpp              optional C++ provider wrapper

src/musikcore/
    ffi/
        common/
            export.h            private export implementation helpers
            version.cpp
            error.cpp
            string.cpp
            handle.cpp          only if generic handle support is needed
        consumer/
            context.cpp
            event.cpp
            request.cpp
            library.cpp
            track_page.cpp
            track_list.cpp
            playback.cpp
            indexer.cpp
            preferences.cpp
            audio.cpp
        plugin_abi/
            PluginRegistry.h/.cpp
            Plugin.h/.cpp       per-library state and lifetime
            stream.cpp          C contracts and engine-owned implementation wrappers
            decoder.cpp
            encoder.cpp
            output.cpp
            dsp.cpp
            metadata.cpp
            indexer.cpp
            visualizer.cpp
            services.cpp

tests/
    scripts/                    orchestration and fixture generation
    unit/musikcore/             focused core units
    unit/musikcube/             focused application units
    integration/api/            standalone C/C++ contract consumers
    integration/app/            application/core integration
    integration/library/        typed SQLite query contracts
    integration/transport/      loopback and protocol boundaries
    integration/binary/         linkage and installed-artifact checks
    integration/performance/    explicitly selected native baselines
    ui/scenarios/               production user workflows
    ui/driver/                  parser, PTY, and artifact utility tests
    ui/support/                 reusable terminal/benchmark helpers
    support/                    shared native fixtures and checks
    baselines/
```

The exact file count may be reduced during implementation. The important
separation is:

- Installed C headers do not include private headers.
- `ffi/common` owns behavior shared by both directions of the C boundary.
- `ffi/consumer` contains the application-facing façade, exception boundaries,
  and validation.
- `ffi/plugin_abi` contains loading, registry ownership, typed C family support,
  and engine-owned wrappers; no wrapper implements the former C++ SDK.
- Public C++ convenience wrappers are header-only and use public C declarations.
- Library/playback attachment adapters have been removed from Cube. Test-only
  probes stay out of standalone production builds.

## CMake target topology

### Current build topology

Unix production builds compile the shared engine sources from
`.cmake/MusikcoreSources.cmake` into position-independent `musikcore_objects`.
Those objects supply the shared, C-export-only production `musikcore`. With
`BUILD_TESTING`, they also supply private static `musikcore_internal` for tests
that need implementation access. The normal musikcube and musikcubed targets
use the production public C ABI; they do not link the private test archive.

Windows builds `musikcore_objects` from the same source list, then
shares `musikcore.dll` between console and GUI frontends. Private integration
tests reuse `musikcore_internal` without widening the DLL exports. The reduced
`musikcore_abi0` shared target remains separate. Native Windows Debug/Release
checks cover PE exports, both frontend import tables, installed consumers, and
runtime placement; retain the recorded revision/scope of that evidence.

`musikcore_abi0` is a reduced standalone contract-test runtime, not a second
production engine or the target used by Cube. It shares consumer ABI sources
through `.cmake/MusikcoreConsumerApi.cmake`, supplies SQLite reads, and rejects
unsupported production-service operations. `musikcore_cpp_abi0` is its public
header-only C++ interface target. Its runtime files remain under `bin/abi0`.

The earlier proposal to introduce `musikcore_legacy` was superseded by this
layout. There is no target or retired C boundary to preserve under that name.
Common production source lists are still duplicated between Unix and Windows;
BLD-001 tracks consolidation separately from the implemented Unix object reuse.

### Process isolation rule

Implement the single-core-per-process decision in
[ADR-0001](../docs/adr/0001-c-abi-and-core-first-refactor.md) with
`.cmake/VerifyCoreBoundary.cmake` and the positive/negative `link_boundary`
fixtures. A consumer must not combine production `musikcore`, the reduced
`musikcore_abi0`, or `musikcore_internal`. Each test selects one implementation;
shared-library consumers use only its exported C functions.

### Final topology

After migration:

```text
musikcore_impl_objects --> musikcore SHARED
musikcore_cpp          --> musikcore
musikcube              --> musikcore_cpp
plugins                --> public plugin headers only
```

The reduced `musikcore_abi0` test runtime is retired when production-bound
contract fixtures replace its remaining uses. No `musikcore_legacy` target
exists to remove. The final shared target uses ABI-major platform naming and
installs headers plus package metadata.

## Export control

The experimental and final shared targets must use two layers of export
control:

1. `MC_API`/`MC_PLUGIN_API` and `MC_CALL` declarations in headers.
2. A platform export allowlist generated or maintained from the public symbol
   list.

Platform mechanisms:

- Windows: explicit `__declspec(dllexport/dllimport)` and optionally a `.def`
  verification file; never `WINDOWS_EXPORT_ALL_SYMBOLS`.
- Linux/ELF: hidden default visibility plus a linker version script.
- macOS: hidden default visibility plus an exported-symbols list.

A packaging test compares actual exports with the allowlist.

## Runtime component design

### Context

Lifecycle and delivery decisions are in
[ADR-0002](../docs/adr/0002-host-dispatch-and-request-lifetime.md).

The production context owns the managed runtime, request registry, dispatch
queue, selected library, playback service, and explicit URI-player children.
Creation selects a fixed service mask; prerequisites expand automatically and
the default enables all services. Library and plugin services are independent;
indexing and catalog playback each add both. URI players require plugins only.

Plugin discovery, service registries, preferences, and path configuration retain
process-level constraints; this is not isolated multi-engine hosting. The
reduced `musikcore_abi0` target does not instantiate the production runtime.

Current ABI0 permits one context. A second create returns `MC_ERROR_BUSY`;
only successful destruction releases the slot.

Context lifecycle states:

```text
constructing -> running -> shutting_down -> stopped -> destroyed
                    |             ^
                    +-- failure --+
```

Construct transactionally and publish only after required initialization.
The worker join, hook barrier, and terminal drain implement the shutdown
contract in ADR-0002; the host drains/cancels its posted dispatch tasks before
destroying the context.

### Request registry

Each accepted operation records its callback, user data, and nonzero
`mc_request_id`, and reserves a terminal completion slot:

```text
accepted -> queued -> running -> completed
                   \-> cancelled
                   \-> failed
```

The registry implements ADR-0002's exactly-once contract. It records retained
inputs/origin, callback state, and terminal capacity before admission returns.
Select completion/cancellation under synchronization, then dispatch without
holding that lock. IDs are nonzero, monotonic within the context, and not reused
while a completion can remain queued.

### Dispatch and notifications

- [x] Replace core sigslot usage and expose caller-owned subscriptions in C
  and header-only C++. `musikcore.unit.subscriptions` covers reentrant and
  concurrent disconnect, including mixed C/C++ delivery across a shared library.
  `musikcore.abi.shared_c` covers C handle ownership and
  self-disconnect across the shared-library boundary.

Completion producers enqueue terminal records for the host-selected dispatcher.
Coordinate wakeup coalescing/rearming with enqueue under the same synchronization
so a producer racing with the final drain cannot strand work. Invoke callbacks
and wakeup hooks outside the queue lock.

Unsolicited notification polling remains unimplemented. When added, reserve
lossless terminal/state notification capacity at admission, coalesce only the
documented progress/invalidation records, and transfer ownership on dequeue.
This future mechanism must preserve the separation selected in ADR-0002.

### Error objects

`mc_error` is core-owned. Synchronous public functions optionally return it on
failure. The request owns its error until the terminal callback returns.

Plugins create detailed errors through `mc_host_api::create_error`; they never
allocate an `mc_error` directly.

Every exported C function catches all C++ exceptions and translates them to a
result and optional error. Allocation failure has a defined fallback result
even if a detailed error object cannot be created.

### Metadata pages

Implement the page ownership and range-read decision in
[ADR-0003](../docs/adr/0003-origin-bound-results-and-paged-metadata.md) with
immutable `mc_track_page` storage containing:

- Row count.
- Requested field descriptors or stable field ordering.
- Fixed-width row identities; numeric album durations live on search results.
- Page-owned field names and rows of UTF-8 string values.
- Stable views into that storage for the page lifetime.

`query::ProjectedTrackPage` currently owns vectors of strings. It is immutable
once published but is not a packed arena with offsets. LIB-002 retains the
compact-storage optimization as deferred work; current zero-copy access and
cache budgets do not require that representation.

Reads create new pages rather than mutating published storage. Future
track-metadata invalidation events remain unimplemented. Existing library,
playlist, indexer, and playback callbacks do not provide a general metadata
invalidation stream. Current lifetime/error details are recorded below and in headers.

### Plugin registry

`PluginRegistry` uniquely owns accepted `Plugin` records for one engine session.
Each record holds its native library, identified ABI/table, copied identity and
metadata, initialization state, and registered factory tables. There are no
per-plugin lease, instance, or active-call counters.

Lifecycle states:

```text
opened -> identified -> initializing -> running -> stopped
             failure ---------------------------> stopped
```

Typed factory views refer to immutable plugin tables without owning them. Registry
ownership keeps their DLLs loaded during normal operation. Before `Shutdown`,
the engine stops new work, interrupts/drains instances, finishes callbacks,
joins workers, and destroys all plugin instances. `Shutdown` synchronously calls
plugin cleanup and closes native libraries on the owner thread. It does not
wait for capability values to be discarded. Calling through a view after
shutdown is a contract violation; individual mid-session unload is unsupported.

The registry's public operations are `Initialize(directory, selection)`,
`ListPlugins()`, `Find<T>(pluginId)`, and `Shutdown()`. Discovery/loading and
candidate results remain private. Initialize scans only once; candidate
failures are logged and skipped, missing directories succeed empty, and fatal
directory failures close the session. `plugins.discovery` checks those rules,
thread/reentry checks, invalid paths, and throwing selection/diagnostic callbacks.
`plugins.null_output` exercises the type-to-ID mapping and typed invocation
against the production DLL; arbitrary table types are compile-time errors.

### Private family wrappers

Private wrappers may simplify calls to C provider tables without recreating the
former C++ SDK. Each wrapper:

- Uses immutable tables only within its engine session.
- Validates required function pointers at construction.
- Translates results and errors.
- Stops work and finishes callbacks before destruction.
- Calls the provider's destroy function exactly once before registry shutdown.
- Contains exceptions thrown by optional C++ convenience wrappers.

These wrappers own family instances, not plugin lifetime records. The engine's
shutdown order keeps their code and scoped services alive until cleanup finishes.

## Test target design

The strategy and alternatives are recorded in
[ADR-0004](../docs/adr/0004-test-the-public-boundary-first.md). The roles below
describe target responsibilities; current executable names and registration
live in `tests/CMakeLists.txt` and `tests/support/CMakeLists.txt`.

### API contract executable

`musikcore_api_contract_tests`:

- Compiled as C11.
- Includes only `include/`.
- Links only the `musikcore_abi0` import/shared library.
- Runs with unique temporary data and plugin roots.
- Exercises version, context, errors, strings, events, requests, pages, and
  later feature families.

### SDK fixture plugins

Each fixture is a real dynamic module compiled as C11 against only installed
SDK headers. Initial fixtures:

- `mc_test_plugin_valid`
- `mc_test_plugin_incompatible`
- `mc_test_plugin_truncated`
- `mc_test_plugin_init_failure`
- `mc_test_plugin_invalid_provider`
- `mc_test_decoder`

The fixtures expose deterministic behavior selected by URI or configuration.

### Internal unit tests

Internal C++ tests link private objects or a dedicated test library. They do not
replace API/SDK contract tests. Initial suites cover request state, event queue,
page arena, plugin lifetime, adapters, and audio-buffer transitions.

### Integration tests

Integration tests run the shared façade with real SQLite, workers, plugin
loading, and deterministic providers. They use deadlines and event predicates,
never sleeps.

## Migration slices

### Public API migration order

1. Context, errors, events, requests.
2. Metadata query and immutable page.
3. Track identity/list and cache window.
4. Preferences.
5. Category and directory queries.
6. Playback snapshots and commands.
7. Queue batch editing.
8. Indexer snapshots, commands, and progress.
9. Output selection, equalizer, replay gain, and transport settings.
10. Advanced stream/decoder/encoder consumer APIs where required.

### Provider SDK migration order

1. Common audio buffer and stream tables.
2. Decoder factory and decoder.
3. Data-stream factory.
4. Encoder factory and encoders.
5. Tag reader/store.
6. Indexer source/writer/notifier.
7. Audio output and devices.
8. DSP and analyzers.
9. Visualizers, remotes, server integration, and schema/configuration.

## Current experimental core contracts

The supported reduced-test and production surfaces are summarized in the
[HLD](musikcore-c-abi-hld.md#current-experimental-scope). Unsupported operations
reject admission with `MC_ERROR_UNSUPPORTED` and deliver no callback.
In production, `plugin_directory` is copied and used when Plugins is selected;
that service starts provider discovery. Its process-selected directory is shared
by both discovery paths; omitting it on a later context reuses the original
selection. `plugins.runtime` tests explicit/custom, omitted, and explicit-again
sessions with fixtures outside the executable default directory. The reduced
ABI-only target supplies no production plugin loader.

### Declarations and validation

Public C declarations live in independently compilable subsystem headers under
`include/musikcore`; `musikcore.h` is the application umbrella. They
contain parameter-level nullability, units, defaults, ownership, and lifetimes.
The `musikcore.abi.inventory` test compares declarations with
`src/musikcore/ffi/exports.txt`; `exports` checks the actual binary. Generate a
current declaration inventory with `node tests/scripts/audit-core-api.js` rather
than maintaining a second symbol list or per-export prose table.

- Handles must have the correct kind and remain live through a call. Arbitrary
  dangling or forged pointers cannot be validated. Immutable reads can run
  concurrently; destroying a handle concurrently with its use is invalid.
- Initialize output handle/error slots to null. Occupied outputs are rejected
  unchanged. Destruction accepts null and clears a successfully consumed slot;
  do not share mutable output slots between concurrent calls.
- Sized option inputs accept a known prefix and default absent fields. Snapshot
  outputs require the full current prefix and preserve a caller's future tail.
  Every sized public struct has an `MC_*_DEFAULT` initializer.
- Text views reject null/nonzero length, embedded NUL, and lengths above
  `PTRDIFF_MAX`. Callers supply UTF-8; the API does not normalize it. Validate
  field counts/page limits before inspecting arrays.
- Track IDs are positive and library-scoped; zero is the missing-ID sentinel.
  Reject null/nonempty arrays, excessive counts, and nonpositive IDs. Invalid
  IDs returned by a backend become `MC_ERROR_IO`. Category IDs are signed and
  may include an uncategorized sentinel. Playlist ID zero means creation; an
  older remote server may also return zero after successful creation.
- Missing metadata has null data. Explicit empty text has non-null data and
  zero length. Queue row absence is `SIZE_MAX`; zero album duration alone does
  not distinguish no group from an unknown/zero duration: use `starts_album`.

### Requests, dispatch, and shutdown

Implement the delivery and lifetime contract in
[ADR-0002](../docs/adr/0002-host-dispatch-and-request-lifetime.md). Current ABI
bindings return `MC_ERROR_BUSY` for nested/concurrent dispatch or shutdown; a
busy/failed destroy leaves the handle intact. Rejected submissions clear their
request-ID output and deliver no callback.

Completion arguments are status, payload, callback-scoped error, then user data;
array counts follow their pointers. Status is authoritative. Success has no
error; allocation failure can omit details. Page/result handles transfer to the
callback. Playlist arrays/names and error messages expire when it returns;
playlist-write IDs are values with no owned payload.

Allocating/dispatching operations use the C exception barrier. Scalar getters
and normal destructors do not allocate or invoke host code. Context destruction
contains cleanup failures. Foreign callbacks must not throw; dispatch contains
callback exceptions but currently has no public diagnostic channel for them.

### Library, result, and playback semantics

The read model and identity decision is in
[ADR-0003](../docs/adr/0003-origin-bound-results-and-paged-metadata.md).
A direct missing-track reference read returns `MC_ERROR_NOT_FOUND`. Plain queries
use a literal title substring; ordered search keeps typed-backend LIKE wildcards
or explicit regex matching across metadata fields.

A source-less remote identity cannot read metadata, including constructed
results/references. It rejects admission as unsupported. A bound empty range
still validates projection support and connection generation, then returns an
owned empty page without submitting an empty metadata query.

Modern servers support an optional lossless projection on the existing typed
batch query. Older servers can ignore it and return the legacy map; their
conversion cannot preserve every NULL/empty distinction or guarantee hidden-row
filtering. Tests cover modern parity and legacy fallback separately.

Playback has an owner-thread contract. Queue snapshots capture immutable
revisions; edits, shuffle, and library changes invalidate stale positional
actions before mutation. Stopped playback has no current row even before the
deferred engine notification, while its retained row remains available for
selection and Play Next. Queue-change notifications must run outside engine
signal locks and support safe replacement/unsubscription.

### Header-only C++ ownership and errors

Wrappers include only public C headers. Construction/admission failures throw
`api::Error`; async completion returns status, owned value, and a callback-scoped
message. Synchronous commands return C status. Snapshot getters use `nullopt`
for an ended/unavailable session and throw for other errors, including a wrong
thread. Destroy `Context` on its dispatcher thread outside callbacks.

The wrapper is strictly header-only per ADR-0001; TrackView ownership and
copy-cost tradeoffs are in ADR-0003. View-producing page accessors reject
temporary owners. `Values` allocates a vector of views, not string copies.
If shared-owner allocation fails during C page adoption, release the page and
deliver one internal-error completion. Playlist wrappers copy callback-scoped
names into owned strings so their returned vectors can be retained.

Request IDs and explicit cancellation follow ADR-0002. Any future coroutine or
future adapters remain optional and header-only.

## Test infrastructure

The organization/tooling decision is in
[ADR-0004](../docs/adr/0004-test-the-public-boundary-first.md#organization-and-execution).
This is the implementation checklist; there is no separate application-testing
proposal to reconcile.

- [x] **TINF-001** Move tests by level and boundary into the top-level `tests/`
  tree, with shared native helpers in `support/`, runners in `scripts/`, and UI
  scenarios/driver code under `ui/`. Update source paths and documentation.
- [x] **TINF-002** Give every CTest entry exactly one level label plus owner and
  subsystem labels. Provide `--suite`, `--list`, and bounded `--jobs` selection;
  level-specific build targets avoid building the app for unit-only runs.
- [x] **TINF-003** Report C++ wrapper checks as independently selectable public
  and production cases with fresh catalogs/processes. Cases initialize their own
  wakeup synchronization instead of relying on another case running first.
  Share C11/C++ expression/file/line diagnostics through `tests/support/check.h`.
- [x] **TINF-004** Add Node driver tests for split UTF-8/escape parsing, cursor,
  erase, scrolling, styling, CJK/combining cells, artifact retention, and real
  PTY resizing. `musikcube.unit.terminal_screen`, `musikcube.unit.ui_artifacts`,
  and `musikcube.ui.terminal_driver` exercise these boundaries.
- [x] **TINF-005** Register independent search, queue, playlist, and browse UI
  smoke scenarios. Remove fixed focus sleeps by activating overlays created
  during dispatch before painting them. The immediate-input playlist scenario
  fails on the previous ordering and passes after the fix.
- [x] **TINF-006** Preserve failed UI runtimes, configuration, indexer logs,
  raw terminal output, visible text and SVG; clean successful runs. Separate
  correctness smoke tests from explicitly selected catalog/timing benchmarks.

Windows runs the shared smoke scenarios through the native ConPTY driver with
an isolated runtime and profile. Emoji/ZWJ and terminal-specific glyph
presentation remain explicit screen-model limits. Add further tests alongside
the code being changed.
Recorded Linux results establish their stated revision and scope; this rebase
adds no new native Linux/Windows execution. New checks remain tied to the
platform evidence recorded below.

## Core C API foundation gate

Native macOS, Linux and Windows foundation validation is complete. Windows
closed the 2026-11-20 deferral with native x64 Debug and Release runs, installed
consumer checks, PE export and dependency audits, and a Release timing baseline.

The completed work below groups the former foundation checklist; detailed
feature tasks remain in the task list rather than a second parallel checklist.
Test names have the `musikcore.abi.` prefix unless fully qualified.

| Completed foundation area | Acceptance evidence |
| --- | --- |
| Contract reconciliation and ergonomics | Independent C11/C++ headers, `inventory`, `exports`, `arguments`, `shared_c`, C/C++ examples; consistent outputs, defaults, callbacks, lifetimes, validation, exception barriers |
| Context, request, dispatch, shutdown | `dispatch`, `remote_query_transport`; transactional/fault-injected creation, binding/reentrancy, exactly-once races, wakeup rearming, admission limits, late replies, repeated teardown |
| Library/results and backend parity | `query`, `result_contracts`, `library_switch`, `typed_track_queries`, `indexed_query`, `remote_query_transport`; ordering/duplicates/origin, range limits, NULL/empty/Unicode/long/multivalue data, missing/hidden rows, reconnects, playlists |
| Playback and queues | Production wrapper checks in `indexed_query`; owner threads, units/output preservation, stale revisions/origins, edits/duplicates/shuffle, stopped/retained/prepared/paused/playing rows, notifications |
| Strictly header-only C++ | Public-only consumers, `result_contracts`, window/cache tests; owning copy/move, retained TrackViews, raw view lifetimes, adoption failure and callback cleanup |
| Hermetic deterministic tests | Isolated config/database/plugin/runtime roots, deadline predicates, deterministic WAV bytes, fake output, dynamic loopback ports, `CoreRuntime`; no user data, hardware or internet |
| Shared-library boundary | `shared_c`, `exports`, `link_boundary`; independent C consumer, version rejection, isolated ABI0 outputs, Mach-O install names/RPATH/dependencies, no duplicate core linkage |
| Performance and allocations | Stored instrumented and uninstrumented baselines, C/C++ submission budgets, bounded cache, local/loopback parity; commands and limits below |

Additional runtime regressions are covered by the eleven standalone
`musikcore.runtime.message_queue.*` cases. They check delivery ordering,
removal/unregister, reentrant cleanup, teardown, scheduling, broadcasts,
wakeups, and producers without linking app services. The UI helper
`musikcube.unit.async_callback_scope` checks replacement, teardown, forwarding,
move-only handlers, and payload cleanup. The macOS
`musikcube.ui.terminal_driver` entry checks the script-only PTY bridge's input,
exit status, and cleanup. On Linux, `musikcore.integration.plugin.playback_reload`
checks decoding and playback with an invalid output module, then reloads
the output during playback with an unavailable selection and verifies restart.

Remaining acceptance:

- [x] **FND-WINDOWS** Native Visual Studio 2026 x64 Debug and Release pass 87/87
  each in separate build and runtime directories. `musikcore.abi.shared_c`,
  `musikcore.abi.exports`, `musikcube.integration.windows_core_dll`, and
  `musikcore.integration.api.installed_sdk` cover import-library loading, PE
  exports and runtime placement. The Release ABI-only run passes 30/30.
  `tests/scripts/run-baseline.js --timing-only` records ABI and production
  local/loopback measurements on Windows with a native Release build.
- [x] **FND-LINUX** Run clean native Linux Debug/Release tests and verify ELF
  SONAME/RUNPATH and actual exports.
- [x] **GATE-FOUNDATION** Record passing clean Debug/Release contract,
  integration, export, external-consumer and baseline validation on all named
  platforms, or an explicitly approved exception with owner, rationale, and
  replacement validation. Native macOS results do not establish DLL/ELF behavior.

The provider phases remain blocked until the migrated core API reaches
release-candidate quality.

- [x] **REMOTE-SESSION** Add explicit session configuration, asynchronous
  connect, state access, disconnect, and generation-bound library opening.
  `musikcore.integration.api.backend_parity` uses the public session for
  loopback queries and checks retained results across reconnect.
  `mc_remote_session_set_callback` observes an explicit session independently of
  the selected library through context dispatch. Initial/current state may be
  coalesced; replacement, removal, session destruction, and context shutdown
  suppress queued delivery. `musikcore.integration.api.managed_remote` checks
  those lifetimes, dispatcher delivery, and self-removal on macOS; Linux and
  Windows execution remain pending for this extension.
  `musikcore.integration.api.remote_versions` checks incompatible major/minor
  and malformed versions, immediate peer close, state notifications, retained
  terminal details after session destruction, and compatible patch differences.
  Mismatch returns `MC_ERROR_UNSUPPORTED` / `MC_REMOTE_INCOMPATIBLE_VERSION` with
  server and required version details.
- [x] Supply Track Search with the same public library interface for either
  backend. The app binds its selected backend at the API boundary.
  `musikcore.abi.remote_query_transport` verifies that search results can
  replace the playback queue and that switching libraries invalidates old
  playback references.
- [x] Put library selection on the public context. App views submit reads
  through that context. `musikcore.abi.library_switch` verifies that
  changing and clearing the selection preserve previously opened identities.
- [x] Attach playback before constructing the app's views. The views borrow
  the context. `musikcore.abi.remote_query_transport` verifies that handles
  opened through the context operate on the same playback queue.
- [x] Drive app selection, status, and notifications through the SDK context.
  `musikcore.abi.remote_query_transport` verifies selection and notification
  across local/remote switches and reconnects.
- [x] Retire the core `MasterLibrary` adapter. Playback uses the concrete
  selected catalog, and plugin metadata and indexing use the local catalog.
- [x] Query category rows through the context and insert category or directory
  results through public playback. `musikcore.integration.api.backend_parity`
  covers category queries. `musikcore.abi.remote_query_transport` covers
  ordered insertion and stale origin rejection.
- [x] Route cube indexer events and single-track and album actions through
  public operations. Remove the private backend resolver.
  `musikcube.ui.browse` verifies album insertion and category discovery
  through the TUI.
- [x] Pass one public library handle to Browse, Directory, Track Search and
  Now Playing track lists. Browse playlist writes and track rating use the
  public API. `musikcore.integration.api.backend_parity` covers rating
  writes in both directions and rejects writes from a stale connection.
  `musikcore.abi.remote_query_transport` checks hot swap origin validation.
  Album-divider Play and Add use a generation-bound category result.
  `musikcube.ui.browse` exercises album queue insertion, and
  `musikcore.abi.remote_query_transport` holds an album reply across a reconnect.
- [x] Bound pending typed queries per remote connection. The default is 64.
  Explicit remote sessions can override it via `mc_remote_options.max_pending_queries`.
  `musikcore.abi.remote_query_transport` verifies default admission and
  `musikcore.integration.api.managed_remote` exercises a custom session limit.

- [x] Include the playing track's loaded genre in its metadata page without a
  catalog query. The existing field positions are preserved.
  `musikcore.integration.api.production.playback_commands` covers this projection.

- [x] Expose the selected visualizer's current visibility through the C API.
  `musikcore.integration.runtime.visualizer_session` covers show and shutdown.

## Foundation validation and baselines

Native macOS arm64 validation at `1f8283ea7`: full Release and Debug suites
pass 104/104, including installed consumers, application import guards,
subscription cross-library delivery, URI players, remote audio/transcoding,
PCM transport stress, and UI scenarios. Logs:
`build/subscription-scope-tests-release.log` and
`build/subscription-scope-tests-debug.log`.

With TST-008/API-013 instrumentation enabled before all targets, native macOS
full Debug passes 109/109, ABI-only Debug passes 34/34, and plain Release
passes 104/104. This includes C and
C++ fault-detection probes, fatal UBSan, shared-library ownership/dispatch,
allocation failures, and the AppleClang 21 unload regression. The latter fails
without image-global cleanup but still diagnoses a real global overflow after
cleanup. Logs: `build/sanitizers-full-tests-debug-final.log`,
`build/sanitizers-abi-only-final.log`, and
`build/sanitizers-tests-release-final.log`. ASan-only and UBSan-only probe selections
also pass; Release and `none` compile/link commands contain no sanitizer flags.
The paused-crossfade test uses separate readiness and teardown deadlines after
concurrent Release/Debug runs exposed its shared 12-second budget. Paused
teardown and worker completion now have their own three-second watchdog.

The mixed C/C++ disconnect regression passes three consecutive focused Release
runs. The same current test compiled against the pre-fix `c17cf1d45` C++ header
hits a five-second deadlock deadline, confirming the cross-library seam rather
than only exercising two header-local publishers. This does not establish
Windows DLL behavior or full callback-state draining during reentrant disconnect.
Earlier production TUI validation used 10,000 tracks across three runs.

Native Linux evidence on 2026-11-20: Arch Linux x86-64, Zig 0.17.1 with
glibc 2.28 targeting. Clean Debug and Release standalone builds passed all
44 CTest entries each, including backend parity and the output reload test.
Clean ABI-only Debug and Release builds passed all 25 entries each. Verified
ELF SONAMEs, relative runtime paths, and the exported C symbol allowlist for
both core libraries. The Release build produced 15 plugin modules. All
resolved their dynamic symbols on the test host. The standalone dependency
scan passed at glibc 2.28, and the Linux timing-only baseline completed.

### Reproducible workflows

The C and header-only C++ examples in `examples/core-query.*` use only public
headers and a supplied local database. Their test host wakeup adapter models a
sleeping application loop. `indexed_query` builds and runs both against generated
indexed media. Full fixtures stage only required decoder/tag-reader plugins,
use fake playback and dynamic loopback ports, and retain failure directories
through `tests/scripts/run-isolated-test.js`.

With normal macOS vendor dependencies already built, run from the repository
root. Build-local runtime outputs keep Debug/Release assets independent:

```sh
cmake -S . -B build/foundation-full-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DBUILD_STANDALONE=true -DMUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY="$PWD/build/foundation-full-debug/bin"
cmake --build build/foundation-full-debug --target musikcube_tests musikcube -j6
ctest --test-dir build/foundation-full-debug -C Debug --output-on-failure -j6

cmake -S . -B build/foundation-full-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_STANDALONE=true -DMUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY="$PWD/build/foundation-full-release/bin"
cmake --build build/foundation-full-release --target musikcube_tests musikcube -j6
ctest --test-dir build/foundation-full-release -C Release --output-on-failure -j6

node tests/scripts/run-tests.js --build-dir build/foundation-debug --config Debug --abi-only --sanitizers address,undefined
node tests/scripts/run-tests.js --build-dir build/foundation-release --config Release --abi-only
node tests/ui/scenarios/track-search.js --build-dir build/foundation-full-release --bin-dir build/foundation-full-release/bin --tracks 10000 --windows --runs 3
```

The normal full-suite runner is
`node tests/scripts/run-tests.js --build-dir <configured-build> --config Release`.
ABI-only mode avoids production audio/curses/vendor dependencies without
changing production topology; it does not replace integration validation.

### Performance and allocation acceptance

Machine-readable results live in `tests/baselines/`. Compare equivalent CPU,
architecture, build configuration, and instrumentation; unlike environments
are rejected. Run timings on an otherwise idle host.

```sh
node tests/scripts/run-baseline.js --build-dir build/foundation-release --config Release --production-build-dir build/foundation-full-release --compare tests/baselines/macos-arm64-release.json --record build/foundation-measurements.json
node tests/scripts/run-baseline.js --build-dir build/foundation-release --config Release --production-build-dir build/foundation-full-release --timing-only --compare tests/baselines/macos-arm64-release-timing.json --record build/foundation-timings.json
```

The synthetic ABI fixture has 10,000 tracks, 128-byte titles, and 31 latency
samples. Production parity uses two indexed WAV tracks locally and over loopback,
seven latency samples, and duplicate-ID playlists with 10,000 entries; it is not
a representative 10,000-track remote metadata benchmark.

The macOS test-only dyld profiler counts dynamic malloc/calloc/realloc calls,
requested bytes, and out-of-line memcpy/memmove bytes across producer threads.
It cannot observe every inlined/system-library copy. Pointer-identity checks and
caller-thread C/C++ comparisons additionally protect zero-copy access and isolate
wrapper callback-state overhead. Instrumented and uninstrumented timings remain
separate.

- Equivalent C++ page submission permits at most four allocations/64 requested
  bytes above C, with no extra observed dynamic-copy bytes. The recorded sample
  adds two allocations/48 bytes. Page adoption permits one allocation (recorded
  32 bytes), with no copied metadata bytes; title-view access must allocate/copy
  nothing.
- The three-page cache has a fixed 512 KiB peak allowance and at most 64 KiB
  growth after warmup while scrolling all 100 fixture pages. Externally retained
  TrackViews can keep evicted pages alive; the budget covers cache-owned pages.
- Baseline comparison permits median ×1.5 + 50 microseconds, p95 ×2 + 100
  microseconds, and nonzero allocation/copy counters ×1.5 + 64. Zero counters
  stay zero. Missing measurements fail. Investigate repeated failures rather
  than silently replacing the baseline; UI timings are observations, not API
  microbenchmark thresholds.

## Task list

### Documentation and decisions

- [x] **DOC-001** Write the detailed public C API and C SDK design.
- [x] **DOC-002** Write the boundary-first unit and integration testing design.
- [x] **DOC-003** Document the static-first high-level architecture and
  migration sequence.
- [x] **DOC-004** Create this low-level design and trackable task plan.
- [x] **DEC-001** Retain the `mc_` public prefix and subsystem headers under
  `include/musikcore`. `musikcore.abi.inventory` and independent header builds
  verify the selected surface.
- [x] **DEC-002** Retain `musikcore-abi0` as the distinct experimental shared
  library name. `musikcore.abi.shared_c` and `musikcore.abi.exports` verify it.
- [x] **DEC-003** Decide whether initial multiple-context creation returns
  `MC_ERROR_BUSY` or supports isolated contexts.
  ABI0 returns `MC_ERROR_BUSY` while one context is active.
- [x] **DEC-004** Use one host-selected dispatcher per context.
  Request completions use callbacks. A separate unsolicited-notification polling
  surface remains planned and is not implemented by the current headers.
- [ ] **DEC-005** Decide whether public and provider ABI versions advance
  together through v1. Deferred to C provider design; the current public ABI
  remains experimental 0.3 and the existing C++ plugin SDK is version 23.
- [x] **DEC-006** Retain CTest, existing named native harnesses, shared C11
  diagnostics, and Node's test runner. ADR-0004 records why a new C++ framework
  is not necessary now; any future framework must be pinned and test-only.
- [ ] **DEC-007** Decide whether raw database access is removed from the
  proposed public surface or reserved for an explicitly unstable extension.
  The current consumer API exposes no database handle; do not add one merely
  to satisfy this unresolved design question.

#### Decision gate

All decisions that affect layouts, target names, and lifecycle semantics are
recorded before the experimental shared façade is introduced.

### Phase 0: testing foundation without changing linkage

- [x] **TST-001** Add `include(CTest)` and `BUILD_TESTING` handling to the
  Unix-like root build.
- [x] **TST-002** Add equivalent CTest registration to the Windows build path
  before its early return.
- [x] **TST-003** Create `tests/CMakeLists.txt` and standard CTest labels.
- [x] **TST-004** Add C11-compatible expression/file/line checks in
  `tests/support/check.h`, without choosing the caller's cleanup strategy.
  The `arguments` and `shared_c` contract consumers use the helper.
- [x] **TST-005** Add cross-platform temporary-directory support that never
  uses production user data.
  `tests/scripts/run-isolated-test.js` and the indexed fixture create private roots;
  `musikcore.abi.query`, `result_contracts`, and `indexed_query` exercise them.
- [x] **TST-006** Add deadline-based event/predicate waiting support with
  timeout diagnostics.
  `tests/support/host-wakeup`, `tests/support/ApiHost.h`, and query wait helpers
  drive `result_contracts`, `remote_query_transport`, and `indexed_query`.
- [x] **TST-007** Supply deterministic stream and output fixtures for the core
  tests. Generated WAVs, fake/null output, paced PCM capture, and its gated
  `IDataStream` exercise decoding, cancellation, and buffer/worker teardown in
  `musikcore.integration.transport.*_pcm` and `musikcore.integration.api.uri_player`.
  A general memory-provider fixture is deferred until a C SDK use case needs it.
- [x] **TST-008** Configure Debug instrumentation before any targets through
  `.cmake/ConfigureSanitizers.cmake`. `MUSIKCUBE_SANITIZERS` selects
  `address,undefined` (Unix Clang/GCC default), either one, or `none`; the test
  runner forwards `--sanitizers`. C/C++ compilation and executable/shared/module
  linking are covered, including ABI-only fixtures. Release remains plain.
  `musikcore.integration.binary.sanitizers.c` and `.cpp` verify actual fault
  detection and fatal UBSan behavior; both fail before the configuration fix.
  `sanitizer_reload` protects AppleClang 21 image-global cleanup and verifies
  that real global overflows remain diagnosed. Windows support is deferred.
- [x] **TST-009** Native unit/contract tests run on Windows, macOS, and Linux.
  The foundation gate records Windows Debug/Release 87/87 and the native Unix
  evidence; newer changes still require their own platform validation.
- [x] **TST-010** Document the standard local configure, build, and CTest
  commands.

#### Phase 0 gate

- [x] **GATE-000** CTest runs isolated tests on all three supported platforms.
  The initial Windows static-link constraint is superseded by the validated
  production DLL migration; native evidence is in the foundation gate.

### Phase 1: shared ABI foundation, still tested without a new DLL

- [x] **ABI-001** Create `include/musikcore/abi.h` with C11/C++ guards.
- [x] **ABI-002** Implement `MC_API`, `MC_PLUGIN_API`, and `MC_CALL` for all
  supported compilers.
- [x] **ABI-003** Define ABI major/minor encoding and runtime version helpers.
- [x] **ABI-004** Define fixed-width `mc_result`, error codes, booleans, and ABI
  enum conventions.
- [x] **ABI-005** Define `mc_string_view`, byte slices, and caller-buffer
  required-size semantics.
  Caller-buffer helpers are currently internal and tested by `musikcore.abi.header`;
  the installed API returns owned pages and borrowed views, not public copy helpers.
- [x] **ABI-006** Define sized-structure initialization and compatibility
  helpers.
- [x] **ABI-007** Define opaque-handle ownership and pointer-to-pointer destroy
  conventions.
- [x] **ABI-008** Implement core-owned error objects with allocation-failure
  fallback behavior.
  Async library errors name their operation and available failure reason;
  `musikcore.abi.header` and `result_contracts` cover diagnostics, ownership,
  and allocation fallback. Human-readable messages are not a parsing contract.
- [x] **ABI-009** Contain exceptions in allocating/dispatching entry points and
  audit scalar getters/destructors as nonthrowing. Share text and ID validation
  helpers. `arguments`, `shared_c`, and `dispatch` verify
  nullability, bounds, output preservation, and failure cleanup.
- [x] **ABI-010** Define common audio format and buffer-view types without
  implementing audio providers yet.
- [x] **ABI-011** Add C11 and C++ header compile tests.
- [x] **ABI-012** Add unit tests for strings, errors, versions, sized
  structures, and ownership helpers.
- [x] **ABI-013** Verify no public header exposes `long`, STL types, C++
  classes, bitfields, packed structures, or flexible arrays.

#### Phase 1 gate

- [x] **GATE-100** Shared ABI primitives, layouts, and ownership pass the
  foundation gate's named native tests. FND-WINDOWS closes the former Windows
  exception and includes production DLL/import-library validation.

### Phase 2: source manifest and experimental shared façade

- [ ] **BLD-001** Move the common core source manifest out of duplicated Unix
  and Windows lists into a shared CMake include.
- [x] **BLD-002** Remove the retired C boundary and keep consumer ABI sources
  separate through `.cmake/MusikcoreConsumerApi.cmake`. Source-manifest
  consolidation across production platforms remains BLD-001.
- [x] **BLD-003** Introduce the private PIC `musikcore_objects` target on Unix;
  production shared core and test-only `musikcore_internal` reuse its objects.
  `musikcore.integration.binary.exports.*` and the frontend import guards verify
  that reuse does not export or link private implementation into consumers.
- [x] **BLD-004** Retired: do not introduce `musikcore_legacy`. Windows and
  Unix production now export the public C ABI through a shared core.
- [x] **BLD-005** Retired: the intermediate application relink to
  `musikcore_legacy` is superseded by MIG-009/MIG-010 and their boundary tests.
- [x] **BLD-006** Add `musikcore_abi0` as a distinct shared target on all
  platforms.
- [x] **BLD-007** Ensure `musikcore_abi0` excludes legacy C boundary sources.
- [x] **BLD-008** Add Windows import/export annotations and import-library
  generation. The ABI0 shared target supplies the import library to
  `musikcore.abi.shared_c`. Native Debug and Release verification is BLD-014.
- [x] **BLD-009** Add ELF version script and macOS exported-symbols list.
- [x] **BLD-010** Add an export allowlist test that rejects private C++ symbols.
  `musikcore.abi.exports` checks the complete PE, ELF and Mach-O export lists.
  `musikcube.integration.windows_core_dll` checks production PE exports and
  both frontend import tables.
- [x] **BLD-011** Prevent one executable from linking multiple production,
  reduced ABI0, or private test-core implementations together.
  `.cmake/VerifyCoreBoundary.cmake` traverses target dependencies; the positive
  and negative `musikcore.abi.link_boundary` configure fixtures exercise it.
- [x] **BLD-012** Give ABI0 separate runtime output and plugin directories.
  `bin/abi0` and its empty reserved plugin directory are build-local. Standalone
  ABI0 currently does not load plugins; production fixtures stage selected plugins.
- [x] **BLD-013** Add a separately compiled C executable linked only to ABI0.
- [x] **BLD-014** Test Debug and Release import/shared-library behavior on
  Windows. `musikcore.abi.shared_c` loads ABI0 through its import library, and
  `musikcore.abi.exports` checks its PE export allowlist.

#### Phase 2 gate

- [ ] **GATE-200** A separate C process loads `musikcore_abi0` on Windows,
  macOS, and Linux, and actual exports match the allowlist. Production
  musikcube links its production shared core separately from ABI0.

### Phase 3: public runtime walking skeleton

- [x] **API-001** Create `include/musikcore/musikcore.h`.
- [x] **API-002** Implement `mc_abi_version` and requested-version validation.
  `musikcore.abi.header`, `shared_c`, and `arguments` exercise version behavior.
  A general capability-discovery API remains deferred; it is not implied by
  version reporting or service-mask selection.
- [x] **API-003** Define context options for ABI version validation,
  dispatch wakeups, query limits, and a copied provider-discovery root. Database
  directories belong to explicit library identities rather than context-global
  data roots. `shared_c` and `arguments` verify these contracts.
- [x] **API-004** Implement transactional `mc_context_create`.
- [x] **API-005** Implement context shutdown states and
  `mc_context_destroy`.
- [x] **API-006** Implement the request ID generator and registry.
- [x] **API-007** Implement owner-thread completion dispatch and wakeup
  integration, verified by `musikcore.abi.dispatch` and managed runtime tests.
  Unsolicited polling is tracked separately by API-014.
- [x] **API-008** Implement callback-scoped errors and owned page/result completion
  payloads, verified by `musikcore.abi.query` and `result_contracts`.
  Owned polled notifications are tracked separately by API-015.
- [x] **API-009** Implement cancellation and exactly-once terminal completion.
- [x] **API-010** Implement wakeup coalescing and dispatcher-thread shutdown draining.
- [x] **API-011** Reject unsupported second contexts according to DEC-003.
- [x] **API-012** Cover current C version, context, errors, callback delivery,
  cancellation, and teardown contracts in `shared_c`, `arguments`, and `dispatch`.
  Future polled-event tests belong with API-014/API-015.
- [x] **API-013** Instrument and validate direct ABI lifecycle/allocation-failure
  paths on native macOS with ASan/UBSan, including `dispatch`, `result_contracts`,
  `shared_c`, and `subscriptions`. Full Debug and ABI-only Debug suites pass;
  actual C/C++ compile and driver-link flags contain the selected sanitizers.
  TST-008's negative probes prevent a plain Debug build from silently claiming
  instrumentation. Native Linux/Windows acceptance for this change stays open.
- [ ] **API-014** Deferred expansion: unsolicited notification polling. Existing
  host-dispatched subsystem callbacks serve current application consumers.
- [ ] **API-015** Deferred expansion: owned polled notification payloads and
  their lifecycle tests; blocked by API-014.

#### Phase 3 gate

- [x] **GATE-300** Current create, dispatch/cancel, callback ownership, and
  shutdown contracts pass `shared_c`, `dispatch`, and `result_contracts` with
  native macOS ASan/UBSan instrumentation (API-013). Cross-platform acceptance
  remains in the foundation gate, where native Windows acceptance is complete;
  this result does not claim new Linux/Windows sanitizer execution. Polled
  notifications remain separately deferred under API-014/API-015.

### Phase 4: C plugin architecture replacement

#### Approved execution order

Replace the existing plugin architecture on this branch. The preserved
`clangen/musikcore-c-abi-completion` branch is the working fallback. There is one
production plugin path in the replacement engine: `PluginRegistry` discovers,
identifies, and initializes C plugins, owns accepted `Plugin` records for the
engine session, and indexes their features. Engine internals may use private C++ types,
but must not depend on the former C++ SDK interfaces. Do not introduce runtime
coexistence, C++ SDK compatibility adapters, or a second discovery list such as
`PluginFactory::CProviderFiles`.

The following six steps supersede the previous incremental migration route.
Existing AUD/STR/DCD/ENC/DST/TAG/IDX/OUT/DSP/EXT task IDs remain authoritative for
individual families; the cross-family tasks below coordinate their order.

The cutover dependency map below identifies current engine callers, relative to
`src/musikcore`. It is an implementation inventory, not permission to keep an
old-interface adapter. The interface-by-interface disposition is in the design.

| Caller group | Current dependency | Replacement work |
| --- | --- | --- |
| `runtime/ManagedRuntime`, `plugin/Plugins`, `plugin/PluginFactory` | Old discovery, process registry, session flags, and `Get*`/`Set*` hooks | Retain explicit context directory/service selection; use only `PluginRegistry`, then remove old discovery and global service setters. |
| `support/Preferences`, plugin catalog/settings C entry points | `IPreferences`, `ISchema`, plugin-name stores, reload calls | Use scoped preference/configuration tables. Decide persistence identity before changing filenames; render data descriptors without plugin-owned UI. |
| `io/DataStreamFactory`, `io/LocalFileStream`, `audio/Streams`, `audio/Stream` | `IDataStream`, decoder/encoder factories, DSP enumeration and caches | Use C stream/factory contracts and engine-owned instances. Preserve create-before-open publication and interruption; absent families report unsupported or bypass DSP. |
| `audio/Buffer`, `audio/Player`, `audio/Outputs`, transport headers, `audio/Crossfader` | SDK buffer/provider/output types and callbacks | Use opaque host buffers, typed output factories, explicit submission tokens, and core-owned playback state. Discover the bundled null-output DLL through the same loader as native outputs. |
| `audio/PlaybackService`, `audio/Visualizer` | Playback-remotes, visualizer interfaces, borrowed playback-service pointers | Deliver owner-thread C snapshots and bounded read-only PCM/spectrum views. Active platform integrations become explicitly stopped/joined extensions. |
| `library/Indexer` | Tag readers, decoder factories, indexer sources, analyzers, SDK metadata writers | Use source-scoped C writers and typed factories. Keep checkpoint/rollback semantics; resolve source-ID and timestamp persistence before conversion. |
| `library/ILibrary`, `library/LocalMetadataProxy`, `library/track/*`, `library/metadata/*` | SDK track/list/map/editor interfaces | Keep concrete metadata storage private to the core; expose existing C identities/results/pages through typed host calls. Do not recreate the exported C++ object graph. |
| `library/query/*`, including `util/SdkWrappers` and `TrackQueryFragments` | SDK metadata wrappers and shared tag/category constants | Replace old interface results with private core data or existing C result types. Preserve typed backend routing and bound SQL; keep SQL in local query implementations. |
| `support/Common`, `support/Playback`, `support/LastFm`, `support/Auddio`, `i18n/Locale`, `net/WebSocketClient`, `ffi/consumer/remote-session`, `pch.hpp` | SDK filesystem, strings, HTTP helpers, constants, and transitive includes | Extract reusable implementation helpers into private core/common headers. These are not new C ABI families. Remove transitive SDK includes after direct callers move. |

`runtime.stream_sdk_contract` currently still exercises the old factory and
stream path. Independent C discovery can probe a rejected old image again; its
fixture asserts one old object released before unload, not a fixed count of
native-image probes. Remove that old-only fixture under PLG-BACKFILL-002 when
replacement stream tests protect its useful create/open/cancel contracts.

1. **Draft all contracts before implementing families.**
   - [ ] **PLG-CONTRACT-001** Inventory every existing SDK interface/export and
     engine caller. Map each to a C provider table, a narrow C host service, an
     existing public C API operation, an internal-only helper, or deliberate
     removal. Document differences from the C++ behavior and affected callers.
     Draft interface mapping is in the design's planned family contracts; caller
     cutover/dependency inventory remains open.
   - [ ] **PLG-CONTRACT-002** Define provisional C shapes for buffers, streams,
     decoders, blocking/streaming encoders, outputs/devices, DSP, analyzers,
     metadata writers/readers, indexer sources/writers, visualizers, playback
     observers, configuration, and lifecycle extensions. Reuse existing C
     primitives/results where appropriate. Complete the contract-definition
     portions of AUD-002, STR-001, DCD-001/DCD-002, ENC-001/ENC-002, DST-001,
     TAG-001, IDX-001, OUT-001/OUT-002, DSP-001, and EXT-001 through EXT-005
     here; their integrations remain open. The design sketches family
     structures/vtables/factories and scoped services, including typed
     catalog/playback host calls. Audio/output declarations are materialized in
     `include/musikcore/plugin/audio.h` and `output.h`; materialize the remaining
     headers/defaults and reconcile real callers before closing this task.
     Existing `SendRawQuery` carries
     named-query JSON through QueryRegistry, not SQL. Preserve that protocol
     where needed; do not add SQL-text execution/database-handle entry points.
   - [ ] **PLG-CONTRACT-003** Review each shape for ownership, allocation/free
     pairing, units, input/output direction, EOF/backpressure, interruption,
     thread affinity, real-time restrictions, and shutdown. Plan standalone
     C11/C++ header/default-initializer checks (`plugins.contract_headers`).
     Shapes remain experimental and may change after representative use; this
     is not an ABI freeze or a mechanical port of C++ inheritance. Review the
     existing C++ hash-based source IDs, file-clock timestamps, and preference
     store identity before any persisted-data conversion. No automatic reindex,
     data rewrite, or legacy compatibility shim is authorized by this draft.
2. **Replace each family's engine path with the new loading contract.**
   Maintainer-approved staging: integrate C outputs first while keeping existing
   decoders, streams, DSP, and other families usable. No output compatibility
   interface or fallback is retained. Full old-SDK removal remains open until
   those families migrate; this supersedes the initial all-family disconnection.
   - [ ] **PLG-CUTOVER-001** Remove the old discovery/startup path, factory
     lookups/caches, `Get*`/`Set*` hook wiring, and engine dependencies on
     `sdk::I*` interfaces. Simplify private types as callers move; engine-owned
     types need not reproduce old interface/provider layers, particularly for
     audio-buffer storage. Old plugin implementations may remain as disconnected
     conversion references, outside active targets/installation. Add a source/
     binary boundary check (`plugins.engine_boundary`) so they cannot be
     reintroduced accidentally. The current names are PluginRegistry/Plugin;
     C discovery is now owned by the registry, but the old engine interfaces still need
     removal. Inventory capabilities lost until restored;
     document explicit C API unsupported outcomes during the rebuild.
   - [ ] **PLG-LOADER-001** Consolidate loading and engine-session ownership in
     `PluginRegistry`. It discovers candidates, validates their C entry tables,
     initializes them, and publishes capabilities transactionally. `Plugin` owns
     one library's tables/state. Remove `CProviderFiles` and all dependence
     on `PluginFactory`. Catalog disabled plugins using copied metadata without
     initializing their engine resources. Preserve existing lifecycle, duplicate-image/ID,
     rollback, owner-thread, and installed-C fixture regressions; adapt lifetime
     checks to session ownership and worker quiescence before shutdown;
     add direct discovery coverage (`plugins.discovery`).
     - [x] Keep native identification/session state in `Plugin`; consolidate
       discovery, loading, and accepted ownership in `PluginRegistry`.
       `CProviderFiles` is removed. `plugins.lifecycle`, `plugins.discovery`,
       `plugins.runtime`, and `plugins.installed` cover direct/installed loading,
       disabled metadata, duplicate images/IDs, rollback, wrong-thread calls,
       broken candidate links, and context-specific discovery directories on macOS.
     - [ ] Add typed lookup for the remaining family tables as their headers and
       concrete callers are implemented. Remove the old `PluginFactory` production
       path under PLG-CUTOVER-001; independent C discovery is not engine cutover.
   - [ ] **PLG-LOADER-002** Ensure failed discovery/negotiation/initialization
     leaves no visible registrations and does not prevent other plugins loading.
     Make dependency injection explicit; no global `SetEnvironment` replacement
     that exposes the entire engine as one untyped service object. Supply typed
     services at instance creation, with active extensions started only after
     required services are ready. Prove stop/cancel/owner-dispatch-drain/join/
     instance-destruction ordering before final registry shutdown/unload, including
     workers holding peer-provider objects and awaiting host completions.
3. **Make empty optional families valid.**
   - [ ] **PLG-EMPTY-001** Support no DSP, encoders, analyzers, visualizers,
     playback observers, or platform integrations. DSP bypasses unchanged PCM;
     encoding reports unsupported; absent observers receive no events. Starting,
     querying, idling, and stopping the engine must not dereference missing
     providers (`plugins.empty_registry`). Missing decoders/streams report
     unsupported input; they must not pretend to read or decode successfully.
   - [ ] **PLG-EMPTY-002** Establish applicable test sets during the rebuild.
     Keep core/query/runtime/loader tests meaningful and green; explicitly record
     which playback/indexing/UI scenarios require families not restored yet.
     Never silently turn required regression tests into passing no-ops. Restore
     their normal registration as each prerequisite family becomes available.
4. **Provide real null implementations for required defaults.**
   - [x] **PLG-NULL-001** Supply `nullout` as a loadable C output plugin.
     Prove pacing, pause/resume, paused-delivery backpressure, exactly-once return,
     unavailable timing, stop/drain, and unload (`plugins.null_output`). An empty
     plugin directory leaves queries/startup/idle usable; playback without an
     output must report unavailable. Do not add a second built-in null sink.
     - [x] Replace the `nullout` target with a C-only DLL exporting
       `mc_plugin_get_api`. `musikcore.integration.plugins.null_output` loads the
       real DLL through PluginRegistry and tests pacing, pause/resume, oversized
       input versus backpressure, synchronous caller-thread completion, failed
       prepare, concurrent stop/write cancellation, destruction, and native unload.
       NullOutput owns no worker or pending audio; only an interruptible producer
       wait and callback-quiescence flag remain. No injected clock or timing estimator.
       `binary.exports.nullout` verifies the sole entry point on macOS.
     - [x] **PLG-CPP-OUTPUT-001** Add the strictly header-only `plugin/output.hpp`
       adapter. `RegisterOutput<T>` generates C tables without inheritance, core
       linkage, or lifecycle policy. Require noexcept queue/stop, translate
       ordinary exceptions, and default optional timing to unknown.
       `plugins.output_adapter` verifies forwarding, construction failure,
       exception containment, and absence of implicit Stop/rollback. The real
       DLL test supplies explicit host stop/cleanup. `plugins.installed` builds
       and loads C/C++ plugins from relocated public headers and audits symbols.
       Extend helpers as other concrete C families are implemented.
     - [x] **PLG-OUTPUT-HOST-001** Establish and test preconditions in the actual
       replacement engine's output call path before integration. Accept one
       typed playback buffer and completion owner; construct C prefixes, format,
       and storage identity internally. Check changing sample counts, storage
       bounds, and format values once before calling the plugin. Token identities
       and required callbacks are host-generated; validate normalized volume
       when it is set, not per buffer.
       Test a downstream recording provider with the production host path so
       invalid inputs produce no calls; a mock caller is not enforcement proof.
       Test successful prepare before queue, Stop before reprepare, cleanup of
       failed prepare, producer/callback quiescence before destroy/unload, and
       failed Stop preventing unsafe destruction. Boundary input checks and
       lifecycle-order assertions run through production audio::Output in
       `plugins.output_host`. Recording-DLL regressions cover inline and deferred
       completion, shared-player ownership, bounded admission, prepare rollback,
       partial-create cleanup, interruption of a blocked producer, and stop/restart.
       Fatal child tests `plugins.output_stop_failure`,
       `plugins.output_duplicate_completion`, and `plugins.output_stale_completion`
       prevent unsafe reuse or destruction after broken plugin contracts.
     - [x] Connect engine output selection and plugin catalog/settings to the C
       factory. Saved device preferences, no-schema settings, plugin enablement,
       reload, and missing-output behavior use the actual engine path. Restore
       the playback/UI and installed-consumer scenarios listed below.
   - [ ] **PLG-NULL-002** Decide any other required defaults from actual engine
     callers. Supply an implementation only where it has truthful behavior;
     otherwise report unsupported. Document whether the implementation is
     core-owned or dynamically loaded, using the same typed C contract either way.
5. **Prove one representative of every supported family.**
   - [ ] **PLG-PROVE-001** First complete AUD/STR/DCD and PERF-002 below using
     deterministic media, a real decoder path, and `NullOutput`. Prove known PCM,
     seeking, final-buffer/EOF behavior, cancellation before/during open, buffer
     ownership, and shutdown/unload through the public application C API.
   - [ ] **PLG-PROVE-002** Then select and record one representative for each
     remaining family: stream source, encoder modes, tag reader, indexer source,
     native output, DSP, analyzer, visualizer, playback observer, configuration,
     and lifecycle/server integration. Use an in-tree production plugin where
     practical or a useful deterministic fixture where the platform/plugin is
     unavailable. Reenable dependent app tests as functionality returns. Each
     family needs boundary/failure/lifetime tests before it is considered proven.
6. **Backfill remaining plugins and remove the disconnected code.**
   - [ ] **PLG-BACKFILL-001** Complete remaining conversion tasks in Phase 8
     against the proven contracts. Remove each old implementation once replaced.
     Keep native Linux/Windows implementation/validation work explicitly assigned
     to their platform sessions; macOS evidence does not close those tasks.
   - [ ] **PLG-BACKFILL-002** Remove old SDK headers, factories, adapters, exports,
     targets, install rules, and old-only fixtures after their replacements have
     meaningful regression coverage. Complete GATE-800, external plugin/archive
     checks, and restored app workflows before release or ABI freeze.

#### Output integration mechanics

Each transport owns its joinable track jobs ([ADR-0009: Playback threading model](../docs/adr/0009-playback-threading-model.md)).
The track worker opens and reads the decoder, applies gain and owns the fixed PCM
pool. The transport reads duration and capabilities from snapshots. Closing the
session interrupts active, prepared and retired jobs before joining any one of
them. Independent URI sessions remain usable while a peer is stalled.

`audio::Output::Writer` binds a track to one output. It records which slots
contain that track's buffers. Native callbacks claim the matching token, publish
the outcome and wake the worker. The worker recycles its own slots and handles
FFT and notifications. Revoking a writer prevents new submissions, seeks, drains
and failure cleanup from affecting a replacement track. Accepted slots must
still be returned. Same-format gapless tracks share the device queue. An output
that returns queue-full must wake its producer when capacity or a terminal
error becomes observable, even if it already returned all outstanding buffers.

Ordinary backend operations are serialized. Stop can interrupt a blocked write
or drain independently. Failed prepare and partially created instances are
stopped before reuse or destruction. Service teardown detaches plugin hosts
while playback/catalogs exist, then releases outputs before DLL unload. Reentry
is checked before service mutation. Cleanup failures that prevent quiescence are
fatal.

CoreAudio allocates AudioQueue storage during prepare and copies PCM on producer
workers. Callbacks allocate/free no storage and take no engine locks. Synchronous
disposal quiesces callbacks before returning outstanding cancelled tokens. Drain
requests AudioQueueStop(false) and waits for both accepted frames to complete
and IsRunning to become false; buffer reuse alone does not prove audible playback
has finished. Timing remains explicitly unknown until a meaningful backend
measurement is available.

#### Replacement foundation validation

Loader, buffer, and output checks exercise the actual C boundaries. The null
output test uses the production shared library, not a copy of its implementation
linked into the test host. Native Linux/Windows checks remain SDK-LINUX/SDK-WINDOWS.
Full engine cutover, remaining family headers, and the first C decoder playback gate
remain open. Passing currently applicable checks does not close these gates.

The output integration restores the previously deferred plugin playback/reload,
managed runtime, playback-without-indexer, remote audio/transcoding, URI player,
and TUI settings/lyrics scenarios. Installed SDK consumers again exercise managed
plugin catalogs and C/C++ URI players. None remain disabled for output cutover.

`transport.playback_pcm` scenarios cover real engine PCM handoff, gapless and
crossfade transitions, seek, held completions, paused teardown, retired short-track
cancellation, and queue/callback failure. Terminal transport Destroy cancels
retired sessions and joins workers before plugin unload, even while a caller
retains the transport. Failure regressions require exactly one OpenFailed and
no natural Finished/automatic-next event. Throwing host detachment terminates
before later hooks or native unload (`runtime.failure`).

The macOS gate `plugins.coreaudio_output` uses the physical
AudioQueue with silent buffers, device UID selection, immediate/repeated drain,
pause/stop/restart, and format change. It also drives a generated silent WAV
through the public context/URI-player API and the existing FFmpeg decoder,
including live runtime shutdown. Only actual device absence permits a skip;
operational errors fail. Device unplug/replacement and other native platforms
remain unverified; these runs do not close SDK-LINUX/SDK-WINDOWS.

The remaining C++ SDK environment vtable now exposes output configuration
snapshots and selection instead of output objects. Its version advances from
23 to 24 and remaining plugins must rebuild; no compatibility shim is added.
The public application C ABI is unchanged. This intentional autonomous SDK
change requires maintainer acknowledgment before further family migration.

Public C/C++ headers compile independently. The relocated installed-SDK check
builds and loads C and C++ plugins without core linkage and audits their symbols.
The NullOutput DLL exports only mc_plugin_get_api and depends only on the system
C/C++ runtime. Adapter compile checks reject invalid noexcept/result signatures
and mutable timing methods; adapters own no implicit lifecycle policy.

#### Existing bootstrap foundation (retain and adapt)

- [x] **SDK-001** Create `include/musikcore/plugin.h`. Independent C11/C++17
  translation units compile it in `musikcore_public_headers`; it remains separate
  from the application umbrella header.
- [x] **SDK-002** Define the single `mc_plugin_get_api` entry point. Fixture and
  external-provider export audits enforce that surface; C providers use host
  callbacks without importing core symbols.
- [x] **SDK-003** Define sized `mc_host_api` and `mc_plugin_api` tables with
  documented fields, byte limits, and default initializers. Initialize receives
  the host table and returns opaque per-session state; shutdown owns its cleanup,
  including partial/null state after failure. `plugins.lifecycle` verifies
  independent sessions sharing one mapped image.
- [x] **SDK-004** Implement bootstrap logging and host-owned error creation and
  destruction. `plugins.lifecycle` verifies copied diagnostics survive mutation
  of their source and native unload, and contains throwing log sinks.
- [x] **SDK-005** Implement ABI negotiation and required-table validation.
  `plugins.lifecycle` rejects incompatible/newer/zero versions, truncated tables,
  missing hooks, invalid identifiers/UTF-8, and malformed registration sizes.
  The zero-version regression failed before the validation fix.
- [x] **SDK-006** Implement plugin records and lifecycle states. Negotiation,
  initializing, running, and stopped states separate publication from
  session teardown. `plugins.lifecycle` checks rejection before lifecycle entry,
  rollback after failed initialization, and one-shot shutdown/unload.
- [x] **SDK-007** Implement capability/factory registration lifetime rules.
  Registration copies temporary descriptors/IDs, rejects duplicates and wrong
  phases/threads, and publishes only after successful initialization. Registered
  typed tables remain provider-owned. `Find<T>(pluginId)` maps supported C table
  types to their IDs internally; unsupported types fail at compile time.
  No public string capability lookup, unchecked pointer constructor, or friends.
  `plugins.null_output` checks typed calls, callback self-reset, independent nested
  sessions, empty views, and retained passive views through shutdown;
  concrete decoder/stream instance contracts remain Phase 6 work.
- [x] **SDK-008** Keep loaded plugins uniquely owned for the engine session.
  `PluginCallState` detects owner-thread reentry per session, independently of
  exception policy; it neither counts workers nor coordinates teardown.
  `plugins.lifecycle` verifies nested/thread-isolated guards, unwinding,
  and owner-thread cleanup; `plugins.null_output` checks harmless empty/stopped
  destruction inside a real factory call. The real
  `plugins.exception_entry`/`exception_initialize` DLLs verify contextual errors
  and replacement of pre-existing error slots; `plugins.exception_shutdown`
  terminates in a child while asserting the DLL remains mapped. Host factory
  callback exceptions propagate, verified by `plugins.null_output`.
  Typed factory views are non-owning; no lease counters, retirement phase,
  shared worker accounting, or blocking unload waits. `plugins.lifecycle` checks that
  registry ownership leaves a running DLL mapped and native unload occurs on
  the owner. `plugins.null_output` covers passive retained views and completion
  of output workers/instances before teardown. Wrong-thread lifecycle calls
  return a distinct error without mutation.
  `plugins.reentry_*` subprocesses verify fatal same-session reentry through
  factory calls, selection, diagnostics, initialization, and shutdown; termination
  occurs with the DLL still mapped, including Release builds.
  The whole-engine teardown entry must enforce the same fatal reentry invariant
  before destroying services; the private registry check cannot protect earlier
  engine mutation. Public context dispatch/BUSY behavior is unchanged.
  GATE-600 covers stream and decoder quiescence. Full engine orchestration
  and extension teardown remain PLG-CUTOVER-001 and the extension task below.
- [x] **SDK-009** Add the valid pure-C fixture plugin. Its test-only probe table
  exercises session-owned state without inventing a shipped provider family.
- [x] **SDK-010** Add incompatible, truncated, missing-hook/version/entry,
  initialization-failure, malformed-capability, and duplicate-ID fixtures.
  `plugins.lifecycle` verifies failure cleanup without publishing partial state.
- [x] **SDK-011** Add contract tests for discovery, negotiation, initialization,
  shutdown, and unload: `musikcore.integration.plugins.lifecycle`,
  `musikcore.integration.plugins.runtime`, and
  `musikcore.integration.plugins.installed`. The runtime case repeats plugins-only
  context creation through the shared public C API, keeps a valid provider usable
  alongside rejected negotiation and failed initialization, and observes rollback
  diagnostics through normal owner-thread wakeup/dispatch. The original bootstrap
  used `PluginFactory::CProviderFiles` for coexistence discovery; that wiring is
  superseded by PLG-CUTOVER-001/PLG-LOADER-001. The old
  `runtime.stream_sdk_contract` test remains evidence for that implementation,
  not a requirement to preserve SDK 23 at runtime.
- [x] **SDK-012** Verify fixture plugins include only installed C headers and do
  not link private musikcore targets. `plugins.installed` builds a separate C-only
  project against a relocated SDK, checks exports/imports, and runs it through the
  same lifecycle/drain tests from a path containing spaces.

The per-session-state regression failed with the initial global-host fixture:
closing one host affected the other's callbacks. Returning state from initialize
removes that coupling without changing the application runtime's existing
process-wide ownership policy. See
[ADR-0008](../docs/adr/0008-c-provider-bootstrap-and-unload-lifetime.md).

The missing-entry fixture has no instrumented globals. It exposed a link failure
in the AppleClang 21 unload helper, which now supplies a hidden weak zero flag
that coalesces with compiler-generated registration state when present.
`musikcore.integration.binary.sanitizer_reload` and the provider lifecycle case
pass with this empty-module path under ASan/UBSan.

#### Bootstrap gate (previous implementation evidence)

GATE-400 records the tested bootstrap before the clean replacement. It does not
mark the new loader, engine disconnection, all-family contracts, or null/default
behavior complete. Those remain the unchecked tasks above.

- [x] **GATE-400** The shared production runtime loads a separately built pure-C
  provider. Focused host tests observe its registered capability, release retained
  references, drain admitted calls, invoke shutdown once, and verify native unload.
  Native macOS arm64 Release passes 126/126 and Debug ASan/UBSan passes 131/131.
  Final focused header/inventory/provider/SDK-regression runs pass 6/6 Release
  and 7/7 Debug after the last fixture validation changes. Rebuilt/extracted
  app startup/search/queue/shutdown passes three times; daemon lifecycle and
  relocated SDK C/C++ query/URI players plus Rust tests/five doctests pass again
  with the provider host compiled into the runtime.
  Scope exception: the maintainer has deferred fresh Linux/Windows parity runs
  while working from macOS. Those are the explicit follow-up tasks below, not
  passing platform results. This completes the bootstrap scope; typed providers,
  application-facing C-provider catalog/settings integration, and decoder migration
  are not claimed complete.
- [ ] **SDK-LINUX** Run lifecycle, installed-provider/export checks, shared-runtime
  restart, and ASan/UBSan on native Linux. Verify ELF visibility, loader search
  paths, and actual `dlclose` after workers/instances finish, including passive
  capability views that must not delay unload.
- [ ] **SDK-WINDOWS** Run the same provider checks on native Windows. Verify cdecl
  function tables, DLL exports/imports/search paths, and UTF-8/Unicode filenames.
  The intentionally missing-entry fixture must not advertise the provider export.

### Phase 5: metadata/query public vertical slice

- [x] **LIB-001** Define query options with copied field and filter inputs.
  `musikcore.abi.query` mutates both after submission and verifies the original inputs.
- [ ] **LIB-002** Deferred optimization: replace immutable page-owned vectors
  with a packed arena if allocation measurements justify it. Current
  `ProjectedTrackPage` storage and stable string views pass `result_contracts`
  and the recorded allocation/cache budgets; immutable page ownership is done.
- [x] **LIB-003** Implement row identity, projected string/multivalue, missing,
  and explicit-empty accessors, verified by `musikcore.abi.result_contracts`.
  Search results expose numeric album durations; generic numeric metadata
  accessors are not part of the current consumer surface and remain deferred.
- [x] **LIB-004** Implement async query submission and per-request completion callbacks.
- [x] **LIB-005** Implement owned page transfer to the completion callback.
- [x] **LIB-006** Define track identity/list handles without eager metadata.
  Exercised by `musikcore.abi.query` and `musikcore.abi.indexed_query`.
- [x] **LIB-007** Implement async visible-range/cache-window requests.
  Exercised by the same ABI tests, including removal during paging.
- [ ] **LIB-008** Deferred expansion: general track-metadata state/invalidation
  events. Selection, playlists, indexer, and playback already have callbacks;
  published pages stay immutable and later reads can observe current metadata.
- [ ] **LIB-009** Deferred expansion: atomic batch track-list edits. Current
  ordered queue edits and playlist writes are implemented under PUB-005.
- [x] **LIB-010** Cover current paging, ordering, Unicode, long strings,
  immutable pages, retained results, missing rows, and cancellation in
  `musikcore.abi.query`, `result_contracts`, and `library_switch`.
  A general invalidation-event contract must be tested when LIB-008 is added.
- [x] **LIB-011** Add SQLite integration tests with isolated databases.
  `musikcore.abi.query`, `musikcore.abi.indexed_query`, and
  `musikcore.abi.library_switch` use fixtures under the build tree.
- [x] **LIB-012** Exercise pending reads, viewport changes, invalidation, and
  stale completions in `musikcore.integration.api.production.result_windows`.
  `tests/integration/performance/core-api.cpp` separately measures bounded
  three-page cache growth over 100 pages through `run-baseline.js`.
- [x] **LIB-013** Move dispatch and shutdown to one app-owned context.
  Add an owning C++ wrapper around the library handle and bind search
  results and track references to the originating library. Test switching
  between libraries with a request in flight in `musikcore.abi.library_switch`.
- [x] **LIB-014** Dispatch local and remote ordered search/range reads through
  typed library queries. Modern remote batch projections retain arbitrary and
  multivalued tags and NULL/empty distinctions; legacy-server fallback limitations
  are explicit. `musikcore.abi.remote_query_transport` and
  `musikcore.integration.api.backend_parity` cover routing and projection parity.
- [x] **LIB-015** Hold socket replies across cancellation, library selection,
  disconnect, and reconnect in `musikcore.abi.remote_query_transport`. Verify
  exactly-once completion and retained origin. Stale viewport delivery is covered
  separately by `musikcore.integration.api.production.result_windows`; this is
  not a claim of a single combined remote-TUI fault-injection scenario.
- [x] **LIB-016** Preserve originating-library validation and result order in
  queue/result commands. `musikcore.abi.remote_query_transport` checks remote
  result insertion and stale origins; production `playback_commands` checks
  play-from-result. `remote_audio` and `remote_transcode.*` separately exercise
  actual remote media streaming through the production server and decoder.
- [x] **CPP-001** Supply owning public-only Context, Library, TrackPage,
  TrackResult, and TrackRef wrappers plus async request submission/cancellation.
  `public_headers`, `result_contracts`, and `installed_sdk` validate these
  consumers. Requests use IDs; owned polled-event wrappers await API-014/API-015.
- [x] **CPP-002** Add C++ wrapper tests that use no private core headers.
- [x] **MIG-001** Select one musikcube search/list vertical slice for the pilot.
  Start with the track-search results view in `TrackSearchLayout`.
- [x] **MIG-002** Migrate that slice through the C++ wrapper in an ABI0-only
  migration executable. Exercised by `node tests/ui/scenarios/track-search.js --compare`.
- [x] **PERF-001** Add baseline and ABI0 benchmarks for query, initial display,
  and rapid scrolling. The headless Track Search benchmark measures
  keystroke-to-render latency and records the mainline baseline in the
  [testing design](testing-design.md#headless-track-search).

#### Track Search migration

Production `TrackSearchLayout` uses the public context and the same
`TrackResultWindow` for local and remote search. An owned result freezes row
order and album boundaries; visible-range reads preserve positions when rows
are removed. Generation checks discard obsolete callbacks, and bounded cached
pages retain metadata only for the working viewport. Retained TrackViews can
extend individual page lifetimes explicitly.

Playback and playlist actions use origin-bound results/references through the
public API. The earlier private backend bindings and legacy action-list
materialization are no longer required by Cube. MIG-009/MIG-010 and the frontend
boundary checks cover removal of private core dependencies.

`result_windows`, `result_contracts`, `remote_query_transport`, and the search
TUI scenario cover their respective seams. PERF-001 and the stored allocation
baselines distinguish API/cache budgets from observed keystroke-to-render timing.
This completed pilot is not a second migration backlog.

#### Phase 5 gate

- [x] **GATE-500** The pilot is superseded by production public-ABI Track Search.
  Page lifetime/cache/window tests and recorded PERF-001 budgets pass in the
  accepted foundation scope. No private-core frontend dependency remains.
- [x] **GATE-510** Production local and remote Track Search share the wrapper;
  library-switch and loopback contracts cover origin/session changes. Native
  Windows application acceptance remains GATE-700/FND-WINDOWS.
  [Remote library routing proposal](musikcore-remote-library-proposal.md)

### Phase 6: decoder SDK vertical slice

[ADR-0010](../docs/adr/0010-shared-media-services-for-playback-and-transcoding.md)
records why playback, server transcoding, and nested sources share one media
service. Keep the legacy C++ SDK version at 24 until the C migration is complete.
Build the host and its C++ plugins from the same revision. The environment and
encoder interfaces changed, so a matching version alone cannot establish binary
compatibility between migration builds.

- [ ] **AUD-001** Implement decoder scratch storage and fixed playback-pool
  handoff/recycling.
  - [x] Implement opaque growable `mc_audio_scratch_buffer` storage through
    `mc_audio_scratch_buffer_api`, reserve/format changes, exclusive
    `begin_write`/`end_write`, and read-only PCM publication.
    `musikcore.unit.plugins.audio_buffers` checks frame/sample units, overflow,
    failed/abandoned writes, nested-access rejection, copied-view isolation, and
    same-channel capacity reuse, and no-op/failed reserve preservation.
    `musikcore.unit.plugins.contract_headers` and
    independent C11/C++17 translation units cover audio/output declarations.
  - [ ] Connect decoder scratch to a private, contiguous fixed playback pool.
    Split decoded blocks across slices without growing playback storage; test
    pool exhaustion/backpressure, exactly-once recycling after output completion,
    and quiescence before pool replacement on format changes. The C output
    handoff is covered by OUT-002. C scratch now feeds the fixed playback
    pool (`transport.decoder_contract_pcm`, `transport.gapless_pcm`, and
    `transport.completion_pcm`). A change of channel count or sample rate
    during one track ends playback with an error. Retain the existing
    fixed-format playback pool for this migration. Mid-track pool replacement
    is deferred.
- [x] **AUD-002** Define frame counts, capacities, formats, and buffer state
  transitions precisely in `plugin/audio.h` and `plugin/output.h`.
  `plugins.audio_buffers`, `plugins.contract_headers`, and `plugins.null_output`
  verify the implemented contracts on macOS, including host-sized returned
  format prefixes and permanent oversized-submission errors versus backpressure.
  Native-platform follow-ups remain SDK-LINUX/SDK-WINDOWS. The production
  decoder-to-output path is exercised by GATE-600; mid-track pool replacement
  remains AUD-001.
  PCM is fixed interleaved native float32, with typed `float*` writable views;
  no encoding selector or per-block sample-format checks. The experimental
  `mc_audio_format` layout is 16 bytes; consumers of these structs must rebuild.
  Hosts supply validated values and negotiated prefixes; plugins handle backend
  capacity/availability. `plugins.null_output` tests unavailable-device recovery
  with explicit host cleanup; production engine input enforcement is covered
  by PLG-OUTPUT-HOST-001. Bootstrap
  checks are centralized in `mc_plugin_validate_host_api`, covered by strict
  `plugins.contract_headers` compatibility/prefix/default tests.
- [x] **STR-001** Define stream table operations with `uint64_t` positions,
  partial reads, EOF, seek, and interruption. Exercised on macOS by
  `musikcore.unit.plugins.local_file_source`, including offsets above 4 GiB.
- [x] **STR-002** Implement core-provided and plugin-provided C streams, with
  engine-owned wrappers as needed; do not reintroduce `sdk::IDataStream`.
  The local file, HTTP and subtrack providers use the same C stream
  contract. Playback and server transcoding obtain these streams through
  `mc_media_host_api`. See `runtime.stream_sdk_contract` and `api.uri_player` on macOS.
- [x] **DCD-001** Define decoder factory capability advertisement.
  `musikcore.integration.plugins.lifecycle` rejects incomplete decoder and
  stream registrations. `musikcore.integration.plugins.decoder_stream` loads
  a registered C decoder by type.
- [x] **DCD-002** Define decoder open, decode, seek, duration, preferred rate,
  exhaustion, and destroy operations. `musikcore.integration.plugins.decoder_stream`
  exercises the C contract with real file bytes and host scratch on macOS.
- [x] **DCD-003** Connect C decoder instances to playback through engine-owned
  types, removing `sdk::IDecoder` dependencies rather than adapting the old SDK.
  `transport.decoder_contract_pcm` runs a C-only decoder in a public URI player;
  the FFmpeg gapless, seek and crossfade tests run through the same host.
- [x] **DCD-004** Implement the pure-C deterministic decoder fixture.
  `musikcore.integration.plugins.decoder_stream` verifies sample values across
  a partial final block, EOF, seek, cancellation, and destruction before unload.
- [x] **DCD-005** Add controlled decoder failure modes selected by fixture URI.
  `transport.decoder_contract_pcm` rejects a successful empty block and a
  read error during playback, and distinguishes both from a healthy EOF with final frames.
- [x] **DCD-006** Add SDK contract tests for stream and decoder edge cases.
  `plugins.local_file_source`, `plugins.decoder_stream`, and `api.uri_player`
  cover large offsets, short reads, live streams, seeks, and interruption on macOS.
- [x] **DCD-007** Run the deterministic decoder through the real playback
  pipeline and fake output. `transport.decoder_contract_pcm` compares each
  captured sample, including the final partial block.
- [ ] **DCD-SEEK** Verify the first post-seek PCM timestamp using a packet-based
  fixture with non-frame-aligned seeks. FFmpeg currently reports the requested
  logical playback position after the demuxer accepts it. Before promising
  sample-accurate seeks, specify whether to decode and discard a partial packet,
  what to report for streams with missing timestamps, and the cost for HTTP.
- [x] **DCD-008** Add malformed provider results and unload-race tests.
  `transport.decoder_contract_pcm` rejects a decoder that stops publishing
  after one valid block, verifies the previous samples cannot replay, and shuts
  the managed runtime down while a C decoder is inside its input's blocked read.
  The source must be interrupted and destroyed before shutdown returns.
  `plugins.partial_media_create` requires an abort at the factory boundary if
  a failed stream creation leaves an instance in the output slot.
  `transport.decoder_drain_pcm` interrupts a blocked decode with a pending device
  completion and checks that the requested drain plays out the accepted audio.
- [x] **PERF-002** Verify the ABI introduces no PCM copy and no per-sample call.
  `plugins.ffmpeg_resample` counts one scratch write per nonempty output block,
  checks the published buffer address and verifies all 48,000 frames including
  the resampler tail. Playback still copies into fixed output slices.

#### Phase 6 gate

- [x] **GATE-600** Known PCM frames cross a C decoder provider into the real
  playback pipeline with correct seek, failure, ownership, teardown, and unload
  behavior under sanitizers. macOS ASan/UBSan acceptance uses
  `plugins.decoder_stream`, `transport.decoder_contract_pcm`, `transport.seek_tail_pcm`
  and `api.uri_player`. Native Linux and Windows verification remains deferred.

### Phase 7: expand and migrate the public API

- [x] **LOCAL-STARTUP** `MC_CONTEXT_START_LOCAL` selects the local catalog and
  defers the default remote connection until selection or explicit reconnect.
  Saved remote startup without this flag preserves its normal behavior.
  `musikcore.integration.runtime.local_startup` verifies no initial connection
  and explicit opt-in against loopback on macOS; Linux/Windows runs are pending.

- [x] **PUB-001** Add preference namespace, typed values, batch updates, and
  persistence contracts.
  Settings and Session support typed reads, atomic in-memory batches, and
  explicit saving through the C ABI. `musikcore.integration.api.managed_runtime`
  verifies scope isolation, copied text, and rejection of an invalid batch.
  Hotkeys use a separate store and key enumeration.
  `musikcube.ui.browse` launches search through a configured key override.
- [x] **PUB-002** Add category, album, directory, and remaining metadata query
  shapes.
  Browse and Directories use typed ordered results for their track panes.
  Now Playing projects metadata from its queue snapshot using the same paged
  track view. The indexed-query and loopback tests cover order and headers.
  `musikcore.integration.api.remote_facets` verifies contextual facets through
  established remote category/search/metadata commands when the authenticated
  server environment does not advertise native support. It covers stable IDs,
  duplicate labels, bounded metadata batches, cancellation, and generations.
  Advertised support retains the native single-query path; query errors are
  not retried. Frontends supply predicates; selection order is app state.
- [ ] **FACET-COMPAT-REMOVAL** Remove the temporary core composition/capability
  routing after the maintainer's Pi server upgrade is ready. Compatibility is
  retained by explicit maintainer request for functionality testing until then;
  the Rust frontend already uses only the common public facet operation.
- [x] **PUB-003** Add coherent playback snapshots.
  The transport control surface reads state, position, duration, volume,
  and mute through `mc_playback_get_snapshot`. Category and track lists use
  `mc_playback_get_current_track` with dispatcher notifications. The
  `musikcore.integration.api.production.playback_commands` test checks the
  current track while prepared, playing, paused, and stopped. Transport and
  the window title read loaded metadata through
  `mc_playback_get_current_track_metadata`. The indexed playback contract
  checks that the snapshot reflects playback's in-memory tags during buffering,
  preserves empty fields, and remains readable after stop.
  The playback command contract checks that a buffering track has a reference,
  repeated startup stream events do not repeat its track notification, and
  canceling startup clears the track before playback reaches Playing.
- [x] **PUB-004** Add playback commands and ordered state events.
  The app's transport keys and controls use `mc_playback_control`.
  `musikcore.abi.indexed_query` exercises the bound service with a fake
  transport, including play now, play next, append, prepend, and rejection
  after switching libraries. Live playback subscriptions deliver one event
  kind per callback on the owner thread with optional filters and coalescing
  of repeated kinds. `musikcore.integration.api.production.playback_commands`
  covers independent subscribers, filter ownership, cross-thread replacement
  and unsubscription, in-flight delivery, and scoped teardown. It also verifies
  output reload during Buffering/Buffered startup for Immediate and Wait modes,
  getter transitions during startup, and that explicit Stop never restarts.
- [x] **PUB-005** Add queue snapshots and commands for existing queue edits.
  Move, remove, and clear commands power the Now Playing keyboard editor.
  Track Search inserts tracks through origin-bound references. Now Playing
  reads an ordered snapshot and refreshes on playback dispatcher notifications.
  Positional actions reject snapshots invalidated by another queue writer.
  The indexed-query test exercises deferred callback delivery and reentrant
  subscription changes through the dispatcher.
  Now Playing loads saved playlists through the local or remote typed
  `GetPlaylistQuery` and replaces the queue without starting playback.
  The indexed-query and remote transport tests cover playlist order,
  duplicate IDs, and rejection of a delayed result after switching libraries.
  The Now Playing picker lists bound-library playlists and saves or overwrites
  them from an ordered queue snapshot. The indexed-query test verifies the
  listed ID and name and reloads the overwritten queue with duplicates.
  The Now Playing picker also renames and deletes bound-library playlists.
  The headless TUI verifies the updated name and removal through the picker.
  Track Search and Browse add tracks, albums, and categories using the bound
  playlist picker and typed save or append commands. The indexed-query test
  verifies that an ordered append keeps duplicates and leaves prior rows in place.
  Atomic batch edits are deferred beyond parity.
- [x] **PUB-006** Add local indexer paths, scan commands, state, progress, and
  terminal events. `musikcore.abi.remote_query_transport` verifies local path
  editing and scan notifications while a remote catalog is selected.
  `mc_indexer_cancel` cooperatively cancels the active scan and discards scans
  queued before the call, preserving processed updates and allowing later scans.
  `MC_INDEXER_CANCELLED` is a distinct terminal event after the scanner settles
  to Idle; missing-track cleanup is suppressed for incomplete scans.
  `musikcore.integration.runtime.indexer_cancel` checks active/queued cancellation,
  idempotency, idle publication, reuse, and shutdown on macOS. Provider handoff
  races have a separate `indexer_provider_cancel` regression; Linux and Windows
  execution of these new contracts remains pending.
- [x] **PUB-007** Add output selection, replay gain, equalizer, and transport
  settings required by musikcube.
  Cube reloads output through `mc_playback_control` and reads replay-gain
  availability from the playback snapshot. The
  `musikcore.integration.api.production.playback_commands` test verifies
  both against the engine boundary.
  Output driver and device choices use the C ABI in the existing playback
  overlays. `musikcore.integration.plugin.playback_reload` verifies output
  discovery, driver validation, and fallback during playback reload.
  The equalizer overlay reads and updates the SDK band snapshot.
  `musikcube.ui.browse` exercises a band edit and reset with the real
  equalizer plugin and checks that Settings displays the headless output.
  The same scenario switches from gapless to crossfade in Settings.
  `musikcube.ui.transport.*` holds the first output buffer and checks that the
  title stays visible through buffering, playing, pause, resume, and stop.
  The gapless and crossfade PCM integration tests check the actual audio
  transitions using the same paced output fixture.
  `musikcube.ui.lyrics` checks that lyrics and loaded playback metadata follow
  the current track. The backend parity test checks embedded lyrics through the
  remote projection path.
  `musikcore.integration.plugin.playback_reload` verifies transport mode
  and playback gain round trips through the SDK, including preamp range
  validation. `musikcube.ui.browse` also changes replay gain to track mode.
- [ ] **PUB-008** Add advanced consumer stream/decoder/encoder APIs only for
  approved use cases.
- [x] **PUB-009** Cover implemented public feature families with their named
  contract/integration tests below. Every future family still requires tests
  alongside implementation; deferred APIs are not claimed as covered.
- [x] **CPP-003** Wrap the public feature families used by Cube.
  `musikcore.public_headers` builds each wrapper as a standalone include.
- [x] **MIG-003** Migrate preferences and settings consumers.
  Search and Browse use the public preferences API for sort and session state.
  Startup, layout and theme preferences, and remote endpoint settings also
  use the public store. `musikcore.integration.api.managed_remote` starts
  from endpoint settings written as a single public batch.
- [x] **MIG-004** Migrate remaining library/category/directory views.
  Browse receives local playlist mutations through context dispatch.
  `musikcore.abi.dispatch` verifies notification ordering and
  suppression on selection changes. `musikcube.ui.browse` exercises playlist
  editing and refresh.
- [x] **MIG-005** Migrate playback and transport consumers.
  `musikcore.integration.api.production.playback_commands` covers the public engine contract.
- [x] **MIG-006** Migrate queue editing and now-playing consumers.
  `musikcube.ui.queue` and `musikcube.ui.playlists` cover the existing UI flows.
- [x] **MIG-007** Migrate indexer and local-library settings. Cube uses the SDK
  for indexer commands, directory paths, and dispatch notifications.
- [x] **MIG-008** Migrate environment/output/equalizer consumers.
  The visualizer chooser and hotkey use the public visualizer API.
  The inline WinUI meter pulls a copied FFT spectrum from the C API.
  `musikcore.integration.transport.gapless_pcm` verifies bin levels and
  sample rate from headless playback without opening a visualizer plugin.
  `musikcore.integration.plugin.playback_reload` and `musikcube.ui.browse`
  cover an installation without a visualizer plugin.
  The equalizer overlay subscribes to public dispatch for live updates.
  `musikcore.integration.api.managed_runtime` verifies coalesced changes
  using the equalizer plugin.
  Server settings use a typed snapshot and configuration call.
  The managed runtime test checks the real plugin with both listeners disabled,
  and `musikcube.ui.browse` opens the original server setup overlay.
  Cube lists plugins and edits exposed settings through the public catalog.
  The managed runtime test verifies enablement and schema validation.
  `musikcube.ui.browse` edits the Null plugin's sample rate and opens advanced
  application settings using the same overlay.
- [x] **MIG-009** Remove private musikcore headers from musikcube targets.
  `musikcube.integration.core_boundary` checks app includes and the executable's imports.
- [x] **MIG-010** Link the normal frontend only through the public C ABI.
  The executable uses the full production runtime. See [ADR-0006](../docs/adr/0006-enforce-the-frontend-consumer-boundary.md).
  `musikcore.integration.api.managed_runtime` starts production services
  through public headers, exercises blocking-host wakeups and delayed playback
  work, then destroys and restarts the runtime with a retained catalog handle.
  Cube now starts those services through the same public context option.
  `musikcore.integration.api.managed_remote` covers local/remote switching
  and same-endpoint reconnection through that startup path.
  `musikcore.integration.api.remote_audio` exercises the real loopback server,
  HTTP stream, decoder, and headless output on Linux.
  `musikcore.integration.api.remote_transcode.*` also loads the stock
  encoders and exercises streaming MP3 and blocking FLAC.

#### Creation-time service selection

- [x] **API-SERVICES-001** Replace the managed-runtime flag with fixed creation-time
  Library/Plugins/Indexer/Playback selection and full-engine defaults. Add the
  immutable expanded-set getter in C and the header-only wrapper. Cover omitted
  fields, explicit none, all combinations, invalid masks, thread ownership, and
  restart in `musikcore.integration.runtime.services` and `musikcore.abi.arguments`.
  `musikcore.integration.runtime.plugin_directory` guards failed-discovery rollback.
- [x] **API-SERVICES-002** Decouple catalog construction from indexing and playback
  from scanner subscriptions. Request only Indexer in the media seeder; exercise
  real provider playback without Indexer in
  `musikcore.integration.api.playback_without_indexer`. Both that test and
  `musikcore.integration.api.managed_runtime` reopen the indexed catalog with
  only Library, checking configured status and queries without a new scanner or
  provider session. Preserve rollback and shutdown regressions in the existing
  runtime and plugin suites.
- [x] **API-SERVICES-003** Update Cube/daemon startup, comments, lifecycle contract,
  and C/C++ examples for both context-owned engine and caller-owned catalog modes.
  Compile/run both against the relocated shared SDK in
  `musikcore.integration.api.installed_sdk`; retain daemon lifecycle and TUI tests.

- [x] **API-SERVICES-004** Persist migration-required scanning with the catalog
  transaction; resume it after service bindings/subscriptions are ready. Keep the
  marker through failed metadata writes, commits, and completion writes.
  `musikcore.integration.runtime.migration_scan` runs library-only migration,
  failed writes, successful retry with refreshed tracks, and no-repeat startup
  in separate processes.
  `musikcore.integration.runtime.database_failure` checks schema/marker rollback.
- [x] **API-SERVICES-005** Retire visualizer state for every provider session,
  including Plugins-only contexts. `musikcore.integration.runtime.visualizer_session`
  uses a headless legacy provider to check hide/release and disabled-provider
  rediscovery after context destruction.

Selection is immutable for the context lifetime; ordinary scan and playback
commands remain available. The dependency graph and rationale are in
[ADR-0005](../docs/adr/0005-context-owned-production-runtime.md). Validation for
this slice targets native macOS; Linux/Windows parity remains deferred by the
maintainer. No plugin C SDK work is included.

Native macOS arm64 validation: full Debug and Release suites pass 93/93;
ABI-only Debug passes 29/29. These include daemon lifecycle, real headless
plugin playback, local/remote parity, TUI scenarios, installed/relocated C/C++
examples, standalone testing-disabled configuration, and exact binary exports.
The migration, visualizer-session, and schema-rollback cases also pass three
consecutive Release runs and a focused independent rerun. Migration completion
preserves the indexer's existing best-effort per-file/provider semantics;
interrupted/source-only exclusions are explicit in code, without new dedicated
marker-retention fixtures for those paths.
The pre-existing parity fixture deferred playback initialization until after its
commands; it now finishes initialization first and disables unrelated session
restoration. Logs are under `build/services-*.log`.

#### Explicit URI playback

Maintainer-approved core API slice: provide manually constructed URI players
without catalogs/indexing, using native macOS validation while platform parity
remains deferred. This does not begin the new plugin C SDK. See
[ADR-0007](../docs/adr/0007-explicit-uri-player-resources.md).

- [x] **API-URI-001** Add the public `mc_player` ownership, control, snapshot, and
  dispatch callback contract, plus a strictly header-only movable C++ wrapper.
  Validate absence/malformed inputs in `musikcore.abi.arguments` and headers/
  exports/inventory through the existing ABI boundary checks.
- [x] **API-URI-002** Supply a generic provider environment without a catalog and
  scope worker completion to individual transports. Stop URI resources before
  retiring the context's provider session; retained handles remain safely inert.
- [x] **API-URI-003** Harden opening/publication and cancellation. Require
  create-before-open through SDK 23 factory/environment contracts, including
  nested GME/OpenMPT streams; remove optional exports and compatibility paths.
  Stock HTTP supports interrupted opens and WAV MIME types.
- [x] **API-URI-004** Exercise real local/file/HTTP decoding without a database,
  pause/seek/EOF/failure, wrong-thread and callback destruction rejection,
  independent players, early close, observer replacement within a detached
  dispatch batch, empty HTTP failures, sustained HTTP readiness before EOF,
  stop and replacement during a blocked HTTP open, cancellation during HTTP
  retry backoff, seek rejection after EOF, and retained handles in
  `musikcore.integration.api.uri_player` using headless output and loopback HTTP.
- [x] **API-URI-005** Ship C/C++ example apps and verify their public-only builds
  and playback against the relocated SDK in `musikcore.integration.api.installed_sdk`.
- [x] **API-URI-006** Migrate in-tree stream factories and environment callers to
  SDK 23. In `musikcore.integration.runtime.stream_sdk_contract`, verify creation
  leaves disk untouched, cancelled publication prevents opening, explicit opening
  still writes correctly, accepted plugins are acquired once, and SDK 22 plugins
  are released before unload without querying incompatible factories. Real GME
  and OpenMPT providers also cover unopened release, successful nested opening,
  and cancellation before opening. Windows runs the same contract against GME
  and OpenMPT. Windows CDDA device cancellation is tracked in API-URI-007.
- [x] Allow a managed context to select a catalog from an explicit remote
  session through `mc_context_set_library`. The playback queue follows the
  selected origin. A lost connection invalidates that generation without
  retargeting retained results. `musikcore.integration.api.managed_remote`
  verifies selection, queue admission, and disconnect invalidation.
- [x] **API-FACETS** List contextual genre, artist, and album values with a
  track search and typed category constraints.
  `musikcore.integration.api.backend_parity` checks search and constraints
  against the local catalog and the loopback query server.
- [ ] **API-URI-007** Implement and validate Windows CDDA device cancellation.
  Its existing `Interrupt` is a no-op; SDK 23 creation now exposes the stream
  before opening, but does not by itself cancel its synchronous device I/O.
  Audit pre-open cancellation and blocked device reads on Windows hardware.

Native URI-player validation is included in the full-suite results above.
The initial independent C++ review reproduced stale observer delivery and HTTP
readiness lost-wakeup bugs; both have regressions in the URI integration case.
SDK 23 tests load real GME/OpenMPT providers for nested create/open/interrupt
behavior. Windows CDDA device cancellation and native platform acceptance remain
explicitly open rather than inferred from these device-free macOS checks.

#### Managed runtime hardening reconciliation

The startup/teardown protections from the earlier `f83bbb028` implementation and
plugin fixes from `ebda8eb12` are adapted to the managed runtime rather than
replacing its public API. The process-owned plugin registry remains unchanged;
session factories and visualizers retire after workers stop. This does not
claim module-unload support or per-context configuration-root isolation.

- [x] **RTH-001** Keep distinct persisted catalog IDs and stable lookup by name.
  `musikcore.integration.runtime.registry` guards both contracts.
- [x] **RTH-002** Restore local saved playback state before publishing the engine.
  `musikcore.integration.runtime.startup` checks that first dispatch cannot
  overwrite an immediately submitted volume command. Remote restore stays async.
- [x] **RTH-003** Distinguish wrong-thread use from reentrancy and preserve owner
  recovery. `musikcore.integration.runtime.owner`, `musikcore.abi.dispatch`, and
  production `playback_commands` require `MC_ERROR_WRONG_THREAD`.
- [x] **RTH-004** Finish all provider teardown hooks after a failure and drain
  accepted callbacks before reporting it. `musikcore.integration.runtime.failure`
  uses two throwing providers and verifies startup rollback and destruction retry.
- [x] **RTH-005** Isolate public read snapshots from legacy shared-cache writers.
  `musikcore.integration.runtime.read_isolation` holds an uncommitted WAL write
  while the public API must read the previously committed title.
  `musikcore.integration.runtime.locked_read_shutdown` holds an exclusive
  SQLite lock through shutdown and checks prompt cancellation.
- [x] **RTH-006** Synchronize stream-factory discovery and retire stream/codec
  caches after workers stop, even when a hook throws.
  `musikcore.integration.runtime.caches` checks concurrent lookup and balanced
  acquisition/release counts for each provider. Null output state is atomic;
  FFmpeg frames copy owned channel-layout storage. Existing PCM, reload, and
  managed-runtime checks exercise real plugin playback.
- [x] **RTH-007** Reject failed database opens and corrupt schemas transactionally.
  `musikcore.integration.runtime.database_failure` checks `MC_ERROR_IO`, an empty
  context output, rollback of partial schema changes, and successful retry
  after repairing the catalog.

The managed, plugin-reload, captured-PCM, remote-HTTP-audio, and transport TUI
checks run natively on macOS and Linux using platform-correct staged filenames.
Failure-provider modules stay under the build's test fixtures, outside normal
app plugin discovery. Browse waits for a complete plugin row before navigation,
so split PTY writes cannot produce a negative row index.

The named runtime regressions run in the native macOS full suite recorded
above; earlier focused runtime-hardening cases also passed repeated Debug runs.
These are device-free checks and do not establish physical CoreAudio or native
Windows behavior. Recorded Linux results apply only to their stated revision.

#### Core consumer release acceptance

- [x] **CORE-RC-001** Validate a non-C++ foreign-runtime consumer of the
  installed shared library. The maintained Rust consumer lives in
  `musikcore-bindings/rust/bindings`; it remains independently runnable with
  `MUSIKCORE_SDK=<relocated-prefix> cargo test --locked -p musikcore` from that
  repository's `rust/` workspace. Its named integration cases include
  `runtime_contracts`, `shutdown_delivers_once_and_releases_capture_even_without_dispatch`,
  and `shutdown_completion_can_drop_event_views`. These exercise isolated
  catalogs, async submission, retained callbacks, owner-thread dispatch, and
  shutdown. Use Rust as the foreign-runtime acceptance vehicle rather than
  introducing another language or making core CTest depend on a sibling checkout.
- [x] **CORE-RC-002** Review installed headers/examples for service selection,
  owner-thread rules, request/output ownership, cancellation, and teardown.
  `musikcore.integration.api.installed_sdk` passes; separately compiled C/C++
  query and URI-player examples pass against an extracted, relocated SDK archive
  with the original install prefix unavailable. Rust tests and all five doctests
  also pass against that archive. This review requires no public API changes.
- [x] **CORE-RC-003** Confirm consumer release acceptance before beginning the
  C provider SDK: API-013 instrumentation, CORE-RC-001/CORE-RC-002, GATE-700,
  and the consumer/application portions of PKG-001 and PKG-005 through PKG-009.
  Use currently shipped SDK 23 plugins for runtime smoke tests. Pure-C provider
  installation/loading (PKG-003/PKG-004) and combined GATE-900 follow provider
  implementation, so they cannot be prerequisites for starting it. Keep approved
  platform exceptions explicit rather than treating them as passing results.
  Consumer acceptance is complete for this pass: native macOS archive checks
  below pass, and the maintainer has explicitly deferred fresh Linux/Windows
  parity runs to their native sessions. Existing platform evidence remains
  scoped to its recorded revision. This permits Phase 4 bootstrap work, not
  completion of the combined provider/package GATE-900.

#### Phase 7 gate

- [x] **GATE-700** musikcube builds and passes smoke and integration tests
  using the public C ABI and header-only wrapper. Linux Release passed
  `musikcube.integration.core_boundary` and all four headless scenarios.
  Native macOS Debug/Release validation also passes the frontend import guard,
  installed consumers, managed local/remote sessions, remote HTTP audio, captured
  gapless/crossfade PCM, and six TUI scenarios. Native Windows Debug and Release
  pass 87/87 each, including both frontend import checks, the relocated SDK,
  managed and headless runtime, remote HTTP audio and MP3/FLAC transcoding,
  captured PCM, and 10 console UI tests through ConPTY.

Native macOS arm64 consumer validation (2026-11-22, `6525f9aff`, Rust
bindings `4ea5bbd`): Release passes 109/109; Debug ASan/UBSan passes 114/114.
An extracted standalone app under a path containing spaces passes three
startup/search/queue/clean-exit runs through the existing PTY driver. The
extracted daemon passes `tests/scripts/check-daemon.js`, including competing
starts, rescan, termination, and background startup failure. These checks add
no new physical audio-device or Linux/Windows evidence.

Freshly staged full-plugin archives can exceed the driver's ordinary ten-second
screen deadline before drawing their first frame. Startup now has a separate
60-second bound; interaction deadlines and immediate exit/error detection are
unchanged. The dependent ad-hoc SDK check also now creates its fixture manifest
before launching the TUI. The final extracted archive passes three runs; no
production startup failure was established.

### Phase 8: representative families and remaining plugin conversions

Contract-definition portions run in PLG-CONTRACT-002; implementation begins only
after the new loading/empty/null path is established. Complete one representative
per family under PLG-PROVE-002 before backfilling the remaining plugins.
Unmigrated families retain their current SDK during the approved output-first
stage; migrated families must not keep a parallel former-SDK path.

#### Provider discovery and configuration

- [ ] **PLG-MGMT-001** Include C provider metadata and enable preferences in
  `mc_plugins_list` and configuration flows before converting user-facing plugins.
  Define metadata discovery for disabled providers without initializing them;
  test listing, preference changes, and restart. Phase 4 proves runtime loading,
  not this application-facing catalog integration. Add typed schema/settings
  support with EXT-001.

#### Data streams and encoders

- [x] **ENC-001** Define blocking encoder output to a host stream.
- [x] **ENC-002** Define streaming encoder packet ownership.
- [x] **ENC-003** Implement encoder adapters and pure-C fixtures.
  `plugins.encoder` covers independent instances, packets, flush and unload.
  The server transcode tests exercise the real factory on macOS.
- [x] **ENC-004** Convert `stockencoders`. `server.transcode_body` covers
  flush and output failure, and `api.remote_transcode` exercises the real
  MP3 and FFmpeg providers on macOS. Windows and Linux remain to be verified.
- [x] **DST-001** Define data-stream factory capability and creation tables.
- [x] **DST-002** Implement data-stream factory adapters and fixtures.
- [x] **DST-003** Convert `httpdatastream` alongside local file and FFmpeg
  playback. Bring forward the C host services for the data directory and
  plugin-scoped settings, including schema discovery. Preserve the existing
  cache location, setting keys, defaults, and limits. Verify that a saved
  cache or timeout override is honored and that authenticated remote playback
  and Stop during a stalled request still work.
  `api.uri_player` covers live settings, chunked HTTP, cancellation and
  overlapping cache downloads. `plugins.progressive_http_read` verifies reads
  resume when another chunk arrives before EOF. `plugins.shared_http_cache`
  verifies that two processes can download the same URL, recover a complete
  entry, and clean up after an interrupted transfer. `api.remote_transcode`
  plays a remote track through the authenticated server. Its MP3 cases check
  that musikdroid receives a finalized file with an exact length on its first
  request for a finite track, even with caching disabled. Unknown-duration
  inputs and other streaming clients can receive bytes during encoding.
  `mp3_range_fallback` covers opt-in full encoding for an uncached range.

#### Decoder plugins

- [x] **PLG-DEC-001** Convert `ffmpegdecoder`. `transport.gapless_pcm`,
  `transport.seek_tail_pcm` and `api.remote_transcode` exercise the C DLL on macOS.
- [x] **PLG-DEC-002** Convert `gmedecoder`.
- [x] **PLG-DEC-003** Convert `libopenmptdecoder`. The GME and OpenMPT
  media DLLs each register a stream and decoder. The installed C++17 plugin
  SDK provides `SubtrackSource` for indexed containers and `seekStream`
  for codec callbacks using relative offsets. Ship the indexers alongside
  their codecs. Each indexer declares its media plugin filename through
  `GetParentPluginFilename` so its enable setting follows the codec until
  overridden. `plugins.subsong_gme` and `runtime.stream_sdk_contract` cover
  selection on macOS. `musikcube.ui.settings` checks both plugin rows after a
  toggle. `plugins.subsong_openmpt` also verifies that the decoder consumes a
  final block carrying EOF without requesting it again.
- [ ] **PLG-DEC-004** Convert Windows `cddadecoder` and its stream/source
  capabilities. CD audio playback is unavailable on this branch until its
  device reads support cancellation. Verify Stop during a blocked read, disc
  eject, and shutdown on a Windows machine with a drive.

#### Tags and indexer sources

- [ ] **TAG-001** Define metadata writer and tag reader/store tables.
- [ ] **TAG-002** Implement adapters and deterministic tag fixtures.
- [ ] **TAG-003** Convert `taglib_plugin`.
- [ ] **IDX-001** Define indexer source, writer, cancellation, and
  commit/rollback tables.
- [ ] **IDX-002** Implement adapters and fixture source.
- [ ] **IDX-003** Convert decoder-provided indexer sources.

#### Audio outputs

- [ ] **OUT-001** Finalize real-time-safe output and device contracts.
- [x] **OUT-002** Implement buffer submission/recycle state machine.
  Production audio::Output publishes bounded token records before queueing and
  reports successful buffers to each player worker in submission order, including
  when a gapless successor reuses the previous track's slots. A device failure
  reaches the worker even if the device holds an earlier buffer until Stop.
  `plugins.output_host` and `transport.playback_pcm` cover both cases, inline
  completion, backpressure, cancellation, and failure without false natural EOF.
  `plugins.output_host` also covers writer revocation across output reuse and
  readiness after early completion. `transport.retired_crossfade_pcm` covers
  shutdown with a blocked fade tail. `transport.failed_next_crossfade_pcm`
  keeps the active track playing through a failed prefetch. `transport.seek_eof_pcm`
  verifies the first samples after buffered and post-EOF fractional seeks.
  `transport.seek_tail_pcm` covers a seek accepted while the last output buffers play.
  `api.uri_player` keeps one session blocked while its peer plays and closes.
- [ ] Before freezing output timing, distinguish when a buffer can be reused
  from when its samples become audible. Specify a media-to-output timeline and
  fallback for unknown device timing, including seek and crossfade overlap.
  Exercise a backend that returns copied PCM before playback.
- [x] Keep Stop reachable during a blocking write. Pause and resume give a
  paced write a short time to finish, then return `MC_ERROR_IN_FLIGHT` if the
  device is still busy. A rejected pause or resume preserves the prior state
  and reports the selected device's error to the caller. A volume change that
  encounters a busy output leaves the latest gain for the next write without
  waiting. `plugins.output_host` exercises repeated updates during a blocked
  write. Control failures are covered by
  `transport.blocked_control_pcm`, `transport.gapless_control_failure_pcm`,
  `transport.crossfade_control_failure_pcm`, and `transport.uri_control_failure_pcm`.
  During overlap, `transport.paused_crossfade_pcm` checks that rejection by
  the selected device leaves the overlap playing and rejection by an outgoing
  device retires only its tail. PCM tests also verify that a seek from pause
  restores the selected track's gain and that a stopped or
  failed replacement cannot bring back the previous track.
- [ ] **OUT-003** Add debug instrumentation for forbidden real-time host calls.
- [x] **OUT-004** Convert `nullout` as the first production output.
  - [x] Ship a C-only shared library with synchronous, interruptible producer pacing.
    `plugins.null_output` exercises its factory/instances and native unload;
    `binary.exports.nullout` verifies the only export is `mc_plugin_get_api`.
  - [x] Restore engine selection, catalog/settings, playback/UI, and installed
    consumers through the C output factory (Phase 4 validation above).
- [x] **OUT-005** Convert `coreaudioout`.
  `plugins.coreaudio_output` validates preallocated AudioQueue storage,
  exactly-once return, interruptible drain, pause/restart, format changes,
  public URI playback, and teardown before DLL unload on macOS. Timing remains
  optional. A missing pinned device follows the system default on the next
  open and retains the pin for reconnection. `plugins.output_host` exercises
  disappearance and reconnection; live device loss during playback remains
  a manual follow-up.
- [ ] **OUT-006** Convert `alsaout`.
- [ ] **OUT-007** Convert `pipewireout`.
- [ ] **OUT-008** Convert `portaudioout`.
- [ ] **OUT-009** Convert `pulseout`.
- [ ] **OUT-010** Convert `sndioout`.
- [ ] **OUT-011** Convert Windows `directsoundout`.
- [ ] **OUT-012** Convert Windows `wasapiout`.
- [ ] **OUT-013** Convert Windows `waveout`.

#### DSP and remaining plugin categories

- [x] **DSP-001** Define in-place processing on the playback worker.
- [x] **DSP-002** Convert `supereqdsp`. `plugins.dsp_processing` checks two
  active processors created by the media host and live equalizer changes on macOS.
  `plugins.dsp_failure` verifies that a partly modified buffer is discarded.
  Windows and Linux
  remain to be verified.
- [x] Implement the analyzer factory and metadata writer on the indexer worker.
  `plugins.analyzer` checks real indexing with successful and failing C
  providers. Analyzer changes for a track are saved only when all participants
  succeed, as recorded in ADR-0011. Cancellation waits for the current analyzer
  call to return. `plugins.analyzer_cancel` covers cancellation and shutdown
  during blocked stream open. The analyzer currently uses the internal Stream
  pipeline over the C stream and decoder primitives. Reassess that remaining
  C++ interface when consolidating decoded-audio consumers. Windows and Linux
  remain to be verified.
- [ ] **EXT-001** Design schema/configuration descriptors as data.
- [ ] **EXT-002** Migrate visualizer providers.
- [ ] **EXT-003** Migrate playback-remote providers.
- [ ] **EXT-004** Migrate server integration without crossing C++ interfaces.
- [ ] **EXT-005** Migrate platform hotkey and media-key plugins.
- [ ] **SDK-013** Add contract fixtures and malformed cases for every supported
  provider family.
- [ ] **SDK-014** Add provider-family performance and teardown tests.

#### Phase 8 gate

- [ ] **GATE-800** The engine uses only PluginRegistry and the
  new C contracts. All shipped plugins use the C SDK or an explicitly approved
  non-plugin integration; no active engine dependency, exported plugin function,
  build target, or installed header preserves the old C++ SDK. Required app
  workflows and plugin tests are restored; missing optional families and
  a missing output remain supported idle configurations; bundled `nullout`
  supplies headless playback.

### Phase 9: system, packaging, and compatibility

- [x] **SYS-001** Add musikcube pseudo-terminal startup/shutdown smoke test.
  `node tests/ui/scenarios/track-search.js` starts the app in a PTY and quits through
  its normal key binding.
- [ ] **SYS-002** Cover valid, unset, and unparseable `TERM` behavior.
- [x] **SYS-003** Add musikcubed startup/readiness/shutdown smoke test.
  `musikcubed.integration.lifecycle` covers concurrent starts, foreground
  scan completion, background readiness and startup failure, and shutdown.
  Native macOS Debug/Release runs pass.
- [x] **PKG-001** Install headers, shared library, platform import library,
  and package metadata into a temporary prefix. Unix and Windows supply the
  relocatable `musikcore::musikcore` CMake target. On Windows,
  `musikcore.integration.api.installed_sdk` moves the prefix and independently
  builds and runs the installed C and C++ consumers, including managed
  runtime and playback against the installed DLL and plugins.
- [x] **PKG-002** Build external C and C++ consumers against the installed prefix
  on native Linux, macOS, and Windows. `musikcore.integration.api.installed_sdk`
  relocates the prefix,
  builds both query examples independently, and runs them against indexed media.
  A public-only C consumer switches from a remote catalog with a withheld reply
  to the local fixture. It checks retained track origin, dispatcher thread
  ownership, self-unsubscription, rejected callback shutdown and destruction,
  and cancellation during owner-thread shutdown.
- [ ] **PKG-003** Build an external C decoder plugin against the installed SDK.
- [ ] **PKG-004** Load and run the installed external plugin.
- [x] **PKG-005** Validate ELF SONAME/RPATH and symbol versions. Linux Release
  installed with the expected `libmusikcore.so` SONAME and relative runtime
  paths. The complete runtime passed `script/scan-standalone.js` at glibc 2.28.
- [x] **PKG-006** Audit packaged Mach-O install names, RPATHs, and dependencies.
  Archive staging and SDK installation share `FixupMachORpaths.cmake`, which
  removes absolute build paths, supplies relative search roots, and re-signs
  changed binaries. `musikcore.integration.binary.macos_package` rejects stale
  absolute paths, escaping relative paths, and absolute install names; it checks
  repeated fixup and signature validity. `musikcore.integration.api.installed_sdk`
  rejects absolute RPATHs in copied libraries. The regression failed on the
  original FFmpeg dependency before the fix and passes afterward. The extracted
  app passes `script/scan-standalone.js`; all 52 app/SDK Mach-O signatures and
  install-name/RPATH checks pass. Export allowlists pass in the full suite.
  Debug installed consumers use the SDK's sanitizer selection and explicitly
  link the compiler runtime; they cannot rely on an absolute toolchain RPATH
  inside the relocated library. The installed-SDK regression also passes with
  native macOS ASan/UBSan after this test-build correction.
- [x] **PKG-007** Validate Windows DLL/import-library/plugin placement.
  Visual Studio 2026 x64 Debug and Release stage `musikcore.dll` beside both
  frontends, the import library under the configuration's build library directory,
  and plugins in the runtime's `plugins` directory. The
  `musikcube.integration.windows_core_dll` test verifies frontend imports and
  the C ABI export allowlist. Both staged configurations passed the Windows
  dependency audit.
- [x] **PKG-008** Run consumer, SDK 23 plugin, and application smoke tests from
  staged standalone archives. Native macOS extracted app startup/search/queue/
  shutdown passes three times; daemon lifecycle, competing starts, rescan, and
  background failure checks pass. External C/C++ queries and URI players plus
  Rust tests and five doctests pass against the extracted SDK with its original
  install prefix unavailable. URI players use shipped FFmpeg/null-output plugins;
  physical audio-device behavior is outside this check. Fresh Linux/Windows
  archive runs are maintainer-deferred; pure-C provider smoke remains separately
  deferred with PKG-003/PKG-004 and does not block core consumer acceptance.
- [x] **PKG-009** Verify consumer packages have no build-machine loader paths or
  unexpected dependencies. The native macOS app/SDK load-command and package
  metadata checks pass after RPATH cleanup. The package scanner rejects absolute
  and out-of-package RPATHs even when a working fallback exists. The existing
  explicit host-provided Homebrew PortAudio exception remains limited to the
  optional output plugin. This checks runtime/package paths, not arbitrary
  diagnostic source strings embedded in vendor binaries. New Linux/Windows
  checks remain maintainer-deferred; GATE-900 stays open for combined acceptance.
- [ ] **CI-001** Add the fast contract/unit/integration matrix for Windows,
  macOS, GCC, and Clang.
- [ ] **CI-002** Add ASan/UBSan jobs.
- [ ] **CI-003** Add a separate TSan job.
- [ ] **CI-004** Add scheduled stress, fault-injection, and package jobs.

#### Phase 9 gate

- [ ] **GATE-900** Installed and archived artifacts pass external consumer,
  external plugin, application smoke, dependency, and runtime-path tests on all
  supported platforms.

### Phase 10: migration cleanup and ABI v1 freeze

- [ ] **FIN-001** Complete the final frontend private-dependency audit. Unix
  musikcube/musikcubed pass their `integration.core_boundary` import checks;
  `musikcube.integration.windows_core_dll` checks production Windows PE exports
  and console/GUI imports. Preserve this evidence through final cleanup.
  `musikcore.integration.binary.exports.*` checks the shipped Unix core,
  executable, and plugin export lists, including symbols pulled in by static
  libraries.
- [ ] **FIN-002** Confirm every shipped plugin has left the C++ SDK ABI.
- [x] **FIN-003** Remove or archive `musikcore_c.h`, `c_context.cpp`, and
  `c_interface_wrappers.cpp` after their consumers are gone. The fixture
  seeder uses the public indexer and is exercised by `musikcore.abi.indexed_query`.
- [ ] **FIN-004** Remove obsolete C++ SDK interfaces from installed/public
  include paths.
- [x] **FIN-005** Replace `core_c_demo` with maintained C and C++ examples.
  `musikcore.abi.indexed_query` runs both query examples against an indexed fixture.
- [x] **FIN-006** Retired: no `musikcore_legacy` target was introduced.
  FIN-003 removed the old C boundary; current private test archives are not
  legacy public libraries and remain isolated by `link_boundary`.
- [ ] **FIN-007** Retire the reduced `musikcore_abi0` runtime after its contract
  fixtures can use the production boundary. Unix production already uses the
  `musikcore` name; Windows now supplies the production DLL too.
- [ ] **FIN-008** Assign ABI major version 1 and platform SONAME/install-name/DLL
  versioning.
- [ ] **FIN-009** Freeze and publish the export allowlist and public layout
  baseline.
- [ ] **FIN-010** Preserve a v1 C consumer binary/source fixture.
- [ ] **FIN-011** Preserve a v1 C plugin binary/source fixture.
- [ ] **FIN-012** Test older v1 fixtures against every newer compatible v1
  build.
- [ ] **FIN-013** Publish API, SDK, ownership, threading, and example
  documentation with release artifacts.

#### Legacy binding cleanup inventory

Cube no longer has library or playback attachment bindings. Playback tests
inspect public queue snapshots. The native fault-injection fixture opens
catalogs through a probe compiled only in test builds. Standalone builds omit
the probe.

#### Final gate

- [ ] **GATE-1000** ABI v1 is released only after all prior gates pass and the
  previous-boundary implementations are absent from shipped artifacts.

## Active next steps

Core consumer acceptance and packaging are complete in the recorded scope on
`clangen/musikcore-c-abi-completion`. The active branch rebuilds the plugin
architecture using the six-step order in Phase 4. Replacing plugin dependencies
must preserve the public application C ABI and its ownership/threading contracts;
capability availability may be temporarily reduced as documented under PLG-EMPTY-002.

1. Keep shared subscription scope and URI lifecycle regressions green. The
   subscription fix centralizes delivery depth in the exported C implementation;
   it does not move typed callback storage into C or make callback-local
   disconnect wait for all admitted calls. `musikcore.unit.subscriptions` checks
   cross-library C/C++ disconnect and ordinary external-thread draining.
2. Preserve TST-008/API-013 instrumentation and fault-detection probes when
   adding targets. Debug now covers direct ABI and production paths; Release
   stays uninstrumented. Linux/Windows execution remains separately owned.
3. Preserve CORE-RC-001/CORE-RC-002 acceptance with the independently maintained
   Rust consumer and installed C/C++ examples. Reuse their existing tests rather
   than adding another foreign-runtime harness.
4. Preserve macOS archive acceptance under PKG-006/PKG-008/PKG-009, including
   shared staging/install fixup, strict loader-path scanning, extracted app/daemon
   runs, and relocated external C/C++/Rust consumers. The native Release suite
   passes 110/110 after packaging changes; the focused UI/SDK/package rerun also
   passes after separating cold-start and interaction deadlines.
5. Preserve recorded native Windows DLL, installed-SDK, playback, and ConPTY
   checks and the existing Linux results. Validate subsequent changes in their
   native platform sessions; the macOS sanitizer change does not establish new
   Windows/Linux coverage. Finish the remaining release gates without reopening
   foundation checks that already have native evidence.
6. Finish PLG-CONTRACT-001/002/003: the high-level family shapes are approved;
   audio/output headers, opaque buffers, the session-owned registry, and a
   boundary-tested loadable `nullout` plugin are implemented. Materialize the remaining family
   headers and reconcile engine callers, then disconnect the old production path
   and select the null factory through the replacement engine. Keep the standalone
   and runtime plugin regressions green. Empty-engine acceptance and the first
   stream/decoder playback proof remain open; GATE-400 is historical bootstrap
   evidence, not completion of this replacement plan.

Deferred expansion includes polling/owned unsolicited events (API-014/API-015),
general metadata invalidation and atomic batch edits (LIB-008/LIB-009), generic
numeric metadata access and capability discovery, and advanced consumer
stream/codec APIs (PUB-008). Packed page storage (LIB-002) is measurement-driven.
These items are not prerequisites for existing application parity. Source-list
consolidation (BLD-001) and CI jobs remain explicit build/test backlog; neither
is silently completed by a passing local suite. TST-008/API-013 are complete
in the stated native macOS scope, with other platform execution still deferred.
Source-list cleanup and a packed arena need not precede external-consumer
ergonomics work.

Historical phase numbers and completed implementation boxes do not override
this dependency order or imply an unchecked cross-platform gate has passed.

## Progress reporting

When completing a task:

1. Check its box in this document in the same commit as the implementation.
2. Add or update the named contract/unit/integration test.
3. Record meaningful architectural decisions as individual ADRs in `docs/adr/`,
   linked from the HLD, with status,
   context, rationale, and consequences; link the affected task. Do not add a
   progress diary or duplicate an existing decision record.
4. Do not check a gate until its acceptance statement passes on every named
   platform or the document explicitly records a temporary exception.

Implementation commits should reference task IDs in their messages where
practical, for example:

```text
ABI-005: add UTF-8 views and buffer-copy semantics
SDK-009: add pure-C fixture plugin
```

This keeps the checklist useful as an execution plan rather than a retrospective
status summary.
