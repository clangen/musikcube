# musikcore C ABI modernization: low-level design and task plan

Status: active migration; macOS foundation validation complete, platform gates open

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

The current goal is a complete core application API consumed by musikcube.
Provider tasks remain deferred until that integration reaches release-candidate
quality. This document is the task/status authority; the HLD links architectural decisions,
and public headers define parameter-level contracts. Runtime and provider
sections describing future services are target design, not implemented features.

The task list uses stable identifiers so progress and dependencies can be
discussed without relying on line numbers. Checkboxes represent repository
state, not design intent.

Status legend:

- `[ ]` not started or not merged
- `[x]` complete in the current branch
- A task annotated `blocked by` must not begin until its dependency gate passes

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

ADRs own the choice and rationale. This plan records implementation mechanisms,
current ABI details, remaining work, and acceptance evidence. Change a decision
through its ADR rather than adding a competing policy to a task description.

## Constraints from the current repository

- Unix-like builds currently define `musikcore` as a shared CMake target.
- Windows currently defines `musikcore` as a static CMake target.
- Core source lists are duplicated between `src/musikcore/CMakeLists.txt` and
  `.cmake/WindowsSources.cmake`.
- The retired legacy C boundary (`c_context.cpp`, `c_interface_wrappers.cpp`,
  and `musikcore_c.h`) has been removed. The new consumer boundary lives in
  `src/musikcore/ffi/consumer`.
- Existing plugins export C-named entry points that exchange C++ SDK pointers.
- CTest now runs header, lifecycle, dispatch, query, wrapper, backend-switch,
  export, and production/remote integration tests.
- Production binaries and plugins currently share source-tree output
  directories in several configurations.

The migration must preserve working production targets until replacement
surfaces cover their consumers.

## Proposed source layout

```text
include/
    musikcore/
        abi.h                   shared C ABI primitives
        musikcore.h             public application umbrella
        context.h               runtime lifecycle and request dispatch
        library.h               queries and metadata pages
        plugin.h                provider/plugin SDK
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
        provider/
            loader.cpp
            host.cpp
            registry.cpp
            provider_lifetime.cpp
            decoder_adapter.cpp
            encoder_adapter.cpp
            stream_adapter.cpp
            output_adapter.cpp
            dsp_adapter.cpp
            tag_adapter.cpp
            indexer_source_adapter.cpp

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
- `ffi/provider` contains the plugin host and private provider adapters.
- Public C++ convenience wrappers are header-only and use public C declarations.
- Temporary library/playback binding adapters remain private and removable
  when public API ownership replaces them.

## CMake target topology

### Before the minimal ABI exists

Preserve the current `musikcore` target behavior. Add testing support without
changing Windows from static to shared.

### Experimental transition topology

Once the shared ABI foundation passes static tests, introduce these conceptual
targets:

```text
musikcore_impl_objects
    |
    +--> musikcore_legacy
    |        Windows: STATIC
    |        Unix:    current internal SHARED behavior
    |
    +--> musikcore_abi0
             all platforms: SHARED
             exports: new C ABI only

