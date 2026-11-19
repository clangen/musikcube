# musikcore C ABI modernization: low-level design and task plan

Status: proposed

## Purpose

This document translates the
[high-level design](musikcore-c-abi-hld.md) into concrete build targets, source
layout, runtime components, state machines, test fixtures, migration steps, and
trackable tasks.

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

## Constraints from the current repository

- Unix-like builds currently define `musikcore` as a shared CMake target.
- Windows currently defines `musikcore` as a static CMake target.
- Core source lists are duplicated between `src/musikcore/CMakeLists.txt` and
  `.cmake/WindowsSources.cmake`.
- The legacy C boundary is implemented primarily in `c_context.cpp` and
  `c_interface_wrappers.cpp`.
- Existing plugins export C-named entry points that exchange C++ SDK pointers.
- No CTest tree or automated unit/integration suite currently exists.
- Production binaries and plugins currently share source-tree output
  directories in several configurations.

The migration must preserve working production targets until replacement
surfaces cover their consumers.

## Proposed source layout

```text
include/
    musikcore/
        abi.h                   shared C ABI primitives
        musikcore.h             public application API
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
        cpp/
            context.cpp         only where wrapper cannot be header-only
            event_dispatcher.cpp

tests/
    support/
    api/
    sdk/
    sdk/plugins/
    unit/
    integration/
    system/
    packaging/
    fixtures/
```

The exact file count may be reduced during implementation. The important
separation is:

- Installed C headers do not include private headers.
- `ffi/common` owns behavior shared by both directions of the C boundary.
- `ffi/consumer` contains the application-facing façade, exception boundaries,
  and validation.
- `ffi/provider` contains the plugin host and private provider adapters.
- `ffi/cpp` contains optional convenience-wrapper implementation that cannot be
  header-only.
- Legacy C API sources remain separate and removable.

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

musikcore_cpp_abi0 --> INTERFACE or small wrapper library --> musikcore_abi0
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

No executable may link `musikcore_legacy` and `musikcore_abi0` together. The
build should enforce this through separate targets and link dependencies. A
debug runtime guard should also detect a second process-wide runtime instance
if feasible.

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

`mc_context` owns or coordinates:

- Runtime/global initialization reference.
- Library/database instance.
- Plugin registry.
- Event queue.
- Async request registry.
- Playback and indexer services.
- Context-scoped preferences and paths.
- Shutdown state.

Initial multiple-context behavior should be explicit. If the current singleton
implementation cannot isolate two contexts, a second create returns
`MC_ERROR_BUSY` rather than silently sharing mutable state.

Context lifecycle states:

```text
constructing -> running -> shutting_down -> stopped -> destroyed
                    |             ^
                    +-- failure --+
```

Creation publishes the handle only after all required services initialize.
Shutdown stops accepting requests, cancels or drains in-flight work, emits
terminal events, wakes the event consumer, releases providers, and then permits
destruction.

### Request registry

Each accepted async operation receives a nonzero `mc_request_id` and one state:

```text
accepted -> queued -> running -> completed
                   \-> cancelled
                   \-> failed
```

Rules:

- Exactly one terminal event is observable per accepted request.
- Cancellation is best effort and never creates a second terminal event.
- Request IDs are not reused while their completion can remain queued.
- Input strings and slices are copied or retained before the initiating call
  returns.
- Context shutdown transitions every accepted request to a terminal outcome.

### Event queue

The first ABI supports one active consumer per context. Producers may be
concurrent.

Events are divided into:

- Lossless terminal events.
- Lossless state transitions.
- Coalescible progress or invalidation events.

The queue must not invoke application code. `mc_context_next_event` transfers
an owned event to the caller. Event payloads use explicit `take` functions;
destroying the event releases any payload not taken.

Shutdown wakes a blocked consumer and produces a documented stopped or closed
result after queued terminal events are handled.

### Error objects

`mc_error` is core-owned. Synchronous public functions optionally return it on
failure. Async events initially own async errors.

Plugins create detailed errors through `mc_host_api::create_error`; they never
allocate an `mc_error` directly.

Every exported C function catches all C++ exceptions and translates them to a
result and optional error. Allocation failure has a defined fallback result
even if a detailed error object cannot be created.

### Metadata pages

`mc_track_page` is immutable after publication. It contains:

- Row count.
- Requested field descriptors or stable field ordering.
- Fixed-width row identity and numeric storage.
- One page-owned UTF-8 arena or equivalent compact backing storage.
- Offsets/lengths into the arena.

Views returned from a page remain valid until page destruction. Database
changes produce invalidation events or new pages; they do not mutate published
pages.

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

## Task list

### Documentation and decisions

- [x] **DOC-001** Write the detailed public C API and C SDK design.
- [x] **DOC-002** Write the boundary-first unit and integration testing design.
- [x] **DOC-003** Document the static-first high-level architecture and
  migration sequence.
