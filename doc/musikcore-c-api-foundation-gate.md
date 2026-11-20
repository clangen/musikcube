# musikcore core C API foundation gate

Status: active

Implementation evidence and remaining gaps:
[foundation progress](musikcore-c-api-foundation-progress.md).

## Purpose

This checklist defines the stabilization work that must finish before the
musikcore core C API gains more feature families or musikcube migrates more
functionality through it. It turns the current experimental surface into a
clean, deterministic foundation before its scope grows.

This gate covers the application-facing C API in `include/musikcore`, its
strictly header-only C++ wrapper, the ABI implementation, and their tests. It
does not begin the plugin/provider API. The plugin API remains a separate,
deferred initiative until the core C API is a release candidate.

Correctness fixes, test infrastructure, documentation, internal refactoring,
and changes required to make the existing surface consistent are in scope.
New preferences, indexer, audio-provider, decoder, encoder, output, DSP, or
other feature families are out of scope until this gate passes.

## Completion policy

- A checkbox represents verified repository state, not intent or partial work.
- Complete an item together with the contract test or other evidence that
  verifies it.
- Record platform-specific exceptions in this document with their rationale,
  owner, and replacement validation.
- Do not mark this gate complete based only on musikcube working interactively.
- Do not check additional feature or plugin/provider tasks in the low-level
  plan until this gate passes.

## 1. Reconcile the current contract

- [x] Inventory every installed core C header, exported `mc_*` symbol, public
  structure, callback, opaque handle, and constant.
- [ ] Reconcile the inventory with the detailed design and low-level task plan;
  correct tasks that are checked prematurely or whose implementation changed.
- [ ] Approve the `mc_` prefix, installed header names, and experimental shared
  library name.
- [x] State which existing calls are experimental, production-bound, local
  only, remote capable, or unsupported by the standalone library.
- [x] Ensure public headers compile independently as C11 and C++ without
  private include paths.
- [x] Ensure the public umbrella includes every application-facing subsystem
  and no plugin/provider declarations.
- [x] Verify that public declarations use only fixed-width or explicitly sized
  C-compatible types and export no new C++ ABI.

## 2. Normalize C API ergonomics

- [x] Define one callback argument order and apply it to every existing async
  completion, including playlist operations.
- [x] Define when detailed `mc_error` objects are present and make error
  reporting consistent across existing async operations.
- [x] Use consistent names for context, callback data, output handles, request
  IDs, counts, offsets, and limits.
- [ ] Document nullability for every pointer parameter and output slot.
- [ ] Document every transfer, retained reference, and borrowed view using one
  ownership vocabulary.
- [ ] Verify all async inputs are copied or retained before the initiating call
  returns.
- [x] Verify every successful callback payload has one obvious release path and
  every borrowed callback payload has an explicit lifetime.
- [x] Review sentinel-returning accessors for ambiguity; replace them or
  document why the sentinel cannot represent valid data.
- [x] Provide an `MC_*_DEFAULT` initializer for every public structure with a
  `struct_size` field and document all fields, units, defaults, and reserved
  values.
- [x] Write a plain-C golden-path example that creates a context, opens a
  library, submits a query, dispatches it, reads results, and shuts down without
  unnecessary bookkeeping.
- [x] Review the golden path as an API design test and correct avoidable
  ceremony before treating the example as documentation.

## 3. Harden context, request, and dispatch behavior

- [ ] Put common argument validation and C++ exception barriers around every
  exported implementation entry point.
- [x] Verify context creation publishes no handle until required initialization
  succeeds.
- [x] Test cleanup after failure at each context initialization phase.
- [x] Define and test the dispatcher-thread binding rule.
- [x] Define and test nested and concurrent dispatch behavior.
- [ ] Guarantee exactly one terminal callback for every accepted request across
  success, error, cancellation, and shutdown races.
- [x] Verify callbacks may safely submit and cancel requests where documented.
- [x] Verify request IDs cannot be confused with zero or reused while a queued
  completion can still refer to them.
- [x] Verify wakeups are coalesced and correctly rearmed when producers race
  with the final drain.
- [x] Verify shutdown stops admission, settles accepted requests, stops wakeup
  hooks, and drains terminal callbacks on the dispatcher thread.
- [x] Test repeated create, dispatch, shutdown, and destroy cycles.
- [x] Test destroy from valid and invalid states, including calls from inside a
  callback.
- [x] Test and document the experimental single-context `MC_ERROR_BUSY`
  behavior.

## 4. Harden existing library and result contracts

- [x] Centralize legacy-query conversion into owned immutable result snapshots;
  conversion code must not borrow through temporary smart-pointer owners.
- [ ] Verify result membership, ordering, duplicates, album boundaries, and
  originating-library identity remain stable for the result lifetime.
- [ ] Verify result, page, track-reference, and library handles retain exactly
  the state promised by their public lifetime contracts.
- [ ] Test invalid rows, invalid fields, missing values, explicit empty values,
  Unicode, long strings, multi-value fields, and removed tracks.
- [ ] Test empty, default-sized, maximum-sized, and out-of-range page requests.
- [ ] Verify local and remote implementations return the same public semantics
  where both claim support.
