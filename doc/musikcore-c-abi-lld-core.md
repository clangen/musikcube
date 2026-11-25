# C ABI LLD: core migration tasks

Status: **complete** in the recorded scope. Deferred items are identified as
such, and application media API work continues in the plugin LLD.

[LLD index](musikcore-c-abi-lld.md) · [SDK removal design](musikcore-sdk-removal-design.md)

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
  Request completions use callbacks. Unsolicited-notification polling is not
  in the current scope and is not implemented by the current headers.
- [x] **DEC-005** Version the application and plugin ABIs independently.
  Changes to one boundary do not by themselves advance the other; see
  [ADR-0016](../docs/adr/0016-independent-application-and-plugin-abi-versions.md).
  Both versions remain experimental until their respective freeze gates.
- [x] **DEC-006** Retain CTest, existing named native harnesses, shared C11
  diagnostics, and Node's test runner. ADR-0004 records why a new C++ framework
  is not necessary now; any future framework must be pinned and test-only.
- [x] **DEC-007** Keep raw database handles and SQL out of the public API.
  Typed library operations abstract local and remote storage, allowing database
  implementation and public API to evolve independently. See
  [ADR-0015](../docs/adr/0015-database-behind-library-api.md).

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

- [x] **BLD-001** Move the common core source manifest out of duplicated Unix
  and Windows lists into a shared CMake include. Both `src/musikcore/CMakeLists.txt`
  and `.cmake/BuildWindows.cmake` use `musikcore_sources` from
  `.cmake/MusikcoreSources.cmake`. Native Windows revalidation is separate.
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

- [x] **GATE-200** A separate C process loads `musikcore_abi0` on Windows,
  macOS, and Linux, and actual exports match the allowlist. Production
  musikcube links its production shared core separately from ABI0.
  The foundation gate records native platform acceptance; `musikcore.abi.shared_c`,
  `musikcore.abi.exports`, and `musikcore.abi.link_boundary` also pass in the
  current Linux build. Windows evidence includes BLD-014 and FND-WINDOWS.

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
  Polling and its tests are deferred under API-014/API-015.
- [x] **API-013** Instrument and validate direct ABI lifecycle/allocation-failure
  paths on native macOS with ASan/UBSan, including `dispatch`, `result_contracts`,
  `shared_c`, and `subscriptions`. Full Debug and ABI-only Debug suites pass;
  actual C/C++ compile and driver-link flags contain the selected sanitizers.
  TST-008's negative probes prevent a plain Debug build from silently claiming
  instrumentation. Native Linux/Windows acceptance for this change stays open.
- [x] **API-014** Deferred by maintainer decision: do not introduce unsolicited
  notification polling now. Host-dispatched callbacks serve current consumers;
  no polling API is implemented or required for this release.
- [x] **API-015** Deferred with API-014: owned polled notification payloads and
  their lifecycle tests are outside the current scope; none are implemented.

#### Phase 3 gate

- [x] **GATE-300** Current create, dispatch/cancel, callback ownership, and
  shutdown contracts pass `shared_c`, `dispatch`, and `result_contracts` with
  native macOS ASan/UBSan instrumentation (API-013). Cross-platform acceptance
  remains in the foundation gate, where native Windows acceptance is complete;
  this result does not claim new Linux/Windows sanitizer execution. Polled
  notifications remain separately deferred under API-014/API-015.

### Phase 5: metadata/query public vertical slice

- [x] **LIB-001** Define query options with copied field and filter inputs.
  `musikcore.abi.query` mutates both after submission and verifies the original inputs.
- [x] **LIB-002** Deferred by maintainer decision: retain immutable page-owned
  vectors. Current `ProjectedTrackPage` storage and stable string views pass
  `result_contracts` and the recorded allocation/cache budgets. Reconsider a
  packed arena only if profiling identifies a need; none is implemented here.
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
- [x] **LIB-008** Deferred by maintainer decision: general track-metadata
  state/invalidation events are outside the current scope and not implemented.
  Selection, playlists, indexer, and playback already have callbacks; published
  pages remain immutable and later reads can observe current metadata.
- [x] **LIB-009** Deferred by maintainer decision: atomic batch track-list edits
  are outside the current scope. Ordered queue edits and playlist writes are
  implemented under PUB-005; atomic batch edits have not been implemented.
- [x] **LIB-010** Cover current paging, ordering, Unicode, long strings,
  immutable pages, retained results, missing rows, and cancellation in
  `musikcore.abi.query`, `result_contracts`, and `library_switch`.
  If general invalidation events are added later, test their contract then.
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
  typed library queries. Remote batch projections retain arbitrary and
  multivalued tags and NULL/empty distinctions. `musikcore.abi.remote_query_transport` and
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
  consumers. Requests use IDs; polled-event wrappers are outside current scope.
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
service. Plugins use the C provider ABI and are built with the same release.

