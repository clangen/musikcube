# musikcore C ABI modernization: high-level design

Status: active migration; foundation stabilization in progress

## Current sequencing decision

The [core C API foundation gate](musikcore-c-api-foundation-gate.md) takes
precedence over the original sequence below. Finish that gate before adding
feature families or migrating further application functionality. Then finish
the core application API and validate musikcube as its consumer to reach core
release-candidate quality. Begin the plugin/provider API only afterward.
Experimental ABI0 remains unfrozen throughout this work. Final ABI v1 freeze
still follows validation of both surfaces.

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

musikcore currently has two boundary mechanisms:

- A C API that forwards through opaque values into C++ interfaces.
- Plugin entry points with C linkage that return C++ interface pointers.

Neither is a suitable long-lived, cross-language binary contract. The current
C API has ambiguous ownership, inconsistent errors, incomplete async behavior,
and callback/threading hazards. The plugin entry points avoid C++ symbol
mangling, but still expose C++ vtables, allocation behavior, and compiler ABI.

The current build topology also differs by platform. Unix-like builds currently
produce a shared musikcore library for the application, while Windows links
musikcore statically. The existing Unix shared library is an internal build
artifact, not the proposed stable C ABI.

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

### Stage A: current production topology

```text
Windows musikcube ──> current static musikcore
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

The legacy path remains available to the normal build until the new public
surface covers the required application behavior.

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
- Request dispatch, wakeups, and notification polling.
- Library queries and metadata pages.
- Track lists and async cache windows.
- Playback and queue commands and snapshots.
- Indexer and preferences.
- Advanced audio handles where justified.

### Plugin host

The plugin host owns:

- Dynamic library discovery and loading.
- One versioned plugin entry point.
- Host service tables.
- Capability and factory registration.
- Provider instance tracking.
- Shutdown barriers and unload eligibility.
- Translation between provider C tables and private C++ interfaces.

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

### 1. Test infrastructure without a DLL conversion

Add CTest, isolated temporary state, a small C harness, and focused internal
tests. Preserve the current Windows static target and Unix production topology.

Gate: tests run on all supported build systems without changing how musikcube
links musikcore.

### 2. Shared ABI foundation

Define and test symbol macros, calling convention, versioning, results, errors,
strings, sized structures, opaque ownership, and exception containment.

Gate: the public headers compile as C11 and C++, and foundation behavior passes
in a static test configuration.

### 3. Experimental shared façade and walking skeleton

Only now add an experimental shared target. Expose context creation, version
query, event retrieval, and destruction. Add the plugin entry point and one
trivial fixture capability.

Gate: a separate C process crosses the shared-library boundary and loads a
separate C plugin on Windows, macOS, and Linux without leaking or exporting
private C++ symbols.

### 4. Metadata/query public vertical slice

Implement async requests, immutable metadata pages, track identities, cache
windows, and invalidation events. Add the C++ wrapper and migrate one
representative musikcube list view.

Gate: search and scrolling remain nonblocking and show no meaningful
performance regression.

### 5. Decoder SDK vertical slice

Implement stream, buffer, decoder-factory, and decoder tables. Prove the design
with a pure-C deterministic decoder and the real playback pipeline using a fake
output.

Gate: known PCM frames flow through the C SDK with correct ownership, failures,
teardown, and plugin unload.

### 6. Expand and migrate the public API

Add preferences, remaining metadata views, playback, queue editing, indexer,
and other application-required behavior one vertical slice at a time. Each
slice lands with contract tests, wrapper support, and a migrated consumer.

Gate: musikcube no longer includes or links private musikcore application
interfaces.

### 7. Expand and migrate the provider SDK

Add data-stream factories, encoders, tag readers, indexer sources, outputs,
DSPs, and secondary plugin categories. Each category begins with a fixture and
then migrates one simple production plugin before wider conversion.

Gate: in-tree plugins no longer return current C++ SDK interfaces across module
boundaries.

### 8. Integration, packaging, and compatibility

Run application smoke tests, installed external consumers, installed external
plugins, dependency scans, runtime path checks, and archive tests.

Gate: staged release artifacts work outside the source and build trees on all
supported platforms.

### 9. ABI v1 freeze

Record exported symbols and public layouts, retain prior-version executable
fixtures, assign platform ABI metadata, remove legacy boundaries, and publish
the supported surface.

Gate: older v1 consumer and plugin fixtures work with a newer compatible v1
library without recompilation.

## Windows-specific strategy

- Do not convert the current Windows production core target to a DLL before the
  minimal C ABI foundation is complete.
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

Mitigation: track factories and instances, stop new creation before shutdown,
wait for in-flight calls, and unload only after all references are gone.

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

## Decision summary

musikcore will adopt one stable C ABI foundation with two distinct surfaces: a
public application API and a provider SDK. Testing begins at those boundaries.
The current Windows static build is preserved until the minimal C ABI exists;
the first new shared artifact is an explicitly experimental façade used by
contract tests, not a DLL conversion of the existing public C++ surface.

Migration proceeds through tested vertical slices. The final shared library
becomes the normal musikcore artifact only after musikcube and production
plugins have moved to the C surfaces and all supported platforms pass package
and compatibility tests.