- [x] **DOC-004** Create this low-level design and trackable task plan.
- [ ] **DEC-001** Approve the `mc_` public prefix and installed header names.
- [ ] **DEC-002** Approve the experimental shared-library name
  `musikcore-abi0` or select a replacement.
- [ ] **DEC-003** Decide whether initial multiple-context creation returns
  `MC_ERROR_BUSY` or supports isolated contexts.
- [ ] **DEC-004** Confirm one active event consumer per context for ABI v1.
- [ ] **DEC-005** Decide whether public and provider ABI versions advance
  together through v1.
- [ ] **DEC-006** Select and pin the internal C++ unit-test framework.
- [ ] **DEC-007** Decide whether raw database access is removed or moved into
  an explicitly unstable extension.

#### Decision gate

All decisions that affect layouts, target names, and lifecycle semantics are
recorded before the experimental shared façade is introduced.

### Phase 0: testing foundation without changing linkage

- [ ] **TST-001** Add `include(CTest)` and `BUILD_TESTING` handling to the
  Unix-like root build.
- [ ] **TST-002** Add equivalent CTest registration to the Windows build path
  before its early return.
- [ ] **TST-003** Create `tests/CMakeLists.txt` and standard CTest labels.
- [ ] **TST-004** Add the minimal C11 harness under `tests/support/c_test.h`.
- [ ] **TST-005** Add cross-platform temporary-directory support that never
  uses production user data.
- [ ] **TST-006** Add deadline-based event/predicate waiting support with
  timeout diagnostics.
- [ ] **TST-007** Add a deterministic memory stream and fake/null output for
  component tests.
- [ ] **TST-008** Add sanitizer CMake options for ASan/UBSan without changing
  release defaults.
- [ ] **TST-009** Add an initial internal smoke/unit test to prove CTest on
  Windows, macOS, and Linux.
- [ ] **TST-010** Document the standard local configure, build, and CTest
  commands.

#### Phase 0 gate

- [ ] **GATE-000** CTest runs an isolated test on all three platforms while
  Windows musikcube still links the current static core.

### Phase 1: shared ABI foundation, still tested without a new DLL

- [ ] **ABI-001** Create `include/musikcore/abi.h` with C11/C++ guards.
- [ ] **ABI-002** Implement `MC_API`, `MC_PLUGIN_API`, and `MC_CALL` for all
  supported compilers.
- [ ] **ABI-003** Define ABI major/minor encoding and runtime version helpers.
- [ ] **ABI-004** Define fixed-width `mc_result`, error codes, booleans, and ABI
  enum conventions.
- [ ] **ABI-005** Define `mc_string_view`, byte slices, and caller-buffer
  required-size semantics.
- [ ] **ABI-006** Define sized-structure initialization and compatibility
  helpers.
- [ ] **ABI-007** Define opaque-handle ownership and pointer-to-pointer destroy
  conventions.
- [ ] **ABI-008** Implement core-owned error objects with allocation-failure
  fallback behavior.
- [ ] **ABI-009** Add exception barriers and argument-validation helpers used by
  every public entry point.
- [ ] **ABI-010** Define common audio format and buffer-view types without
  implementing audio providers yet.
- [ ] **ABI-011** Add C11 and C++ header compile tests.
- [ ] **ABI-012** Add unit tests for strings, errors, versions, sized
  structures, and ownership helpers.
- [ ] **ABI-013** Verify no public header exposes `long`, STL types, C++
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
- [ ] **BLD-006** Add `musikcore_abi0` as a distinct shared target on all
  platforms.
- [ ] **BLD-007** Ensure `musikcore_abi0` excludes legacy C boundary sources.
- [ ] **BLD-008** Add Windows import/export annotations and import-library
  generation.
- [ ] **BLD-009** Add ELF version script and macOS exported-symbols list.
- [ ] **BLD-010** Add an export allowlist test that rejects private C++ symbols.
- [ ] **BLD-011** Add a build/link check preventing one executable from linking
  legacy and ABI0 targets together.
- [ ] **BLD-012** Give ABI0 separate runtime output and plugin directories.
- [ ] **BLD-013** Add a separately compiled C executable linked only to ABI0.
- [ ] **BLD-014** Test Debug and Release import/shared-library behavior on
  Windows.

#### Phase 2 gate

- [ ] **GATE-200** A separate C process loads `musikcore_abi0` on Windows,
  macOS, and Linux; actual exports match the allowlist; production musikcube
  continues using only the legacy path.

### Phase 3: public runtime walking skeleton

- [ ] **API-001** Create `include/musikcore/musikcore.h`.
- [ ] **API-002** Implement `mc_abi_version` and initial feature discovery.
- [ ] **API-003** Define `mc_context_options` with explicit data and plugin
  roots.
- [ ] **API-004** Implement transactional `mc_context_create`.
- [ ] **API-005** Implement context shutdown states and
  `mc_context_destroy`.
