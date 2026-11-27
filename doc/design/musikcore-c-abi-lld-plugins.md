# C ABI LLD: plugin migration tasks

[LLD index](musikcore-c-abi-lld.md) · [SDK removal design](musikcore-sdk-removal-design.md)

## Plugin architecture and provider tasks

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

The following historical cutover map records the original caller groups, not
current source paths or remaining interfaces. The [SDK removal design](musikcore-sdk-removal-design.md)
contains the current inventory. No old-interface adapter is authorized.

| Caller group | Current dependency | Replacement work |
| --- | --- | --- |
| `runtime/ManagedRuntime`, `plugin/Plugins`, `plugin/PluginFactory` | Old discovery, process registry, session flags, and `Get*`/`Set*` hooks | Retain explicit context directory/service selection; use only `PluginRegistry`, then remove old discovery and global service setters. |
| `support/Settings`, plugin catalog/settings C entry points | `ISettings`, `ISchema`, plugin-name stores, reload calls | Use scoped setting/configuration tables. Decide persistence identity before changing filenames; render data descriptors without plugin-owned UI. |
| `io/DataStreamFactory`, `io/LocalFileStream`, `audio/Streams`, `audio/Stream` | `IDataStream`, decoder/encoder factories, DSP enumeration and caches | Use C stream/factory contracts and engine-owned instances. Preserve create-before-open publication and interruption; absent families report unsupported or bypass DSP. |
| `audio/Buffer`, `audio/Player`, `audio/Outputs`, transport headers, `audio/Crossfader` | SDK buffer/provider/output types and callbacks | Use opaque host buffers, typed output factories, explicit submission tokens, and core-owned playback state. Discover the bundled null-output DLL through the same loader as native outputs. |
| `audio/PlaybackService`, `audio/Visualizer` | Playback-remotes, visualizer interfaces, borrowed playback-service pointers | Deliver owner-thread C snapshots and bounded read-only PCM/spectrum views. Playback remotes stop and join any workers they own. |
| `library/Indexer` | Tag readers, decoder factories, indexer sources, analyzers, SDK metadata writers | Use source-scoped C writers and typed factories. Keep checkpoint/rollback semantics; resolve source-ID and timestamp persistence before conversion. |
| `library/ILibrary`, `library/LocalMetadataProxy`, `library/track/*`, `library/metadata/*` | SDK track/list/map/editor interfaces | Keep concrete metadata storage private to the core; expose existing C identities/results/pages through typed host calls. Do not recreate the exported C++ object graph. |
| `library/query/*`, including `util/SdkWrappers` and `TrackQueryFragments` | SDK metadata wrappers and shared tag/category constants | Replace old interface results with private core data or existing C result types. Preserve typed backend routing and bound SQL; keep SQL in local query implementations. |
| `support/Common`, `support/Playback`, `support/LastFm`, `support/Auddio`, `i18n/Locale`, `net/WebSocketClient`, `ffi/consumer/remote-session`, `pch.hpp` | SDK filesystem, strings, HTTP helpers, constants, and transitive includes | Extract reusable implementation helpers into private core/common headers. These are not new C ABI families. Remove transitive SDK includes after direct callers move. |

`runtime.stream_sdk_contract` exercises C provider discovery and stream
create/open/cancel contracts. A sentinel module verifies that discovery never
calls the retired C++ `GetPlugin()` entry point.

1. **Draft all contracts before implementing families.**
   - [x] **PLG-CONTRACT-001** Inventory every existing SDK interface/export and
     engine caller. Map each to a C provider table, a narrow C host service, an
     existing public C API operation, an internal-only helper, or deliberate
     removal. Document differences from the C++ behavior and affected callers.
     The historical mapping above and SDK removal design record the disposition;
     in-tree providers use the C boundary and the retired interfaces are removed.
   - [x] **PLG-CONTRACT-002** Define provisional C provider shapes. The
     family tables and defaults are present under `include/musikcore/plugin/`;
     `plugins.contract_headers` checks their C/C++ declarations. Individual
     Output timing, native validation and ABI freeze remain tracked under
     OUT-001, SDK-LINUX/SDK-WINDOWS and phase 10.
     The former blanket contract-review task is retired; targeted malformed
     provider coverage is recorded under SDK-013. Existing lifecycle tests and
     playback benchmarks cover teardown and performance; additional tests should
     address concrete gaps. This does not claim an independent review of every
     interface.