musikcore_cpp_abi0 --> INTERFACE --> musikcore_abi0
```

`musikcore_impl_objects` excludes both the legacy C boundary and the new public
façade. It is position-independent where required.

`musikcore_legacy` includes `c_context.cpp`, `c_interface_wrappers.cpp`, and any
other existing boundary sources. Existing application targets continue to link
this target during early migration.

`musikcore_abi0` includes the new public and provider ABI sources. Its output
name is distinct, for example:

- Linux: `libmusikcore-abi0.so`
- macOS: `libmusikcore-abi0.dylib`
- Windows: `musikcore-abi0.dll`

The experimental target is not installed as the final supported artifact.

### Process isolation rule

Implement the single-core-per-process decision in
[ADR-0001](../docs/adr/0001-c-abi-and-core-first-refactor.md) with
`.cmake/VerifyCoreBoundary.cmake` and the positive/negative `link_boundary`
fixtures. Both targets compile the consumer API sources; production uses its
SQLite implementation and extensions, while ABI0 has a strict export allowlist.
The indexed-query suite exercises both targets in separate processes.

### Final topology

After migration:

```text
musikcore_impl_objects --> musikcore SHARED
musikcore_cpp          --> musikcore
musikcube              --> musikcore_cpp
plugins                --> public plugin headers only
```

`musikcore_legacy` and `musikcore_abi0` are removed. The final shared target
uses ABI-major platform naming and installs headers plus package metadata.

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

The target context owns or coordinates the following services; this is the
future runtime layout, not a list of services already exposed by ABI0:

- Runtime/global initialization reference.
- Library/database instance.
- Plugin registry.
- Completion and notification queues.
- Async request registry.
- Playback and indexer services.
- Context-scoped preferences and paths.
- Shutdown state.

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
- Fixed-width row identity and numeric storage.
- One page-owned UTF-8 arena or equivalent compact backing storage.
- Offsets/lengths into the arena.

Reads create new pages rather than mutating published storage. Future
invalidation events can signal database changes; there is no public notification
surface yet. Current lifetime/error details are recorded below and in headers.

### Plugin registry

Each loaded plugin record contains:

- Native library handle.
- Negotiated ABI version and table size.
- Stable plugin ID and metadata.
- Lifecycle state.
- Registered factory records.
- Live provider instance count.
- In-flight call count.

Lifecycle states:

```text
discovered -> loaded -> initialized -> active
                                    -> stopping -> stopped -> unloaded
                         failure --------------------^