- [ ] **API-006** Implement the request ID generator and registry.
- [ ] **API-007** Implement the single-consumer event queue.
- [ ] **API-008** Implement owned event objects and take-or-destroy payloads.
- [ ] **API-009** Implement cancellation and exactly-once terminal completion.
- [ ] **API-010** Wake blocked event consumers during shutdown.
- [ ] **API-011** Reject unsupported second contexts according to DEC-003.
- [ ] **API-012** Add C contract tests for version, context, errors, events,
  cancellation, and teardown.
- [ ] **API-013** Add repeated create/destroy and failure-injection tests under
  ASan/UBSan.

#### Phase 3 gate

- [ ] **GATE-300** The C contract executable creates an ABI0 context, completes
  and cancels synthetic requests, consumes events, and shuts down without leaks
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

- [ ] **LIB-001** Define query options with copied field and filter inputs.
- [ ] **LIB-002** Implement immutable page arena storage.
- [ ] **LIB-003** Implement row identity, string, numeric, and missing-value
  accessors.
- [ ] **LIB-004** Implement async query submission and completion events.
- [ ] **LIB-005** Implement page payload transfer from events.
- [ ] **LIB-006** Define track identity/list handles without eager metadata.
- [ ] **LIB-007** Implement async visible-range/cache-window requests.
- [ ] **LIB-008** Implement metadata state and invalidation events.
- [ ] **LIB-009** Implement batch track-list edits with defined atomicity.
- [ ] **LIB-010** Add C contract tests for paging, ordering, Unicode, long
  strings, snapshots, invalidation, and cancellation.
- [ ] **LIB-011** Add SQLite integration tests with isolated databases.
- [ ] **LIB-012** Add scrolling/cache pressure tests without sleeps.
- [ ] **CPP-001** Create initial RAII C++ context, event, request, page, and list
  wrappers.
- [ ] **CPP-002** Add C++ wrapper tests that use no private core headers.
- [ ] **MIG-001** Select one musikcube search/list vertical slice for the pilot.
- [ ] **MIG-002** Migrate that slice through the C++ wrapper in an ABI0-only
  migration executable.
- [ ] **PERF-001** Add baseline and ABI0 benchmarks for query, initial display,
  and rapid scrolling.

#### Phase 5 gate

- [ ] **GATE-500** The pilot musikcube view uses ABI0 exclusively, scrolling
  remains nonblocking, metadata pages remain stable, and benchmark regression
  is within an agreed threshold.

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
- [ ] **PUB-003** Add coherent playback snapshots.
- [ ] **PUB-004** Add playback commands and ordered state events.
- [ ] **PUB-005** Add queue snapshots and atomic batch edits.
- [ ] **PUB-006** Add indexer paths, commands, snapshots, progress, and terminal
  events.
- [ ] **PUB-007** Add output selection, replay gain, equalizer, and transport
  settings required by musikcube.
- [ ] **PUB-008** Add advanced consumer stream/decoder/encoder APIs only for
  approved use cases.
- [ ] **PUB-009** Add contract and integration tests with every public feature
  family.
- [ ] **CPP-003** Expand the C++ wrapper with each public feature family.
- [ ] **MIG-003** Migrate preferences and settings consumers.
- [ ] **MIG-004** Migrate remaining library/category/directory views.
- [ ] **MIG-005** Migrate playback and transport consumers.
- [ ] **MIG-006** Migrate queue editing and now-playing consumers.
- [ ] **MIG-007** Migrate indexer and local-library settings.
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

- [ ] **SYS-001** Add musikcube pseudo-terminal startup/shutdown smoke test.
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
- [ ] **FIN-003** Remove or archive `musikcore_c.h`, `c_context.cpp`, and
  `c_interface_wrappers.cpp` after their consumers are gone.
- [ ] **FIN-004** Remove obsolete C++ SDK interfaces from installed/public
  include paths.
- [ ] **FIN-005** Convert `core_c_demo` into a maintained example using the new
  API or remove it after equivalent examples exist.
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

#### Final gate

- [ ] **GATE-1000** ABI v1 is released only after all prior gates pass and the
  previous-boundary implementations are absent from shipped artifacts.

## Recommended first implementation batch

The first batch should stop before creating a shared Windows target:

1. `TST-001` through `TST-010`.
2. `ABI-001` through `ABI-013`.
3. Resolve `DEC-001` through `DEC-006` as needed by those headers.
4. Pass `GATE-000` and `GATE-100`.

The second batch introduces the experimental shared boundary:

1. `BLD-001` through `BLD-014`.
2. `API-001` through `API-013`.
3. Pass `GATE-200` and `GATE-300`.

This ordering ensures the first Windows DLL is built around an intentional C
contract and explicit ownership rules rather than exporting the current C++
surface.

## Progress reporting

When completing a task:

1. Check its box in this document in the same commit as the implementation.
2. Add or update the named contract/unit/integration test.
3. Record any change to an approved decision under the task that required it.
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