2. **Replace each family's engine path with the new loading contract.**
   The output-first staging is complete. All in-tree providers use the C plugin
   boundary; no former C++ provider path is retained. Native-platform and
   physical-device validation remain in the family tasks and platform gates.
   - [x] **PLG-CUTOVER-001** Remove the former plugin discovery and startup
     path, factory, provider interfaces, and `Get*`/`Set*` hook wiring.
     `runtime.stream_sdk_contract` and `plugins.playback_remote_lifecycle`
     include a legacy-entry sentinel; `plugins.playback_remote_stop_order`
     covers C remote shutdown. SDK-era internal interfaces and the boundary
     guard are covered by SDK-REMOVE-001 through SDK-REMOVE-007; GATE-800
     records the completed cutover.
   - [x] **PLG-LOADER-001** Consolidate loading and engine-session ownership in
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
     - [x] Add typed lookup for the supported family tables and remove the
       old `PluginFactory` production path. `plugins.discovery`,
       `plugins.lifecycle`, `plugins.runtime`, and `plugins.installed` passed
       in the current Linux integration run. Native Windows validation remains
       separately open under SDK-WINDOWS.
   - [x] **PLG-LOADER-002** Ensure failed discovery/negotiation/initialization
     leaves no visible registrations and does not prevent other plugins loading.
     Make dependency injection explicit; no global `SetEnvironment` replacement
     that exposes the entire engine as one untyped service object. Supply typed
     services at instance creation, with playback remotes started only after
     required services are ready. Prove stop/cancel/owner-dispatch-drain/join/
     instance-destruction ordering before final registry shutdown/unload, including
     workers holding peer-provider objects and awaiting host completions.
     `plugins.lifecycle`, `plugins.discovery`, `plugins.playback_remote_lifecycle`,
     `plugins.playback_remote_stop_order`, and
     `plugins.playback_remote_startup_rollback` exercise these paths.
3. **Make empty optional families valid.**
   - [x] **PLG-EMPTY-001** Start and stop the engine with no optional plugins.
     `plugins.empty_session` exercises a real playback context with an empty
     plugin directory, no output drivers, idle dispatch and clean shutdown;
     `plugins.discovery` covers an empty registry. Missing stream/decoder and
     unsupported operation cases retain their existing family tests.
   - [x] **PLG-EMPTY-002** Restore tests deferred during the output-first
     migration. The output integration restored playback, runtime, remote
     audio/transcoding, URI-player, installed-consumer, and TUI scenarios;
     none remain disabled for that cutover. Later test failures and platform
     acceptance are tracked independently, not treated as passing here.
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
       factory. Saved device settings, no-schema settings, plugin enablement,
       reload, and missing-output behavior use the actual engine path. Restore
       the playback/UI and installed-consumer scenarios listed below.
   - [x] **PLG-NULL-002** No additional fallback is needed. The core-owned
     local-file stream remains available, while missing decoder or encoder
     providers report unsupported. DSP and integrations may be absent;
     `nullout` remains a loadable output plugin.
5. **Prove one representative of every supported family.**
     The former requirement for one combined production-decoder/`nullout`
     test is retired. `decoder_contract_pcm` and `decoder_drain_pcm` check known
     PCM, EOF and failures with a deterministic decoder and capture output;
     `api.remote_audio` exercises the production decoder and `nullout` through
     the application API. These are separate tests, not one combined proof.
   - [x] **PLG-PROVE-002** Record representative family coverage.
     `runtime.stream_sdk_contract` and `plugins.decoder_stream` cover streams
     and decoders; `plugins.encoder`, `plugins.taglib_metadata`, and
     `plugins.indexer_gme` cover encoding, tags and sources. `plugins.null_output`,
     `plugins.dsp_processing`, `plugins.analyzer`, and `runtime.visualizer_session`
     cover the corresponding audio families. `plugins.playback_remote_lifecycle`,
     `api.uri_player`, and `api.remote_audio` cover remotes, configuration and
     server integration. Additional malformed-provider and teardown cases remain
     under SDK-013/014; the family migrations themselves are complete.
6. **Backfill remaining plugins and remove the disconnected code.**
   - [x] **PLG-BACKFILL-001** Convert the in-tree plugin implementations to
     the C provider ABI and remove their former provider implementations.
     This records code conversion, not completion of the remaining Phase 8
     device, lifecycle, failure, or native-platform validation.
   - [x] **PLG-BACKFILL-002** Remove old SDK headers, factories, adapters, exports,
     targets, install rules, and old-only fixtures after their replacements have
     meaningful regression coverage. Removal and boundary checks are complete;
     remaining acceptance is tracked under SDK-REMOVE-007, PKG-003/004 and GATE-800.

#### Output integration mechanics

Each transport owns its joinable track jobs ([ADR-0008: Playback threading model](../adr/0008-playback-threading-model.md)).
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
no natural Finished/automatic-next event. C playback remote tests cover startup
rollback, stop ordering, and fatal exceptions at provider boundaries.

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
  and playback-remote teardown remain covered by the relevant lifecycle tasks
  below; internal SDK cleanup is tracked under SDK-REMOVE-001 through
  SDK-REMOVE-007.
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
[ADR-0007](../adr/0007-c-provider-bootstrap-and-unload-lifetime.md).

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
- [x] **SDK-LINUX** Run lifecycle, installed-provider/export checks, shared-runtime
  restart, and ASan/UBSan on native Linux. Verify ELF visibility, loader search
  paths, and actual `dlclose` after workers/instances finish, including passive
  capability views that must not delay unload. Linux ASan/UBSan lifecycle,
  discovery, installed-provider, output and remote lifecycle checks passed with
  isolated provider fixtures. Loading the host's system FFmpeg dependency also
  loads `libgomp`; a standalone `dlopen()`/`dlclose()` of that system library
  reproduces a 104-byte LeakSanitizer report. On 2026-11-26 the maintainer
  accepted this external-library limitation for the gate. The dependency chain
  is system FFmpeg to `libsoxr` to `libgomp`.
  On 2026-11-26, native Arch Linux x86-64 at code commit `e0e63ae38`
  passed provider lifecycle, playback reload, playback-remote lifecycle,
  installed SDK, frontend boundary, and configured binary export tests in a
  system-compiler build without an explicit CMake build type. Installed-provider,
  discovery, shared-runtime restart, and passive-view unload checks also passed.
  A native GCC Debug ASan/UBSan build passed sanitizer probes, lifecycle,
  discovery, output host, installed-provider, shared-runtime, null-output and
  playback-remote tests with leak detection enabled. Playback reload failed during
  fixture setup on the accepted 104-byte system-library leak, then passed with
  `ASAN_OPTIONS=detect_leaks=0`; address and undefined-behavior checks stayed on.