- [x] **AUD-001** Implement decoder scratch storage and fixed playback-pool
  handoff/recycling.
  - [x] Implement opaque growable `mc_audio_scratch_buffer` storage through
    `mc_audio_scratch_buffer_api`, reserve/format changes, exclusive
    `begin_write`/`end_write`, and read-only PCM publication.
    `musikcore.unit.plugins.audio_buffers` checks frame/sample units, overflow,
    failed/abandoned writes, nested-access rejection, copied-view isolation, and
    same-channel capacity reuse, and no-op/failed reserve preservation.
    `musikcore.unit.plugins.contract_headers` and
    independent C11/C++17 translation units cover audio/output declarations.
  - [x] Connect decoder scratch to a private, contiguous fixed playback pool.
    Decoded blocks are split across slices without growing playback storage.
    Pool exhaustion, backpressure, and exactly-once recycling are covered by
    the output and transport tests; the C output handoff is covered by OUT-002. C scratch feeds the fixed playback pool
    (`transport.decoder_contract_pcm`, `transport.gapless_pcm`, and
    `transport.completion_pcm`). A change of channel count or sample rate
    during one track ends playback with an error; mid-track pool replacement
    is not part of the current playback scope.
- [x] **AUD-002** Define frame counts, capacities, formats, and buffer state
  transitions precisely in `plugin/audio.h` and `plugin/output.h`.
  `plugins.audio_buffers`, `plugins.contract_headers`, and `plugins.null_output`
  verify the implemented contracts on macOS, including host-sized returned
  format prefixes and permanent oversized-submission errors versus backpressure.
  Native-platform follow-ups remain SDK-LINUX/SDK-WINDOWS. The production
  decoder-to-output path is exercised by GATE-600; mid-track pool replacement
  is outside the current playback scope.
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
- [x] **DCD-SEEK** Retired from the current playback scope by the maintainer.
  FFmpeg reports the requested logical position after the demuxer accepts a
  seek; sample-accurate first PCM timestamps have not been established and are
  not promised by this decision.
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
  `musikcore.integration.api.remote_facets` verifies native contextual facet
  queries, cancellation, failure, and reconnection over the remote transport.
  Clients and servers update together for the 4.0 release; older servers without
  the native facet query are not supported by this path.
- [x] **FACET-COMPAT-REMOVAL** Remove the legacy multi-query facet composition
  and capability routing. Remote facets use one native `FacetListQuery`;
  `api.remote_facets` verifies it is used without capability advertisement.
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
  Explicit pause and resume commands are idempotent: pause affects playing
  transport, resume affects paused or prepared transport, and stopped playback
  stays stopped. `musikcore.integration.api.managed_runtime` verifies repeated
  commands on the production playback path; `plugins.mpris` checks that the
  D-Bus methods submit these commands through the playback-remote host.
  `transport.gapless_control_failure_pcm` and
  `transport.crossfade_control_failure_pcm` verify that device failures reach
  the caller without falsely changing playback state.
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
The open **PUB-008** media-consumer API work is tracked in the
[plugin LLD](musikcore-c-abi-lld-plugins.md#application-media-apis).
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
  The remote settings include an opt-in for untrusted HTTPS certificates;
  `musikcore.integration.runtime.local_startup` checks the default and the
  resulting remote track policy.
  The FFmpeg, GME, libopenmpt, CD Audio, TagLib, SuperEq, and HTTP stream
  provider display names no longer contain legacy interface names. Their
  plugin-scoped preferences use the new names; existing settings under the old
  names are not migrated automatically. Indexer identity keys remain unchanged.
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
  uses a headless C provider to check hide/release and disabled-provider
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
  still writes correctly, and the retired C++ entry point is never called. Real GME
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
- [x] **API-URI-007** Implement Windows CDDA device cancellation. The C stream
  uses overlapped device requests and `CancelIoEx`, and the physical-drive test
  covers pre-open and concurrent read interruption. The maintainer accepts the
  remaining deliberately blocked Stop and disc-removal hardware checks as
  deferred; those scenarios were not verified by this decision and cancellation
  depends on the device driver.

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
- [x] **RTH-004** Retire provider work and drain accepted callbacks at shutdown.
  `musikcore.integration.runtime.caches` checks callback completion and stale
  handles. Playback remote startup rollback, stop ordering, and fatal provider
  exceptions have separate C provider tests. The retired C++ detachment-hook
  failure test no longer applies because those hooks were removed.
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

