# musikcore C ABI modernization: high-level design

Status: active migration; foundation stabilization in progress

## Current delivery goal

Finish the core application C API and integrate musikcube through its strictly
header-only C++ wrapper. The [foundation gate](musikcore-c-abi-lld.md#core-c-api-foundation-gate)
comes first. Then close the application API gaps, migrate the remaining
consumers, and reach core release-candidate quality before beginning
plugin/provider API work.

This document owns architecture and sequencing, and links the decision records
in `docs/adr/`.
The [LLD](musikcore-c-abi-lld.md) owns the implemented contract, acceptance
criteria, task status, and validation commands. Public headers define individual
parameters and lifetimes; tests verify their behavior. The detailed API/SDK
design describes the target, including deferred surfaces.

## Purpose

This document defines the high-level architecture and migration sequence for
making musikcore:

- Consumable as a stable native shared library from multiple languages and
  runtimes.
- Extensible through a C plugin SDK so decoders, encoders, streams, outputs,
  DSPs, tag readers, and other providers do not depend on a C++ ABI.
- Testable first at the public API and plugin SDK boundary, then inward at the
  component level and outward through applications and release packages.

Detailed interface proposals are in
[musikcore-c-abi-design.md](musikcore-c-abi-design.md). The executable testing
strategy is in [testing-design.md](testing-design.md). Concrete implementation
tasks and acceptance gates are in
[musikcore-c-abi-lld.md](musikcore-c-abi-lld.md).

## Context

At the start of this refactor, musikcore had two boundary mechanisms:

- A C API that forwards through opaque values into C++ interfaces.
- Plugin entry points with C linkage that return C++ interface pointers.

Those boundaries motivated the replacement described in
[ADR-0001](../docs/adr/0001-c-abi-and-core-first-refactor.md). The previous C API
had ambiguous ownership, inconsistent errors, incomplete async behavior, and
callback/threading hazards. Its declarations are now retired. Existing plugin
entry points still expose C++ vtables and compiler ABI despite their C linkage.

Unix-like and Windows builds use a shared musikcore library for the
application. On Windows the GUI and console frontends import the same
`musikcore.dll`. The application C ABI remains experimental until freeze.

Forcing the current implementation into a Windows DLL before the C ABI is
defined would introduce avoidable risk:

- Private C++ symbols and vtables would cross the DLL boundary.
- CRT and allocator differences could corrupt cross-module objects.
- Debug and Release configurations could expose incompatible layouts.
- Existing C++ plugin interfaces would remain compiler-dependent.
- The process could accidentally contain duplicate musikcore global state if
  static and shared variants were loaded together.

The new shared-library artifact must therefore follow the minimal C ABI, not
precede it.

## Goals

- Establish a stable, versioned C ABI as musikcore's only promised binary ABI.
- Separate application-facing operations from plugin-provider interfaces.
- Keep memory allocation and destruction within the originating module.
- Support deterministic async operations and event delivery across runtimes.
- Preserve zero-copy PCM paths.
- Make metadata queries efficient through immutable, page-owned snapshots.
- Exercise every public contract through separately compiled C consumers and
  C plugins.
- Migrate musikcube through an idiomatic C++ wrapper over the public ABI.
- Preserve a working application throughout the migration.
- Freeze ABI major version 1 only after application and plugin dogfooding on
  Windows, macOS, and Linux.

## Non-goals

- Maintaining compatibility with the existing `mcsdk_*` API.
- Maintaining the current C++ plugin ABI after in-tree migrations finish.
- Exposing the internal C++ object graph as a public object model.
- Shipping the experimental shared façade before it has a usable supported
  surface.
- Supporting untrusted in-process plugins.
- Rewriting the musikcore implementation in C.
- Defining an out-of-process RPC protocol in this project phase.

## Current experimental scope

The installed names are `mc_*`, `include/musikcore`, and `musikcore-abi0`.
ABI0 is experimental; callers and the library must be rebuilt together after
interface changes. Successful context or identity creation does not imply a
running library connection, plugin loader, or audio session.

| Surface | Standalone ABI0 | Production-bound core |
| --- | --- | --- |
| Context, dispatch, cancellation, shutdown, errors | Supported | Supported |
| Local queries, search, result/range reads | Supported against `musik.db` | Selected typed backend |
| Remote queries and projected reads | Identity only; no public connection factory | Live connection-generation adapter; projection limits in `library.h` |
| Category/directory selection and playlists | Unsupported without adapter | Typed local/remote queries |
| Playback and queues | No public session factory | Handle bound to the application's session |
| Plugin loading, notifications, preferences, indexing | No public surface yet | Private implementation services |

Existing bound operations are migration steps, not a finished embedding API.
Public runtime/session ownership and the remaining application consumers must
be addressed before the app can stop depending on private core services.

## Architectural principles

### The C ABI is the contract

Only C declarations, opaque handles, fixed-layout value structures, and C
function tables cross module boundaries. C++ exceptions, classes, references,
STL objects, RTTI, and compiler-defined layouts remain private.

Language-specific APIs are wrappers over the C ABI. The initial official
wrapper is C++ and is used by musikcube. Other bindings can be generated or
implemented once the ABI has been validated.

### Consumer and provider surfaces are separate

The public C API and C SDK share primitive definitions but serve opposite call
directions:

- The public API lets applications call musikcore.
- The C SDK lets musikcore call plugin-provided implementations.

The public API favors commands, immutable snapshots, async requests with per-request callbacks,
and a queue for unsolicited notifications. The SDK favors explicitly versioned function tables because the
host must invoke providers directly, including in audio paths.

Their private implementation is grouped under `src/musikcore/ffi`, with
`common`, `consumer`, and `provider` subdirectories. Installed headers
remain flat under `include/musikcore`.

### Ownership never depends on a shared allocator

The module that creates an allocation or object destroys it:

- Applications destroy core handles through public API functions.
- Plugins destroy provider instances through their own function tables.
- Host audio buffers are created, mapped, unmapped, and recycled through host
  services.
- Strings cross as views, caller buffers, or storage owned by an explicit
  parent object.

This permits different CRT selections on Windows and different language
runtimes without requiring a common heap.

### Async behavior is part of the contract

Database queries, metadata loading, indexing, and other potentially blocking
work do not become synchronous merely because they cross a C API.

Each accepted request has one terminal callback delivered by nonblocking
dispatch on a host-selected thread. Request IDs support cancellation. An
optional wakeup hook posts work to sleeping host loops. Unsolicited
notifications use a separate queue. Shutdown settles outstanding callbacks
before the host releases its state.

### Tests define observable behavior

API and SDK behavior is written as a black-box contract test before or with the
implementation. Tests compile separately against public headers and cross the
actual shared-library or dynamic-plugin boundary.

Unit tests fan inward where a state machine or algorithm needs more precise
coverage. Integration and packaging tests fan outward once the boundary works
in isolation.

### The ABI remains experimental during migration

The new surface uses an ABI version 0 or an explicitly experimental library
name until both of these are true:

- musikcube exercises the public API through its C++ wrapper.
- At least one production plugin exercises the provider SDK.

This prevents premature compatibility commitments while real consumers expose
missing operations or awkward ownership.

## Target architecture

```text
                         musikcore C++ implementation
                                    |
                         private adapter layer
                                    |
                 +------------------+------------------+
                 |                                     |
          public C API                            C plugin host
                 |                                     |
       +---------+---------+                   C SDK function tables
       |                   |                             |
 C++ wrapper         other bindings              provider plugins
       |
   musikcube
```

The final release topology is:

```text
musikcube ──> C++ wrapper ──> libmusikcore C ABI ──> private C++ core

plugin DLL/.so/.dylib ──> C SDK tables <───────────┘
```

Applications do not link private implementation libraries. Plugins do not link
or return private C++ interfaces.

## Transition architecture

The transition deliberately separates the production application from the
experimental ABI.

### Stage A: initial production topology

```text
Windows musikcube ──> static musikcore
Unix musikcube    ──> current internal shared musikcore
existing plugins  ──> current C++ SDK interfaces
```

Testing infrastructure and ABI header design begin without changing this
topology.

### Stage B: experimental façade

After the minimal ABI foundation exists:

```text
contract C consumer ──> experimental musikcore ABI shared library
                                  |
                            private core code
                                  |
pure-C fixture plugin ──> experimental C SDK host

production musikcube ──> production musikcore (legacy core and consumer C API)
```

The experimental library has a distinct logical and output name, such as
`musikcore-abi0`, so it cannot accidentally replace the current production
target. It is initially a development and test artifact.

No process may link the private implementation statically and also load the
experimental shared façade. Doing so would duplicate singletons, message
queues, plugin registries, database state, and audio globals.

### Stage C: application migration

Individual musikcube features move through the C++ wrapper to the consumer
C API compiled into production musikcore. The production build shares its
SQLite implementation and extensions with the new API. The separate ABI0
shared target remains available for contract tests and the pilot. Temporary
internal adapters preserve existing behavior behind the façade.

That application migration preserved its working path until the public surface
covered the required behavior. The provider replacement uses its own branch and
cuts over one family at a time. Migrated families use only the C contract;
other families keep their existing implementations until their cutover. This
keeps the app usable without compatibility exports or C-to-old-SDK adapters.

#### Library routing and identity

The app owns one context for request dispatch, cancellation, and
shutdown. An explicit library handle selects the local or remote backend.
Consumer operations pass typed queries through that handle and return the
same owned results for either backend. Discovery, connection settings, and
library selection expose the backend distinction.

Search results and track identities stay bound to their originating library.
The result retains a stable order for page reads. A deleted track keeps
its position. The client retains remote result IDs and uses the existing
typed batch query for metadata. A disconnect leaves materialized pages
readable and ends reads tied to the old connection. Every accepted request settles
once on the dispatcher, including when a reply races with cancellation or
disconnect. Catalog operations target the library handle, and playback
uses the originating library to resolve media for the client.

Prove local and remote Track Search parity before widening the migration to
other library views. The remote routing proposal describes the data flow
and compatibility rules.
[Remote library routing proposal](musikcore-remote-library-proposal.md)

### Stage D: final topology

After musikcube and in-tree plugins migrate:

- Remove the legacy C API and C++ plugin binary contract.
- Make the C ABI shared library the normal musikcore target.
- Remove the experimental library name.
- Assign ABI major version 1 and platform library version metadata.
- Retain old consumer and plugin fixtures as compatibility tests.

## High-level components

### Shared ABI foundation

One installed foundation header defines:

- Symbol visibility and calling conventions.
- ABI versions and feature discovery.
- Fixed-width results and enum representations.
- Errors and UTF-8 string views.
- Sized structures and slices.
- Common audio formats and buffer views.

### Public runtime

The application-facing runtime owns:

- Context creation and shutdown.
- Request IDs and cancellation.
- Request dispatch, wakeups, and callback-based notifications.
- Library queries and metadata pages.
- Track lists and async cache windows.
- Playback and queue commands and snapshots.
- Indexer and preferences.
- Advanced audio handles where justified.

The default context starts the complete engine. Callers can select a fixed set
of Library, Plugins, Indexer, and Playback services at creation; prerequisite
expansion starts Library and Plugins for either Indexer or Playback. Playback
does not require indexing. An explicit empty set retains request processing
for caller-owned catalogs/sessions. See [the lifecycle contract](musikcore-c-abi-design.md#context-and-runtime-lifecycle)
and [ADR-0005](../docs/adr/0005-context-owned-production-runtime.md).

Explicit URI player resources can be constructed within a Plugins-only context,
without Library or Indexer. Their transport lifetime ends at explicit destruction
or context shutdown; catalog playback remains a separate origin-bound contract.
See [ADR-0007](../docs/adr/0007-explicit-uri-player-resources.md).

### Plugin loading and ownership

Use one C plugin architecture in the replacement engine:

- `PluginRegistry` discovers libraries, identifies the C entry point, initializes
  candidates transactionally, and owns accepted plugins for the engine session.
  It also provides copied metadata and non-owning capability lookup.
- `Plugin` owns one library's tables and per-plugin state. All family instances
  and their calls finish before registry teardown.

Draft the shapes for every family together. Then build the registry,
empty optional-family behavior, and the loadable `nullout` C plugin before proving
one representative per family and converting the rest. The replacement engine
has no runtime fallback to the old C++ SDK; the preserved branch is the working
fallback during reconstruction. Missing optional families are ordinary supported
states. Missing decoding/encoding support reports unsupported operations.

Factories register before application services are bound. Instance creation
receives scoped typed services; active integrations start after their required
services are ready. Stop integrations, interrupt I/O, drain scoped host callbacks,
join workers, and destroy instances before final registry teardown. Private C++
wrappers may simplify calls internally, but do not implement the former SDK as a
compatibility layer. See
[ADR-0008](../docs/adr/0008-c-provider-bootstrap-and-unload-lifetime.md).

### C++ wrappers

The application wrapper provides RAII handles, strings, spans, errors, and
event dispatch suitable for musikcube. The provider wrapper optionally lets
C++ plugin authors implement friendly classes while exporting only C function
tables.

The wrappers are header-only source conveniences over the C contracts.

### Test harness

The test harness provides:

- A separately compiled C application.
- Pure-C fixture plugins.
- Temporary data and plugin roots.
- Deterministic media streams, decoders, and outputs.
- Event waiters with deadlines rather than sleeps.
- Sanitizer and fault-injection configurations.
- Installed-artifact and package tests.

## Sequencing

1. Establish tests, C ABI primitives, and an experimental shared façade without
   changing the Windows production application's static linkage. These are
   already implemented to varying degrees; the LLD tracks the remaining gates.
2. Finish the core C API foundation gate. Validate the existing contracts and
   native binary boundary before adding feature families or migrating more
   application functionality.
3. Complete the core application API and migrate musikcube in tested vertical
   slices. Each slice includes C contracts, header-only wrapper support, and
   real application consumers. Remove private application dependencies and the
   temporary library/playback binding adapters as public ownership replaces them.
4. Validate the migrated application and shared library on supported platforms,
   including installed consumers, runtime paths, dependency scans, and staged
   application smoke tests. This establishes core release-candidate quality.
5. Only then replace the plugin architecture on its own branch. Draft all C
   interface shapes first and establish the session-owned registry and empty/null
   behavior. Replace each family's loading, factory, and engine ownership path
   together, beginning with outputs; keep unmigrated families usable on their
   existing contracts. Prove representative providers with boundary fixtures
   before backfilling each family. Remove the former SDK after the last family
   migrates; do not add compatibility exports to converted plugins.
6. Complete consumer/plugin packaging and compatibility validation, remove
   transitional implementations, and freeze ABI v1 after both surfaces have
   proven real consumers. Core release-candidate quality does not freeze ABI0.

The LLD retains stable task IDs and historical phase numbers for tracking.
Those numbers do not authorize starting a provider phase before step 4.

## Windows-specific strategy

The Windows migration first isolated ABI0, then moved both production frontends
to the shared DLL.

- Establish the minimal C ABI boundary before switching the Windows production
  core from static linkage to a DLL.
- Build the experimental façade under a distinct name while musikcube continues
  using the static implementation.
- Export only functions explicitly marked by the C ABI; do not enable automatic
  export of all C++ symbols.
- Generate and test the import library through an external C target.
- Keep allocation and destruction within the originating DLL or executable.
- Test supported Debug and Release configurations independently.
- Ensure the experimental DLL and legacy static core are never present in the
  same process.
- Switch musikcube to the DLL only when the migration build uses no private
  static implementation symbols.

## Risks and mitigations

### Duplicate core state during migration

Risk: an executable links the private static implementation and loads the new
shared façade.

Mitigation: distinct targets and output names, explicit link-graph checks, and
a runtime diagnostic guard in experimental builds.

### Premature ABI freeze

Risk: early bindings make incomplete structures permanent.

Mitigation: use ABI version 0, require musikcube and a production plugin to
dogfood both surfaces, and freeze only after package tests pass.

### Wrapper-driven API leakage

Risk: the C API recreates internal C++ classes and inherits their lifetime
problems.

Mitigation: design around commands, snapshots, pages, request IDs, and events.
Review public operations by use case rather than internal class parity.

### Async regressions

Risk: C wrappers make track retrieval synchronous and degrade scrolling.

Mitigation: make cache windows and completion callbacks part of the first metadata
slice; benchmark the migrated list view before expanding migration.

### Plugin unload and callback races

Risk: a provider function table outlives its dynamic library.

Mitigation: keep DLLs loaded for the engine session. Stop new work, finish calls
and callbacks, join workers, and destroy instances before registry shutdown.
The registry unloads on its owner thread without per-plugin leases or waits.

### Cross-runtime allocation defects

Risk: one module frees memory allocated by another.

Mitigation: explicit destroy functions, caller buffers, parent-owned views, and
host-provided audio allocation. Exercise mismatched-runtime configurations on
Windows.

### Real-time regressions

Risk: provider abstraction adds allocation, locking, or copies to audio paths.

Mitigation: define real-time-safe tables, use host-owned mapped buffers, add
debug instrumentation, and test frame flow through deterministic outputs.

## Success criteria

The modernization is complete when:

- musikcube uses the public C ABI exclusively through its C++ wrapper.
- A separately built C application can use the installed shared library.
- A separately built pure-C plugin can provide a decoder through the installed
  SDK.
- In-tree plugins no longer exchange C++ interfaces with the host.
- Public async behavior preserves nonblocking list and playback behavior.
- PCM data does not require an ABI-induced copy.
- Windows, macOS, and Linux contract and integration suites pass.
- Sanitizers pass lifecycle, cancellation, and plugin unload tests.
- Staged package tests pass outside the source tree.
- ABI version 1 compatibility is enforced by older consumer and plugin
  fixtures.

## Architectural decision records

- [ADR-0001: C ABI choice and core-first refactor](../docs/adr/0001-c-abi-and-core-first-refactor.md)
- [ADR-0002: host dispatch and request lifetime](../docs/adr/0002-host-dispatch-and-request-lifetime.md)
- [ADR-0003: origin-bound results and paged metadata](../docs/adr/0003-origin-bound-results-and-paged-metadata.md)
- [ADR-0004: testing at the public boundary](../docs/adr/0004-test-the-public-boundary-first.md)
- [ADR-0005: context-owned production runtime](../docs/adr/0005-context-owned-production-runtime.md)
- [ADR-0006: enforce the frontend consumer boundary](../docs/adr/0006-enforce-the-frontend-consumer-boundary.md)
- [ADR-0007: explicit URI player resources](../docs/adr/0007-explicit-uri-player-resources.md)
- [ADR-0008: C provider bootstrap and unload lifetime](../docs/adr/0008-c-provider-bootstrap-and-unload-lifetime.md)
- [ADR-0009: Playback threading model](../docs/adr/0009-playback-threading-model.md)
- [ADR-0010: Shared media services](../docs/adr/0010-shared-media-services-for-playback-and-transcoding.md)
- [ADR-0011: Analyzer track results](../docs/adr/0011-analyzer-track-results.md)
- [ADR-0012: DSP processing failure](../docs/adr/0012-dsp-processing-failure.md)
- [ADR-0013: Indexer source identity and checkpoints](../docs/adr/0013-indexer-source-identity-and-checkpoints.md)
- [ADR-0014: Playback remote command lifetime](../docs/adr/0014-playback-remote-command-lifetime.md)
- [ADR-0015: Database behind the library API](../docs/adr/0015-database-behind-library-api.md)
- [ADR-0016: Independent application and plugin ABI versions](../docs/adr/0016-independent-application-and-plugin-abi-versions.md)
- [ADR-0017: Library scope and query lifetime](../docs/adr/0017-library-scope-query-lifetime.md)

- [ADR-0018: Library layers and plugin services](../docs/adr/0018-library-layers-and-plugin-services.md)

- [ADR-0019: Application media ownership](../docs/adr/0019-application-media-ownership.md)

- [ADR-0020: Playback position and mix points](../docs/adr/0020-playback-position-and-mix-points.md)