- [x] **SDK-WINDOWS** Run the same provider checks on native Windows. Verify cdecl
  function tables, DLL exports/imports/search paths, and UTF-8/Unicode filenames.
  The intentionally missing-entry fixture must not advertise the provider export.
  Native Windows x64 Release passed 70 focused provider, runtime and export
  checks on 2026-11-26, including relocated SDK providers, lifecycle, restart,
  Unicode discovery and the output providers. `plugins.installed` now inspects
  the relocated C, C++ output and C decoder provider exports/imports and the
  missing-entry fixture on Windows; all passed. The x64 ABI declares `MC_CALL`
  as `__cdecl` and the native function-table tests pass. The maintainer accepts
  the additional library exports in `gmedecoder.dll` and `projectm.dll` and the
  legacy exports in `milkdrop2.dll`. The server currently imports `musikcore.dll`
  through application API calls; replacing those calls with host services is a
  separate SDK design issue. This x64 run does not distinguish calling
  conventions as a 32-bit build would.

### Phase 8: representative families and remaining plugin conversions

The output-first staging described in the original plan has passed: in-tree
providers now use C entry points. The open items below retain remaining
validation and acceptance rather than authorizing a parallel C++ provider path.
Family representatives are recorded under PLG-PROVE-002; the phase-eight
cutover gate is complete.

#### Provider discovery and configuration

- [x] **PLG-MGMT-001** Include C provider metadata, enable settings, and
  typed configuration/settings in the application catalog. `runtime.windows_plugins`
  covers listing, disabling, rediscovery and re-enabling; `runtime.visualizer_session`
  covers disabled rediscovery through the C registry. `api.uri_player` exercises
  typed settings reads, writes and validation. Windows native revalidation remains
  covered by SDK-WINDOWS; this item records implemented catalog behavior.

#### Data streams and encoders

- [x] **ENC-001** Define blocking encoder output to a host stream.
- [x] **ENC-002** Define streaming encoder packet ownership.
- [x] **ENC-003** Implement encoder adapters and pure-C fixtures.
  `plugins.encoder` covers independent instances, packets, flush and unload.
  The server transcode tests exercise the real factory on macOS.
- [x] **ENC-004** Convert `stockencoders`. `server.transcode_body` covers
  flush and output failure, and `api.remote_transcode` exercises the real
  MP3 and FFmpeg providers on macOS. Windows remains to be verified.
  Native Linux `server.transcode_body` and all five `api.remote_transcode.*`
  cases passed on 2026-11-26 at code commit `e0e63ae38`.
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
  for codec callbacks using relative offsets. Each plugin also registers its
  indexer source and settings, with one enabled setting for all capabilities.
  `plugins.subsong_gme` and `runtime.stream_sdk_contract` cover selection on
  macOS. `musikcube.ui.settings` checks the combined plugin row. `plugins.subsong_openmpt` also verifies that the decoder consumes a
  final block carrying EOF without requesting it again.
- [x] **PLG-DEC-004** Validate converted Windows `cddadecoder` and its stream/source
  capabilities. A single C plugin now registers the stream, decoder, and
  indexer, with MusicBrainz metadata and generic labels when lookup fails.
  `plugins.cdda_plugin` covers registration and device-change notifications;
  `plugins.cdda_stream` covers cancellation and the MusicBrainz ID fixture.
  The maintainer accepts CDDA validation as complete; Windows physical-drive
  checks were not independently run as part of this reconciliation.

#### Tags and indexer sources

- [x] **TAG-001** Define metadata writer and tag reader/store tables.
- [x] **TAG-002** Implement adapters and deterministic tag fixtures.
- [x] **TAG-003** Validate the converted `taglib_plugin`.
  `plugins.taglib_metadata` exercises tags, artwork, replay gain and metadata
  writer failure through the real plugin; `plugins.metadata_writer` checks
  invalid input and exception containment. Indexed browse tests cover the
  application path. These checks passed on Linux; platform use is accepted by
  the maintainer.