- [x] Return `MC_ERROR_UNSUPPORTED` predictably where a backend cannot honor a
  documented operation or projection.
- [ ] Test stale library generations, reconnects, cancellation, and shutdown
  while local and remote reads are pending.
- [x] Verify playlist create, overwrite, append, rename, delete, open, ordering,
  and duplicate preservation through public entry points.

## 5. Harden existing playback and queue contracts

- [x] Define owner-thread requirements consistently for every playback and
  queue operation.
- [ ] Verify playback snapshots have coherent units, ranges, defaults, and
  unavailable-value behavior.
- [x] Verify queue snapshots remain immutable and stale snapshots fail without
  mutating the live queue.
- [ ] Test stopped, retained, prepared, paused, and playing row semantics.
- [ ] Test queue replacement, clear, move, remove, play-row, play-next, prepend,
  append, shuffle, and duplicate-track behavior.
- [x] Verify queue-change callbacks run outside engine signal locks, are
  coalesced as documented, and can be replaced or removed safely.
- [x] Verify playback operations reject track identities from stale or
  unavailable library sessions.

## 6. Keep the C++ wrapper thin and predictable

- [x] Keep `musikcore.hpp` and all subsystem wrappers strictly header-only.
- [ ] Verify the wrapper depends only on installed C headers, exports no C++
  symbols, and introduces no runtime behavior absent from the C API.
- [x] Declare and document copy and move behavior for every owning wrapper.
- [ ] Protect borrowed views from use after their owning page or snapshot is
  destroyed or moved.
- [x] Choose and consistently apply the wrapper's exception, result, optional,
  and expected-absence policy.
- [x] Add a move-only request/cancellation wrapper, or document why raw request
  IDs remain preferable for the experimental surface.
- [x] Ensure callback state is released on submission failure, terminal
  completion, cancellation, and shutdown.
- [x] Write a concise C++ golden-path example covering the same flow as the C
  example.
- [x] Keep futures, coroutines, and runtime-specific adapters optional and
  header-only if they are introduced.

## 7. Make tests deterministic and hermetic

- [x] Provide shared cross-platform temporary-directory and isolated preference
  support.
- [x] Give each test isolated database, plugin, configuration, and runtime
  directories.
- [x] Provide deadline-based event and predicate waiting without arbitrary
  sleeps.
- [ ] Replace generic completion waits with operation-specific tokens,
  predicates, or completion objects.
- [ ] Provide deterministic memory streams and fake/null playback output.
- [x] Provide one RAII fixture for debug, plugin, library, worker, and message
  queue initialization and shutdown in production order.
- [x] Ensure the default suite does not require user data, user preferences,
  physical audio hardware, internet access, or production service ports.
- [x] Keep loopback transport fixtures self-contained and use dynamically
  selected ports.
- [ ] Run every reported crash through a focused regression test that fails by
  contract assertion rather than relying only on a process crash.
- [x] Document one canonical clean configure, build, and CTest workflow.

## 8. Exercise the actual binary boundary

- [x] Build a separately compiled C11 consumer using only installed/public
  headers and the experimental shared library.
- [x] Verify the exact public export allowlist and reject leaked private or C++
  symbols.
- [x] Prevent one executable from linking the production and experimental core
  implementations together.
- [ ] Give the experimental ABI isolated runtime and plugin output directories.
- [ ] Test clean out-of-tree Debug and Release builds on Windows, macOS, and
  Linux.
- [ ] Test Windows DLL/import-library loading and runtime placement.
- [ ] Test ELF SONAME/RPATH behavior used by development artifacts.
- [x] Test Mach-O install names and runtime search paths used by development
  artifacts.
- [ ] Confirm a clean consumer gets an actionable error for an incompatible or
  stale experimental binary.

## 9. Establish performance and allocation baselines

- [ ] Record baseline latency for context creation, initial query, page reads,
  large playlist opening, queue replacement, and shutdown.
- [ ] Measure allocations and copied bytes for strings, metadata pages, track
  results, playlist operations, and C++ callback state.
- [ ] Verify paging and cache windows bound memory while scrolling large
  libraries.
- [ ] Verify the wrapper does not introduce metadata or string copies beyond
  its documented ownership conversions.
- [ ] Compare local and loopback-remote behavior using deterministic fixtures.
- [ ] Define regression tolerances and store reproducible benchmark commands
  with the results.

## Gate acceptance

The core C API foundation gate passes only when:

- [ ] Sections 1 through 9 are complete, or each recorded exception has an
  approved replacement validation.
- [ ] The canonical clean workflow passes on Windows, macOS, and Linux in the
  named Debug and Release configurations.
- [ ] Required contract, integration, export, and external C consumer
  validations pass.
- [ ] Performance and allocation baselines are recorded with reproducible
  commands and regression tolerances.
- [ ] The C and C++ golden paths are concise, documented, and tested.
- [ ] The current ownership, threading, callback, error, and shutdown contracts
  are internally consistent and match their executable tests.
- [ ] The low-level task plan is reconciled with the verified repository state.

After this gate passes, work may resume on additional core application API
features. The plugin/provider API remains deferred until the core C API reaches
release-candidate quality and the high- and low-level plans record that revised
sequencing explicitly.
