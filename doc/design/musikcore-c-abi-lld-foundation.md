# C ABI LLD: foundation and runtime

Status: **complete** in the recorded foundation-gate scope. Later regressions,
platform validation, and follow-up work remain tracked in the other LLDs.

[LLD index](musikcore-c-abi-lld.md) · [SDK removal design](musikcore-sdk-removal-design.md)

## Constraints from the current repository

- Unix-like builds currently define `musikcore` as a shared CMake target.
- Windows builds `musikcore.dll` and shares it between the GUI and console frontends.
- Windows and Unix build the engine from the shared source list in
  `.cmake/MusikcoreSources.cmake`.
- The retired legacy C boundary (`c_context.cpp`, `c_interface_wrappers.cpp`,
  and `musikcore_c.h`) has been removed. The new consumer boundary lives in
  `src/musikcore/ffi/consumer`.
- Plugins use the C provider entry point; the former C++ plugin entry points have been removed.
- CTest now runs header, lifecycle, dispatch, query, wrapper, backend-switch,
  export, and production/remote integration tests.
- Production binaries and plugins currently share source-tree output
  directories in several configurations.

The core-completion branch preserves the earlier application baseline. The
provider migration now uses the C plugin path; former output-first test
deferrals have been restored under PLG-EMPTY-002. Remaining native-platform
validation and internal SDK removal are tracked in the plugin LLD.

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
[ADR-0001](../adr/0001-c-abi-and-core-first-refactor.md) with
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
[ADR-0002](../adr/0002-host-dispatch-and-request-lifetime.md).

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

Unsolicited notification polling is deferred and remains unimplemented. If it
is introduced later, reserve
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
[ADR-0003](../adr/0003-origin-bound-results-and-paged-metadata.md) with
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
[ADR-0004](../adr/0004-test-the-public-boundary-first.md). The roles below
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
[ADR-0002](../adr/0002-host-dispatch-and-request-lifetime.md). Current ABI
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
[ADR-0003](../adr/0003-origin-bound-results-and-paged-metadata.md).
A direct missing-track reference read returns `MC_ERROR_NOT_FOUND`. Plain queries
use a literal title substring; ordered search keeps typed-backend LIKE wildcards
or explicit regex matching across metadata fields.

A source-less remote identity cannot read metadata, including constructed
results/references. It rejects admission as unsupported. A bound empty range
still validates projection support and connection generation, then returns an
owned empty page without submitting an empty metadata query.

Remote batch queries support a lossless projection that preserves NULL and
empty values. Projected requests require a projected response.

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
[testing design](testing-design.md#test-directory-layout), following
[ADR-0004](../adr/0004-test-the-public-boundary-first.md).
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