```

New factory/instance acquisition stops before shutdown. The native library is
unloaded only after no factories, instances, string views, or in-flight calls
can reference its code or storage.

### Provider adapters

Private adapters implement current internal C++ interfaces by forwarding to C
provider tables. Each adapter:

- Retains the plugin lifetime record.
- Validates required function pointers at construction.
- Translates results and errors.
- Prevents calls after plugin stopping begins.
- Calls the provider's destroy function exactly once.
- Contains exceptions thrown by optional C++ convenience wrappers.

Adapters allow the internal playback/indexer implementation to migrate without
being rewritten at the same time as the plugin ABI.

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

The supported standalone and production-bound surfaces are summarized in the
[HLD](musikcore-c-abi-hld.md#current-experimental-scope). Unsupported operations
reject admission with `MC_ERROR_UNSUPPORTED` and deliver no callback.
`plugin_directory` is copied but does not start a plugin loader.

### Declarations and validation

Public C declarations live in independently compilable `abi.h`, `context.h`,
`library.h`, and `playback.h`; `musikcore.h` is the application umbrella. They
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

Windows UI needs a native PTY backend and validation before its smoke scenarios
can be enabled. Linux execution remains deferred to a separate user-directed
session. Emoji/ZWJ and terminal-specific glyph presentation remain explicit
screen-model limits, not coverage claims. Add further tests alongside the code
being changed; this work does not authorize a broad legacy-test backfill.

## Core C API foundation gate

Status: native macOS and Linux foundation validation complete. The maintainer
approved deferring native Windows acceptance on 2026-11-20 so remote-session
work can proceed using the verified Linux and macOS contracts as interim
evidence. Native Windows acceptance remains a release requirement.

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

- [ ] **FND-WINDOWS** Run clean native Windows Debug/Release tests, verify actual
  DLL exports, import-library loading, and runtime placement. Requires a Windows
  host; project maintainer owns native execution.
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
- [ ] Retire the core `MasterLibrary` adapter once playback and plugins no
  longer consume the legacy `ILibrary` interface.
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

## Foundation validation and baselines

Native evidence on 2026-11-20: macOS 26.6.2, Apple M4/arm64, AppleClang
21.0.0.21000101. The current full Debug and Release app builds and all 44 CTest
entries pass, including four independently selected 1,000-track production UI
scenarios.
Clean standalone ABI-only Debug and Release workflows pass all 26 entries.
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

node tests/scripts/run-tests.js --build-dir build/foundation-debug --config Debug --abi-only
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
  together through v1.
- [x] **DEC-006** Retain CTest, existing named native harnesses, shared C11
  diagnostics, and Node's test runner. ADR-0004 records why a new C++ framework
  is not necessary now; any future framework must be pinned and test-only.
- [ ] **DEC-007** Decide whether raw database access is removed or moved into
  an explicitly unstable extension.

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
- [ ] **TST-007** Add a deterministic memory stream and fake/null output for
  component tests.
  Fake transport and deterministic in-memory WAV generation cover current core
  API tests. An `IDataStream`/provider fixture remains deferred with plugin work.
- [ ] **TST-008** Add sanitizer CMake options for ASan/UBSan without changing
  release defaults.
- [ ] **TST-009** Add an initial internal smoke/unit test to prove CTest on
  Windows, macOS, and Linux.
- [x] **TST-010** Document the standard local configure, build, and CTest
  commands.

#### Phase 0 gate

- [ ] **GATE-000** CTest runs an isolated test on all three platforms while
  Windows musikcube still links the current static core.

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

- [ ] **GATE-100** Shared ABI primitives pass static/internal tests and their
  layouts and ownership rules are approved before any Windows DLL conversion.

### Phase 2: source manifest and experimental shared façade

- [ ] **BLD-001** Move the common core source manifest out of duplicated Unix
  and Windows lists into a shared CMake include.
- [ ] **BLD-002** Separate legacy boundary sources from private implementation
  sources in that manifest.
- [ ] **BLD-003** Introduce `musikcore_impl_objects` or an equivalent private
  reusable target with PIC where required.
- [ ] **BLD-004** Preserve `musikcore_legacy` with current Windows static and
  Unix internal shared behavior.
- [ ] **BLD-005** Relink existing applications against `musikcore_legacy`
  without changing runtime behavior.
- [x] **BLD-006** Add `musikcore_abi0` as a distinct shared target on all
  platforms.
- [x] **BLD-007** Ensure `musikcore_abi0` excludes legacy C boundary sources.
- [x] **BLD-008** Add Windows import/export annotations and import-library
  generation. The ABI0 shared target supplies the import library to
  `musikcore.abi.shared_c`. Native Debug and Release verification is BLD-014.
- [x] **BLD-009** Add ELF version script and macOS exported-symbols list.
- [x] **BLD-010** Add an export allowlist test that rejects private C++ symbols.
  `musikcore.abi.exports` checks the complete ELF and Mach-O export lists.
  Windows export verification remains part of GATE-200.
- [x] **BLD-011** Add a build/link check preventing one executable from linking
  legacy and ABI0 targets together.
  `.cmake/VerifyCoreBoundary.cmake` traverses target dependencies; the positive
  and negative `musikcore.abi.link_boundary` configure fixtures exercise it.
- [x] **BLD-012** Give ABI0 separate runtime output and plugin directories.
  `bin/abi0` and its empty reserved plugin directory are build-local. Standalone
  ABI0 currently does not load plugins; production fixtures stage selected plugins.
- [x] **BLD-013** Add a separately compiled C executable linked only to ABI0.
- [ ] **BLD-014** Test Debug and Release import/shared-library behavior on
  Windows.

#### Phase 2 gate

- [ ] **GATE-200** A separate C process loads `musikcore_abi0` on Windows,
  macOS, and Linux; actual exports match the allowlist; production musikcube
  continues using only the legacy path.

### Phase 3: public runtime walking skeleton

- [x] **API-001** Create `include/musikcore/musikcore.h`.
- [ ] **API-002** Implement `mc_abi_version` and initial feature discovery.
- [x] **API-003** Define context options for ABI version validation,
  dispatch wakeups, query limits, and a reserved copied plugin root. Database
  directories belong to explicit library identities rather than context-global
  data roots. `shared_c` and `arguments` verify these contracts.
- [x] **API-004** Implement transactional `mc_context_create`.
- [x] **API-005** Implement context shutdown states and
  `mc_context_destroy`.
- [x] **API-006** Implement the request ID generator and registry.
- [ ] **API-007** Implement caller-thread completion dispatch and notification polling.
  Completion dispatch is implemented and tested by `dispatch`; notification
  polling is a future feature and must not be inferred from context creation.
- [ ] **API-008** Implement completion payload ownership and owned notifications.
  Current page/result completion ownership is verified by `query` and
  `result_contracts`; owned unsolicited notifications remain unimplemented.
- [x] **API-009** Implement cancellation and exactly-once terminal completion.
- [x] **API-010** Implement wakeup coalescing and dispatcher-thread shutdown draining.
- [x] **API-011** Reject unsupported second contexts according to DEC-003.
- [ ] **API-012** Add C contract tests for version, context, errors, events,
  cancellation, and teardown.
- [ ] **API-013** Add repeated create/destroy and failure-injection tests under
  ASan/UBSan.

#### Phase 3 gate

- [ ] **GATE-300** The C contract executable creates an ABI0 context, completes
  and cancels synthetic requests through callbacks, consumes notifications,
  and shuts down without leaks
  or arbitrary sleeps.

### Phase 4: C SDK walking skeleton

- [ ] **SDK-001** Create `include/musikcore/plugin.h`.
- [ ] **SDK-002** Define the single `mc_plugin_get_api` entry point.
- [ ] **SDK-003** Define sized `mc_host_api` and `mc_plugin_api` tables.
- [ ] **SDK-004** Implement bootstrap logging and host-owned error creation.
- [ ] **SDK-005** Implement ABI negotiation and required-table validation.
- [ ] **SDK-006** Implement plugin records and lifecycle states.
- [ ] **SDK-007** Implement capability/factory registration lifetime rules.
- [ ] **SDK-008** Track in-flight calls and prevent premature unload.
- [ ] **SDK-009** Add the valid pure-C fixture plugin.
- [ ] **SDK-010** Add incompatible, truncated, and initialization-failure
  fixture plugins.
- [ ] **SDK-011** Add contract tests for discovery, negotiation, initialization,
  shutdown, and unload.
- [ ] **SDK-012** Verify fixture plugins include only installed C headers and do
  not link private musikcore targets.

#### Phase 4 gate

- [ ] **GATE-400** ABI0 loads a separately built pure-C plugin, observes a
  registered trivial capability, releases all references, shuts it down once,
  and unloads it safely on all platforms.

### Phase 5: metadata/query public vertical slice

- [x] **LIB-001** Define query options with copied field and filter inputs.
  `musikcore.abi.query` mutates both after submission and verifies the original inputs.
- [ ] **LIB-002** Implement immutable page arena storage.
- [ ] **LIB-003** Implement row identity, string, numeric, and missing-value
  accessors.
- [x] **LIB-004** Implement async query submission and per-request completion callbacks.
- [x] **LIB-005** Implement owned page transfer to the completion callback.
- [x] **LIB-006** Define track identity/list handles without eager metadata.
  Exercised by `musikcore.abi.query` and `musikcore.abi.indexed_query`.
- [x] **LIB-007** Implement async visible-range/cache-window requests.
  Exercised by the same ABI tests, including removal during paging.
- [ ] **LIB-008** Implement metadata state and invalidation events.
- [ ] **LIB-009** Implement batch track-list edits with defined atomicity.
- [ ] **LIB-010** Add C contract tests for paging, ordering, Unicode, long
  strings, snapshots, invalidation, and cancellation.
- [x] **LIB-011** Add SQLite integration tests with isolated databases.
  `musikcore.abi.query`, `musikcore.abi.indexed_query`, and
  `musikcore.abi.library_switch` use fixtures under the build tree.
- [ ] **LIB-012** Add scrolling/cache pressure tests without sleeps.
- [x] **LIB-013** Move dispatch and shutdown to one app-owned context.
  Add an owning C++ wrapper around the library handle and bind search
  results and track references to the originating library. Test switching
  between libraries with a request in flight in `musikcore.abi.library_switch`.
- [ ] **LIB-014** Route Track Search and projected range reads through the
  selected library's typed query dispatcher. Local dispatch is exercised
  through the app binding in `musikcore.abi.indexed_query`. Retain remote
  ordered IDs and album boundaries on the client and fetch pages through
  the legacy typed metadata batch query. Preserve row positions when a
  track is deleted between reads. Server snapshots are deferred.
  The app search projection is wired for both backends. Remote arbitrary
  and multivalued tags still require a projected query.
- [ ] **LIB-015** Add a real loopback contract test with a controllably
  delayed page reply. Race the reply with cancellation, library switching,
  and disconnect. Assert exactly one completion, no stale repaint, and
  no change to the target of a retained track reference. Cover reconnect
  and retained handles from the old connection.
  `musikcore.abi.remote_query_transport` covers the wire and ABI seams,
  including a delayed page across selection and cancellation, missing IDs,
  disconnect with an unanswered read, and rejection of an old reference
  after reconnect. View-level and playback coverage remain separate.
- [ ] **LIB-016** Route playback media through the track's originating
  library and verify that playing from a search position preserves the
  snapshot order. Verify remote search and streaming separately.
  Track Search play-from-row installs the ordered result directly in the
  engine queue. The command requires the result's originating library
  session to still be selected. Single-track queue actions use an origin-bound
  track reference. The playback tests check rejection after a library switch
  and after reconnecting to a remote library that returns the same track ID.
- [ ] **CPP-001** Create initial RAII C++ context, event, request, page, and list
  wrappers.
  Context, query submission, and page ownership are implemented.
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

The pilot resolves a selected row through a move-only `TrackRef`. The
reference owns local library identity and can outlive the search result.
Resolving a hidden or removed track fails with `MC_ERROR_NOT_FOUND`.
The next playback slice needs two distinct operations: act on one track by
identity, and play from a result position in snapshot order.

Production Track Search opens local and remote results through the C++
wrapper in `libmusikcore`. Production and the pilot share
`TrackResultWindow` for paging. `TrackListView` materializes an ordered
legacy list for actions that still require it.

1. Add an async C operation that opens a search result with a stable row
   order, a count, and track identities. Specify the current search modes,
   supported sort orders, and album grouping in the contract. The handle
   owns the result order so a later database update cannot shift page
   offsets. Keep the existing title-query API available for callers that
   only need an ad hoc page.
2. Add async metadata reads by visible range on that result. Return owned
   pages with the requested fields, and preserve row positions when a track
   disappears. Batch metadata reads by ID within a read transaction. A
   refresh opens a new result. Metadata already returned in a page stays
   valid until its owner releases it.
3. Add header-only move-only wrappers for the search result and page. In the
   ABI0-only pilot, cache the current viewport and up to one viewport on
   either side. Show loading rows on misses and keep navigation responsive.
   Keep one window request in flight and retain the latest requested range.
   Cancel obsolete work when the search changes and discard stale
   completions using a search generation. Use the context wakeup hook to
   schedule dispatch on the UI thread.
4. Test scrolling across cold and warm windows, jumping to a distant row,
   rapid filter changes, and index updates during paging. Verify search
   parity against the legacy view before wiring it into production. Move
   playback and menu actions to track identities once their ABI operations
   exist. Production remote Track Search uses the same wrapper with
   the existing search and batch query protocol. Verify reconnect and
   playback against the production server.
5. Benchmark both implementations on the same seeded database. Measure
   search submission to the first usable window and cold-window scrolling
   separately from the keystroke-to-render scenario. Include a broad query
   that crosses several cache windows and report median and p95. Use the
   same debounce, visible rows, result ordering, and projected fields in
   the before/after comparison.

#### Phase 5 gate

- [ ] **GATE-500** The pilot musikcube view uses ABI0 exclusively, scrolling
  remains nonblocking, metadata pages remain stable, and benchmark regression
  is within an agreed threshold.
- [ ] **GATE-510** Production Track Search uses the same wrapper for local
  and remote libraries. The library-switch and loopback contract tests pass
  before migrating other library views.
  [Remote library routing proposal](musikcore-remote-library-proposal.md)

### Phase 6: decoder SDK vertical slice

- [ ] **AUD-001** Implement host audio-buffer creation, mapping, unmapping, and
  recycling.
- [ ] **AUD-002** Define frame counts, capacities, formats, and buffer state
  transitions precisely.
- [ ] **STR-001** Define stream table operations with `uint64_t` positions,
  partial reads, EOF, seek, and interruption.
- [ ] **STR-002** Implement private adapters for core-provided and
  plugin-provided streams.
- [ ] **DCD-001** Define decoder factory capability advertisement.
- [ ] **DCD-002** Define decoder open, decode, seek, duration, preferred rate,
  exhaustion, and destroy operations.
- [ ] **DCD-003** Implement the private decoder adapter to the current playback
  interfaces.
- [ ] **DCD-004** Implement the pure-C deterministic decoder fixture.
- [ ] **DCD-005** Add controlled decoder failure modes selected by fixture URI.
- [ ] **DCD-006** Add SDK contract tests for stream and decoder edge cases.
- [ ] **DCD-007** Run the deterministic decoder through the real playback
  pipeline and fake output.
- [ ] **DCD-008** Add malformed provider results and unload-race tests.
- [ ] **PERF-002** Verify the ABI introduces no PCM copy and no per-sample call.

#### Phase 6 gate

- [ ] **GATE-600** Known PCM frames cross a C decoder provider into the real
  playback pipeline with correct seek, failure, ownership, teardown, and unload
  behavior under sanitizers.

### Phase 7: expand and migrate the public API

- [ ] **PUB-001** Add preference namespace, typed values, batch updates, and
  persistence contracts.
- [ ] **PUB-002** Add category, album, directory, and remaining metadata query
  shapes.
  Browse and Directories use typed ordered results for their track panes.
  Now Playing projects metadata from its queue snapshot using the same paged
  track view. The indexed-query and loopback tests cover order and headers.
- [ ] **PUB-003** Add coherent playback snapshots.
  The transport control surface reads state, position, duration, volume,
  and mute through `mc_playback_get_snapshot`. Category and track lists use
  `mc_playback_get_current_track` with dispatcher notifications. The
  `musikcore.integration.api.production.playback_commands` test checks the
  current track while prepared, playing, paused, and stopped.
- [ ] **PUB-004** Add playback commands and ordered state events.
  The app's transport keys and controls use `mc_playback_control`.
  `musikcore.abi.indexed_query` exercises the bound service with a fake
  transport, including play now, play next, append, prepend, and rejection
  after switching libraries. Live playback subscriptions deliver one event
  kind per callback on the owner thread with optional filters and coalescing
  of repeated kinds. `musikcore.integration.api.production.playback_commands`
  covers independent subscribers, filter ownership, cross-thread replacement
  and unsubscription, in-flight delivery, and scoped teardown.
- [ ] **PUB-005** Add queue snapshots and atomic batch edits.
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
- [ ] **PUB-007** Add output selection, replay gain, equalizer, and transport
  settings required by musikcube.
  Cube reloads output through `mc_playback_control` and reads replay-gain
  availability from the playback snapshot. The
  `musikcore.integration.api.production.playback_commands` test verifies
  both against the engine boundary.
  Output driver and device choices use the C ABI in the existing playback
  overlays. `musikcore.integration.plugin.playback_reload` verifies output
  discovery, driver validation, and fallback during playback reload.
- [ ] **PUB-008** Add advanced consumer stream/decoder/encoder APIs only for
  approved use cases.
- [ ] **PUB-009** Add contract and integration tests with every public feature
  family.
- [ ] **CPP-003** Expand the C++ wrapper with each public feature family.
- [ ] **MIG-003** Migrate preferences and settings consumers.
- [ ] **MIG-004** Migrate remaining library/category/directory views.
- [ ] **MIG-005** Migrate playback and transport consumers.
- [ ] **MIG-006** Migrate queue editing and now-playing consumers.
- [x] **MIG-007** Migrate indexer and local-library settings. Cube uses the SDK
  for indexer commands, directory paths, and dispatch notifications.
- [ ] **MIG-008** Migrate environment/output/equalizer consumers.
- [ ] **MIG-009** Remove private musikcore headers from musikcube targets.
- [ ] **MIG-010** Make the normal migration build link ABI0 only.

#### Phase 7 gate

- [ ] **GATE-700** musikcube builds and passes smoke/integration tests while
  linking only the ABI0 shared library and public C++ wrapper.

### Phase 8: expand and migrate the provider SDK

#### Data streams and encoders

- [ ] **ENC-001** Define blocking encoder output to a host stream.
- [ ] **ENC-002** Define streaming encoder packet/sink ownership.
- [ ] **ENC-003** Implement encoder adapters and pure-C fixtures.
- [ ] **ENC-004** Convert `stockencoders`.
- [ ] **DST-001** Define data-stream factory capability and creation tables.
- [ ] **DST-002** Implement data-stream factory adapters and fixtures.
- [ ] **DST-003** Convert `httpdatastream`.

#### Decoder plugins

- [ ] **PLG-DEC-001** Convert `ffmpegdecoder`.
- [ ] **PLG-DEC-002** Convert `gmedecoder`.
- [ ] **PLG-DEC-003** Convert `libopenmptdecoder`.
- [ ] **PLG-DEC-004** Convert Windows `cddadecoder` and its stream/source
  capabilities.

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
- [ ] **OUT-002** Implement buffer submission/recycle state machine.
- [ ] **OUT-003** Add debug instrumentation for forbidden real-time host calls.
- [ ] **OUT-004** Convert `nullout` as the first production output.
- [ ] **OUT-005** Convert `coreaudioout`.
- [ ] **OUT-006** Convert `alsaout`.
- [ ] **OUT-007** Convert `pipewireout`.
- [ ] **OUT-008** Convert `portaudioout`.
- [ ] **OUT-009** Convert `pulseout`.
- [ ] **OUT-010** Convert `sndioout`.
- [ ] **OUT-011** Convert Windows `directsoundout`.
- [ ] **OUT-012** Convert Windows `wasapiout`.
- [ ] **OUT-013** Convert Windows `waveout`.

#### DSP and remaining plugin categories

- [ ] **DSP-001** Define real-time DSP format negotiation and processing table.
- [ ] **DSP-002** Convert `supereqdsp`.
- [ ] **EXT-001** Design schema/configuration descriptors as data.
- [ ] **EXT-002** Migrate visualizer providers.
- [ ] **EXT-003** Migrate playback-remote providers.
- [ ] **EXT-004** Migrate server integration without crossing C++ interfaces.
- [ ] **EXT-005** Migrate platform hotkey and media-key plugins.
- [ ] **SDK-013** Add contract fixtures and malformed cases for every supported
  provider family.
- [ ] **SDK-014** Add provider-family performance and teardown tests.

#### Phase 8 gate

- [ ] **GATE-800** All shipped plugins use the C SDK or an explicitly approved
  non-plugin integration; no dynamic module returns a private C++ SDK pointer.

### Phase 9: system, packaging, and compatibility

- [x] **SYS-001** Add musikcube pseudo-terminal startup/shutdown smoke test.
  `node tests/ui/scenarios/track-search.js` starts the app in a PTY and quits through
  its normal key binding.
- [ ] **SYS-002** Cover valid, unset, and unparseable `TERM` behavior.
- [ ] **SYS-003** Add musikcubed startup/readiness/shutdown smoke test.
- [ ] **PKG-001** Install headers, shared library, import library, and package
  metadata into a temporary prefix.
- [ ] **PKG-002** Build an external C consumer against the installed prefix.
- [ ] **PKG-003** Build an external C decoder plugin against the installed SDK.
- [ ] **PKG-004** Load and run the installed external plugin.
- [ ] **PKG-005** Validate ELF SONAME/RPATH and symbol versions.
- [ ] **PKG-006** Validate Mach-O install names and exported symbols.
- [ ] **PKG-007** Validate Windows DLL/import-library/plugin placement.
- [ ] **PKG-008** Run consumer, plugin, and application smoke tests from staged
  standalone archives.
- [ ] **PKG-009** Verify packages contain no build-machine absolute paths or
  unexpected dependencies.
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

- [ ] **FIN-001** Confirm musikcube and musikcubed have no private core link
  dependencies.
- [ ] **FIN-002** Confirm every shipped plugin has left the C++ SDK ABI.
- [x] **FIN-003** Remove or archive `musikcore_c.h`, `c_context.cpp`, and
  `c_interface_wrappers.cpp` after their consumers are gone. The fixture
  seeder uses the internal indexer and is exercised by `musikcore.abi.indexed_query`.
- [ ] **FIN-004** Remove obsolete C++ SDK interfaces from installed/public
  include paths.
- [ ] **FIN-005** Add a maintained example using the new C API to replace the
  retired `core_c_demo`.
- [ ] **FIN-006** Remove `musikcore_legacy`.
- [ ] **FIN-007** Rename `musikcore_abi0` to the final `musikcore` target and
  output names.
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

| Bridge | Remove when |
| --- | --- |
| `src/musikcore/ffi/consumer/legacy-library-selection.*`, `library-binding.h` and `legacy-library-binding.cpp`, plus `MasterLibrary::OpenApiLibrary()` | The public library API creates and owns local and remote backends directly. The app now receives its selection through the context. `musikcore.abi.remote_query_transport` covers context selection on switch, disconnect, and reconnect. Retained requests, results, and references must still preserve their originating backend and connection generation. |
| `src/musikcore/ffi/consumer/playback-binding.h` and `legacy-playback-binding.cpp`, plus the friendship and lifetime token in `PlaybackService` | The public playback API creates and owns the session directly. Carry forward the owner-thread rules, queue origin checks, and dispatcher notifications. |
| `TrackListView::GetTrackList()` and legacy track and playlist action overlays | Hot-swap, playlist drafts, and the remaining overlays can consume ordered results and track references directly. |

#### Final gate

- [ ] **GATE-1000** ABI v1 is released only after all prior gates pass and the
  previous-boundary implementations are absent from shipped artifacts.

## Active next steps

1. Move app selection and playback from `MasterLibrary` to the public library
   selector. Finish native Windows acceptance before release.
2. Finish the core application API and its real consumers: the remaining
   runtime/query gaps, `PUB-*`, `CPP-003`, and `MIG-003` through `MIG-010`.
   Remove temporary bindings only when public ownership replaces them.
3. Pass `GATE-700` with musikcube using only the C API and public header-only
   wrapper. Validate core consumer installation, runtime paths, dependencies,
   and application packages on supported platforms to reach core
   release-candidate quality. Plugin-specific package tasks remain deferred.
4. Then begin the provider walking skeleton and SDK migration tasks. Finish
   combined packaging/compatibility gates before the final ABI v1 freeze.

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