- [x] **IDX-001** Define indexer source, writer, cancellation, and
  commit/rollback tables.
  The C source, source-scoped writer, rescan notification, and timestamp
  lookup contracts are implemented; [ADR-0010](../adr/0010-indexer-source-identity-and-checkpoints.md)
  records the identity and checkpoint decisions. Source info now requires an
  independent UTF-8 display name; experimental indexer plugins must rebuild and
  supply it. `plugins.indexer_gme` and `plugins.indexer_openmpt` verify published
  names; `plugins.indexer_label-empty`, `plugins.indexer_label-invalid`, and
  `plugins.indexer_label-long` reject invalid labels before scanning.
  Track validation also receives the stored source-specific modification time;
  the experimental callback signature changed and indexer plugins must rebuild.
  `plugins.indexer_adapter` and `plugins.indexer_playlist-validation` cover
  forwarding through the wrapper and the production host.
  Platform use is accepted by
  the maintainer.
- [x] **IDX-002** Implement adapters and fixture source.
  `plugins.indexer_cancel` and `plugins.indexer_fail` exercise cancellation,
  checkpoints, rollback, and foreign-record rejection through the real runtime.
  `plugins.indexer_playlist` pauses a source after a checkpoint and creates an
  empty playlist through the C API before allowing the source to continue. It
  regressed on the previous implementation, which retained the writer lock.
  `plugins.indexer_playlist-validation` checks the same boundary while the source
  validates an existing track; no SQLite read statement spans that callback.
  Source batches now release that lock at checkpoints; GME/OpenMPT and ordinary
  file indexing check a 250 ms interval between files as well as the track limit.
  The host uses the C source boundary directly. The provider cancellation
  fixture has been converted to exercise the same boundary. The C++ registration
  helper receives the plugin-specific host through instance services;
  `plugins.indexer_adapter` checks independent host sessions and rejects temporary
  encoder tables. `plugins.indexer_create-fail` checks that a source creation
  failure terminates startup with a diagnostic before another scan can run.
  The indexer fixture and adapter checks passed on Linux; platform use is
  accepted by the maintainer.
- [x] **IDX-003** Validate converted decoder-provided indexer sources.
  GME and OpenMPT use C factories. `plugins.indexer_gme` and
  `plugins.indexer_openmpt` cover indexing, existing-ID migration, restart, and
  stale-track removal; both passed in the current Linux integration run. The
  Windows Release checks also cover Unicode filenames, shortened containers,
  and recovery after a checkpoint before stale-subtrack validation. Native
  Windows platform use is accepted by the maintainer; Windows-specific checks
  were not rerun as part of this reconciliation.

#### Audio outputs

- [x] **OUT-001** Define real-time-safe output and device contracts.
  Output creation now receives instance-lifetime plugin-scoped setting
  services; `plugins.output_host` checks that a provider can read them. This
  strengthens the experimental output factory contract: hosts which previously
  passed null must provide the service before loading providers that require it.
  `output.h` specifies buffer ownership, callback restrictions, stop and thread
  ordering; `plugins.output_host` and `plugins.output_adapter` passed on Linux.
  Timing and real-time instrumentation remain separate tasks below.
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
- [x] Audit playback position and mix points when an output copies and returns
  buffers before its device consumes them. Keep optional backend latency and
  the existing unknown-timing fallback; do not require a new timeline system or
  device-clock implementations across backends.
  `transport.copied_output_known` uses a bounded copied-PCM queue and explicit
  device advancement to verify that buffer reuse does not advance audible
  position and that the mix point fires once, after its target is reached.
  It reproduced an early mix point caused by whole-second rounding. The player
  now compares unrounded latency-adjusted time while retaining display rounding.
  `transport.copied_output_unknown` verifies bounded approximate progress when
  timing is absent, including ignoring unflagged NaN values.
  Both tests and the existing crossfade, gapless, post-EOF seek and tail-seek
  PCM tests passed on macOS Release (2026-11-25).
  [Output timing contract](musikcore-c-abi-design.md#outputs-and-devices) records the
  position and mix-point policy.
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
- [x] Run the shared `tests/support/plugins/OutputContract.h` checks for each
  output provider using an appropriate device or deterministic fixture. Null,
  WASAPI, ALSA, DirectSound and PortAudio already use them. CoreAudio, PulseAudio
  and PipeWire are now wired into the shared checks as well. CoreAudio and
  PortAudio passed on macOS in Debug/Release and Release respectively.
  Native Linux ALSA, PulseAudio and PipeWire fixture and runtime tests passed
  on 2026-11-26 at code commit `e0e63ae38`. `plugins.sndio_output` runs the
  shared contract against the loadable sndio provider and a deterministic
  device fixture on Linux; it passed in the system build and Debug ASan/UBSan.
  Native Windows x64 Debug and Release `plugins.null_output`,
  `plugins.wasapi_output`, and `plugins.directsound_output` passed on
  2026-11-26 at code commit `166684488`; both device providers exercised the
  default playback endpoint. `plugins.output_host` and
  `plugins.directsound_ring` passed in both configurations as well.
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
- [x] **OUT-006** Validate the converted `alsaout`.
  The Linux ALSA provider now registers the C output factory and uses a
  nonblocking PCM handle with an eventfd wakeup for stopped producers and
  drains. `plugins.alsa_output` covers the real ALSA null device, PCM return,
  drain/reuse, and cleanup; `plugins.alsa_output_cancel` gates a stalled write
  and drain, then checks cancellation and exactly-once return. `binary.exports.alsaout`
  verifies the C entry point. `plugins.alsa_output_runtime` exercises the
  production decoder and player through the ALSA null device, including
  validation of the configurable 10–5000 ms latency. `plugins.alsa_output_cancel`
  checks that the saved latency and default reach ALSA in microseconds. These
  checks passed on Linux, and the maintainer has exercised ALSA playback on a
  physical device. Intermittent silence after switching outputs remains tracked
  under OUT-010.
- [x] **OUT-007** Validate the converted `pipewireout`.
  The PipeWire provider now uses the C output factory and preallocated PCM
  storage. `plugins.pipewire_output` exercises a real isolated PipeWire null
  sink, including pause, drain, and cancellation. `plugins.pipewire_output_runtime`
  exercises the production decoder and player against an isolated null sink.
  The historical setting
  store was named `PipeWire IOutput`; the new provider uses `PipeWire`, so
  existing scoped settings and pinned devices are not migrated. Both isolated
  sink tests passed on Linux; physical playback is accepted based on maintainer
  use.
- [x] **OUT-008** Validate the converted `portaudioout`.
  Accepted by the maintainer on 2026-11-25 with the recorded limitations.
  The PortAudio provider now registers the C output factory and uses a bounded
  preallocated queue. `plugins.portaudio_output` runs the shared output contract
  against the default macOS device when available. `plugins.portaudio_failure`
  checks unexpected device termination and races with buffer submission. Other
  supported platforms and physical device removal remain open.
- [x] **OUT-009** Validate the converted `pulseout`.
  The PulseAudio provider now uses the C output factory and receives scoped
  settings. `plugins.pulse_output` covers PCM return, drain/reuse, the linear
  volume setting, and gated cancellation of stalled writes and drains using
  a deterministic backend. The real provider was also exercised manually on a
  temporary PulseAudio-compatible null sink. `plugins.pulse_output_runtime`
  exercises the production decoder and player against an isolated null sink.
  The historical setting store
  was named for `PulseAudio IOutput`, while the C provider is named
  `PulseAudio`; existing plugin-scoped settings and pinned device selection are
  not automatically migrated. The PulseAudio fixture, no-wait and runtime tests
  passed on Linux; physical playback is accepted based on maintainer use.
  A failed open of a pinned ALSA or PulseAudio device retries the system default.
  PulseAudio pause, resume, and volume submit operations without waiting for a
  server response because the host serializes controls with stop. Immediate
  submission failures are reported; later server-side failures cannot be
  returned to the caller. `plugins.pulse_control_nowait` checks this against a
  real server. Follow-up: revisit whether
  `pulse_blocking_stream.c` is needed; using the PulseAudio API directly may
  simplify the provider.
- [x] **OUT-010** Validate the converted `sndioout`.
  Accepted by the maintainer on 2026-11-25 with the recorded limitations.
  The provider is named `sndio`; settings saved under the previous
  `Sndio IOutput` name are not migrated automatically.
  An intermittent silence after switching outputs was observed with sndio and
  ALSA; restarting playback recovered it. Cached tracks appeared correlated,
  but the cause and a reliable automated reproduction remain open. On older
  sndio without `sio_flush()`, a seek uses the host's stop-and-reprepare path;
  closing the device can drain queued audio and delay stop or seek. The
  `plugins.sndio_no_flush` and `plugins.output_host` tests cover this fallback.
  `plugins.sndio_output` exercises the shared output contract using a Linux
  device fixture; it does not resolve the intermittent physical-output silence.
- [x] **OUT-011** Validate the converted Windows `directsoundout`.
  The provider registers a C output factory; `plugins.directsound_output` and
  `plugins.directsound_ring` passed on native Windows Release on 2026-11-26 at
  code commit `34f77d167`, including the available endpoint playback checks.
- [x] **OUT-012** Validate the converted Windows `wasapiout`.
  The shared-mode provider now registers the C output and configuration APIs.
  `plugins.wasapi_output` covers a Windows endpoint, settings, PCM completion,
  pause/resume, drain, timing, interrupted writes, and teardown.
  Native Windows Release execution passed on 2026-11-26 at code commit
  `34f77d167`, including the available endpoint playback checks. The clock
  snapshot is a bounded 128-bit atomic publication: a 16-bit generation and
  48-bit frame count plus a 64-bit monotonic timestamp; generation wraps after
  65535 preparations. The last valid snapshot remains available until stop or
  reprepare. Live default-device changes, device loss, and interruption of a
  stalled drain remain manual or future deterministic checks. Routing drains
  the old endpoint when possible; a disconnected endpoint can discard audio
  already copied to it. Exclusive mode may be a separate plugin; defer that
  decision until its format negotiation and device behavior are designed.
  The legacy settings scope was `Wasapi IOutput`; the C provider uses
  `WASAPI`, and old scoped settings and pinned device IDs are not migrated.
  Shared-mode playback now waits on WASAPI's buffer event, a stop event, and
  device notifications. WASAPI chooses the shared-mode event buffer size, so
  the former `buffer_length_seconds` setting is retired and saved values are
  ignored. No separate software buffer is introduced.
  The host creates an unopened output before querying its preferred format;
  the instance reads scoped services supplied at creation. Moving this query
  from the factory to the instance changes the experimental output plugin ABI;
  providers must be rebuilt. `plugins.output_host` exercises settings before
  playback and `plugins.wasapi_output` checks the WASAPI setting and lifecycle.
- [x] **OUT-013** Retire the legacy Windows `waveout` plugin; WASAPI and
  DirectSound remain available as Windows outputs.

#### DSP and remaining plugin categories

- [x] **DSP-001** Define in-place processing on the playback worker.
- [x] **DSP-002** Convert `supereqdsp`. `plugins.dsp_processing` checks two
  active processors created by the media host and live equalizer changes on macOS.
  `plugins.dsp_failure` verifies that a partly modified buffer is discarded.
  Windows remains to be verified.
  Native Linux `plugins.dsp_processing`, `plugins.analyzer`, and
  `plugins.analyzer_cancel` passed on 2026-11-26 at code commit `e0e63ae38`.
- [x] Implement the analyzer factory and metadata writer on the indexer worker.
  `plugins.analyzer` checks real indexing with successful and failing C
  providers. Analyzer changes for a track are saved only when all participants
  succeed, following the [analyzer contract](musikcore-c-abi-design.md#dsp-metadata-writers-tag-readers-and-analyzers).
  Cancellation waits for the current analyzer call to return. `plugins.analyzer_cancel` covers cancellation and shutdown
  during blocked stream open. The analyzer currently uses the internal Stream
  pipeline over the C stream and decoder primitives. Reassess that remaining
  C++ interface when consolidating decoded-audio consumers. Windows remains to
  be verified.
- [x] **EXT-001** Design schema/configuration descriptors as data. The C
  settings descriptors and scoped setting services are implemented;
  `api.uri_player` exercises their application-facing reads and writes.
- [x] **EXT-002** Validate migrated visualizer providers across platforms.
  The C visualizer contract is used by the runtime, and the Windows GDI provider
  is converted. `runtime.visualizer_session` passed on Linux; native Windows
  provider behavior is accepted based on platform use.
- [x] **EXT-003** Validate migrated playback-remote providers.
  A playback remote receives playback changes and issues commands; it may be a
  local integration or a network server. Design its C contract around that
  role, including safe worker shutdown where needed. The C factory and event
  tables are declared and malformed registrations are rejected by the registry.
  `plugins.playback_remote_adapter` checks the header-only C++ helper; it does
  not prove engine lifecycle order. `plugins.playback_remote_lifecycle` covers
  activation, playback events, failed start cleanup, and ordinary shutdown
  through the real runtime, including a worker waiting for a queued playback
  command while shutdown cancels it, concurrent submission and dispatch, queue
  saturation, and rejected submissions outside the active lifetime.
  `plugins.playback_remote_stop_order` checks
  that all remotes are stopped before any is joined; on Unix a worker is gated
  on another remote's stop. `plugins.playback_remote_factory_throws` checks
  fatal containment before touching an uncertain instance. The initial command
  host supports bounded owner-thread control requests; individual request
  cancellation is deliberately omitted for disconnects as recorded in
  [Playback remote contract](musikcore-c-abi-design.md#visualizers-and-playback-remotes). Provider
  migrations are complete. Failed remotes must wake their own
  workers independently, so healthy remotes can remain active under the same
  contract.
  `plugins.playback_remote_startup_rollback` exercises that failure policy on
  Unix while a healthy remote continues processing commands.
  The macOS media-keys provider uses the C playback remote and joins its event
  tap before unloading. `plugins.macos_media_keys` exercises repeated
  start/stop/join cycles; physical key handling remains a manual macOS check.
  The Linux MPRIS provider uses the C remote interface. `plugins.mpris` checks
  D-Bus commands and snapshots; `plugins.mpris_runtime` exercises metadata,
  artwork, commands, and shutdown through the real managed runtime. Its worker
  uses the new origin-aware `read_library_thumbnail` library host operation;
  `plugins.mpris_artwork_stall` verifies that D-Bus remains responsive while
  the artwork worker is blocked. Shutdown still waits for an in-flight read.
  `api.managed_remote` checks local and remote identities and cancellation.
  The lifecycle and stop-order tests passed under ThreadSanitizer on macOS and
  ordinary Linux integration tests passed in the current run. Native Windows
  behavior is accepted based on platform use; Linux sanitizer evidence is
  recorded under SDK-LINUX.
- [x] **EXT-004** Validate migrated server integration through the C interfaces.
  Server media helpers consume the C media and scratch-buffer services directly;
  `server.transcode_body` and `api.remote_transcode` cover that path on macOS.
  The proxy's external-ID lookup and playlist removal queries are in
  `PlaylistTrackQueries.h` and retain their inputs; `library.playlist_track_queries`
  checks lookup after the caller changes its input buffer.
  The server registers as a C playback remote and uses the library host,
  the public C playback API, and C settings and media services. The former
  `IMetadataProxy` interface and injection path have been removed.
  `plugins.playback_remote_lifecycle` exercises worker-side library and
  playback calls; `api.managed_remote` covers cancellation of a stalled
  remote-origin library read and transport-slot reuse. Finish validation:
  Queue-page reads reuse the context's asynchronous requests. A disconnected
  client's accepted read can occupy a bounded request slot until the upstream
  replies or the plugin stops; work already sent upstream cannot be revoked.
  Named structured library requests use the existing query registry through
  the plugin host; unknown queries and arbitrary SQL are rejected.
  Queue responses contain at most 1,000 tracks; clients request further pages
  using the offset. Android's queue screen requests 75 tracks per page.
  Server configuration is exposed through the plugin's settings schema; the
  dedicated TUI screen uses generic plugin operations, and the core has no
  server-specific configuration API.
  `api.managed_remote` checks hidden-track direct lookups, visible-only search,
  and playlist removal counts. `api.uri_player` checks that plugin settings
  reject off-owner access before invoking a callback.
  - [x] Register the server and its configuration through C plugin discovery
    and bind its playback remote to the host's playback and library services.
    Stop network workers before releasing those services.
    `server/main.cpp` registers both features; `api.remote_audio` and
    `api.remote_transcode.*` exercise the real server and plugins on Linux.
  - [x] Replace server calls across the former C++ plugin boundary with C
    playback, library, settings, output, indexer, and media operations. The old
    metadata-proxy injection path has been removed.
  - [x] Accept server start, settings reload, active requests, and shutdown
    based on use with real remote clients and existing tests.
  - [x] Accept owner-thread dispatch and disconnected-client request lifetimes
    with the existing tests and bounded behavior. An upstream request may remain
    occupied until a reply or plugin stop; remote cancellation is not promised.
  - [x] Resolve named-query protocol and remote-client wire validation.
    `api.backend_parity` compares local and loopback results, including genre
    projection; its former failure expected that supported field to be rejected.
    `api.remote_audio` exercises the real server and verifies unknown named
    queries and raw SQL are rejected.
- [x] **EXT-005** Validate migrated platform hotkey and media-key plugins.
  The macOS media-keys and Linux MPRIS providers use C playback remotes;
  `plugins.mpris_runtime` passed in the current Linux run. Physical keys and
  platform behavior is accepted based on use.
- [x] **SDK-013** Add contract fixtures and malformed cases for every supported
  provider family. `plugins.lifecycle` rejects incomplete output, stream,
  decoder, encoder, DSP, indexer, settings, playback-remote, tag-reader,
  analyzer and visualizer factories before their callbacks run.

#### Phase 8 gate

- [x] **GATE-800** The engine uses only PluginRegistry and the
  new C contracts. All shipped plugins use the C SDK or an explicitly approved
  non-plugin integration; no active engine dependency, exported plugin function,
  build target, or installed header preserves the old C++ SDK. Required app
  workflows and plugin tests are restored; missing optional families and
  a missing output remain supported idle configurations; bundled `nullout`
  supplies headless playback. `retired_sdk_boundary`, `plugins.empty_session`,
  `plugins.installed`, `plugins.null_output`, `api.uri_player`, and
  `api.remote_audio` passed on Linux. Platform and sanitizer acceptance remain
  tracked independently under SDK-LINUX, SDK-WINDOWS and SDK-REMOVE-007.


## Application media APIs

The application-facing media API is separate from the C plugin SDK. This work
was moved from the core migration LLD because it builds on provider services.

- [x] **PUB-008** Expose stream, decoder, and encoder operations to application
  C consumers. The existing `mc_plugin_media_service` and scratch buffer table are
  provider-session services, not application-owned APIs: their function tables
  and instances can become invalid at plugin shutdown. Do not return them
  directly from `mc_context`.
  - [x] Define application-owned C handles and operations for stream IO,
    decoding to PCM, and encoding, including units, ownership, blocking and
    interrupt behavior, EOF/finalization, diagnostics, and buffer lifetimes.
    Keep the application and plugin ABI/version boundaries separate.
  - [x] Bind handles and in-flight operations to context shutdown. Ensure
    decoder/input and encoder/output ordering; quiesce blocked work before
    destroying providers, and make retained handles safe after shutdown.
  - [x] Prove invalid inputs are rejected before provider calls. Exercise
    local/remote streams, decode, encode, cancellation, failed creation,
    retained handles and teardown through an independently built C consumer;
    update public header, export, ABI-only, and installed-consumer checks.
  Implemented in `media.h` / `media.hpp` and the context-owned `MediaSession`;
  [ADR-0006](../adr/0006-explicit-uri-player-resources.md) records ownership;
  the [media contract](musikcore-c-abi-design.md#advanced-audio-api) defines
  cancellation, PCM copies and explicit encoder finish. Creation consumes an
  opened input/output only on success. Retained handles cannot reach provider
  callbacks after shutdown.

  Acceptance: `musikcore.integration.api.media` exercises the public C API and
  C++ owners against local files, loopback HTTP, byte decoders, streaming/file
  encoders, partial writes, factory/open/write/finalize/close failures, invalid
  inputs before provider calls, cancellation before open, blocked open/read/
  decode/write shutdown, cleanup order, and retained PCM/handles. The independent
  relocated C consumer is also built/run by `musikcore.integration.plugins.installed`.
  `musikcore.abi.header`, `shared_c`, `exports`, `link_boundary` and individually
  compiled public C11/C++17 headers cover defaults, ABI-only availability and
  the export surface. Native Linux/Windows acceptance remains in the existing
  open platform gates. On macOS Release (2026-11-25), all nine targeted CTests
  passed, including the installed-provider/consumer check and URI-player
  regression, and the independent public-header build passed.
- [x] **PKG-003** Build an external C decoder plugin against the installed SDK.
  `plugins.installed` builds the C decoder using only relocated SDK headers and
  checks its exports and imports.
- [x] **PKG-004** Load and exercise that decoder through the installed runtime.
  `plugins.installed` builds an independent C++ consumer linked to the relocated
  runtime and plays the decoder fixture through an external null output. The
  PCM contract fixture also verifies the decoded samples using that decoder.

## Remaining SDK removal

The [SDK removal design](musikcore-sdk-removal-design.md) is the implementation
plan for the core interfaces and incidental machinery that outlived the plugin
cutover. These tasks complete the internal cleanup in PLG-BACKFILL-002; they do not
replace the separate native-platform acceptance checks.

- [x] **SDK-REMOVE-001** Inventory remaining interface callers, ownership,
  backend/thread boundaries, and old-only adapters and select the smallest
  useful replacements; characterize affected public/runtime behavior.
- [x] **SDK-REMOVE-002** Remove unused setting/utility surfaces and move
  genuinely shared helpers to an appropriate internal/common owner. Update
  in-tree plugins, build manifests, and direct includes across platforms.
  Shared helpers are private; the fixed ring queue is an installed C++ utility.
- [x] **SDK-REMOVE-003** Replace `IBuffer` and its audio handoff with a direct
  ownership model. Preserve fixed-pool, completion, cancellation and real-time
  guarantees; run output, transport and player contract tests.
- [x] **SDK-REMOVE-004** Replace `IPlaybackService`, track-list/editor APIs and
  unnecessary proxy machinery with owner-directed operations. Verify queue,
  playback, notifications and remote/local behavior through real entry points.
- [x] **SDK-REMOVE-005** Remove metadata/track/resource SDK object graphs and
  wrappers, preserving typed query backend routing, data/track lifetimes,
  custom keys, paged results and persisted identifiers.
  Linux integration and targeted contract tests cover these changes; native
  platform acceptance remains separately tracked.
- [x] **SDK-REMOVE-006** Remove obsolete SDK interfaces, wrappers, PCH/build/
  install entries and legacy-only tests. The retired-SDK source check and
  existing exported-symbol checks guard the boundary.
- [x] **SDK-REMOVE-007** Complete removal acceptance: testing-disabled build,
  standalone packaging checks and native-platform acceptance. Linux normal and
  `BUILD_TESTING=OFF` builds, installed consumer/provider checks, and a full
  Zig/glibc 2.28 standalone dependency scan passed. The maintainer accepts
  Windows and macOS platform use; these platforms were not rebuilt in this run.
  The separate SDK-LINUX sanitizer limitation is accepted as recorded above.

## Plugin settings writes and projectM state

- [x] **SET-001** Add a plugin-scoped batch write operation backed by the existing
  settings store. `musikcore.integration.plugins.settings` checks real persistence,
  plugin isolation, whole-batch input rejection and memory rollback on I/O failure;
  `musikcore.unit.plugins.contract_headers` checks the sized table initializer.
- [x] **SET-002** Persist projectM controls and selection through the SDK, including
  the macOS helper. `musikcore.integration.plugins.projectm` restores saved values,
  renders a frame, receives the helper's live state, and verifies reopening.
  Selection uses paths rather than list indices; missing files fall back to the
  first available preset. macOS uses the existing socket and audio sender worker;
  settings never enter the real-time PCM callback. Small chunks respect macOS's
  2048-byte local datagram limit. No additional worker or preference-file parser
  is introduced.
- [x] **SET-003** Standardize C/C++ application and plugin APIs, filenames, exports,
  native callers, and Rust/.NET binding sources on settings. Shared scalar types
  live in `setting_types.h`; plugin headers remain independent of application
  operations. Experimental application exports and plugin table layout change;
  rebuild clients/plugins together with fixed experimental ABI versions. Native
  Windows and Linux revalidation remain in their existing release gates.

The new write service persists one complete update per settings change. If saving
fails, projectM logs the error and keeps rendering; subsequent changes can retry.
An environment-selected preset directory overrides a saved directory without
turning the temporary override into the saved collection. Panel/search/confirmation
state is transient. Source display names now omit decoder suffixes; existing
indexer-provider contracts verify "Game music" and "Tracker modules".

Validation on macOS (2026-11-26): native TUI/daemon and standalone Release SDK
builds pass; the eight focused projectM/settings/header/managed-runtime/source
provider/core-boundary checks pass. Rust workspace tests, isolated fixture checks,
and real-renderer UI checks pass against the staged SDK, and the optimized app is
rebuilt. The Linux renderer translation unit passes a host syntax check; that is
not native Linux acceptance. .NET binding sources are renamed but no .NET SDK is
installed here, so .NET/Windows builds remain unverified by this change.
