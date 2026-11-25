# musikcore C ABI modernization: low-level design and task plan

This is the index for the C ABI low-level design and task checklist. The
[HLD](musikcore-c-abi-hld.md) and its ADRs record architecture; the public
headers define the ABI. Task checkboxes in the linked documents record the
implementation and platform evidence, including open gates. A checked task
only covers the scope stated beside it.

The C plugin registry now owns plugin discovery. The old C++ plugin factory,
entry hooks, and plugin-facing interfaces have been removed. Core code still
uses several former SDK interfaces and wrappers; their removal is planned in
[the SDK removal design](musikcore-sdk-removal-design.md). No old plugin
compatibility path is planned.

## Document map

- [Foundation and runtime design](musikcore-c-abi-lld-foundation.md): source and
  build layout, component contracts, testing infrastructure, foundation gate,
  and validation baselines.
- [Core migration tasks](musikcore-c-abi-lld-core.md): decisions and phases
  0–3 and 5–7, including application API and runtime hardening.
- [Plugin migration tasks](musikcore-c-abi-lld-plugins.md): plugin architecture,
  provider families, remaining SDK removal, and their acceptance criteria.
- [Release and cleanup gates](musikcore-c-abi-lld-release.md): phases 9–10,
  historical next steps, and progress reporting rules.
- [SDK removal design](musikcore-sdk-removal-design.md): inventory, intended
  internal replacements, sequencing, risks, and validation for the remaining
  SDK-era object graph and its incidental complexity.

The checklists include completed work and historical bootstrap evidence. Their
phase numbers do not by themselves establish execution order, and open platform
checks do not become complete merely because implementation boxes are checked.
Do not discard or reclassify a gate without checking its actual evidence.

## Checklist audit

An initial checklist audit compared the migration entries with current source
and Linux integration results after the plugin-boundary cleanup. The C provider catalog, provisional provider shapes, registry cutover,
configuration descriptor, and shared source-manifest tasks are now marked complete with named tests or concrete build wiring.
The TagLib, indexer, MPRIS, and playback-remote entries now record the Linux
checks that passed. Converted Windows DirectSound and visualizer providers are
explicitly distinguished from their still-open native validation.

The remaining unchecked items include actual unfinished core SDK removal,
ABI-freeze and CI work, platform and physical-device validation, and deliberate
future expansions. They are not all unimplemented migrations. Historical
staging text and task IDs are preserved for traceability; the checkboxes and
remaining conditions in each entry govern acceptance.

The most recent broad Linux integration run passed 183 of 190 tests. Its URI
player fixture was corrected and passed on rerun; the MPRIS load test passed
when the core library was on the loader path. Five failures remain to investigate:
backend parity, paused crossfade, a PulseAudio fixture with an unresolved
`pa_blocking_flush` symbol, and two audio-service tests unable to publish their
isolated sink. These results do not establish whether the first two are
regressions. No Windows or macOS validation is claimed for this cleanup.

## Active next steps

1. Use the [SDK removal design](musikcore-sdk-removal-design.md) and the
   [SDK removal tasks](musikcore-c-abi-lld-plugins.md#remaining-sdk-removal)
   to replace the remaining internal interfaces in small, testable slices.
2. Preserve the named runtime, provider, consumer, and platform acceptance
   checks in the linked task documents; track unresolved failures and missing
   native validation explicitly.
3. Reconcile stale historical checklist entries with evidence as their areas
   are touched. Do not treat the raw number of checked boxes as a release gate.

## Core C API foundation gate

The authoritative [foundation gate and its open exceptions](musikcore-c-abi-lld-foundation.md#core-c-api-foundation-gate)
remain in force.

## Current experimental core contracts

See [current experimental core contracts](musikcore-c-abi-lld-foundation.md#current-experimental-core-contracts).

## Runtime component design

See [runtime component design](musikcore-c-abi-lld-foundation.md#runtime-component-design).

## Library, result, and playback semantics

See [library, result, and playback semantics](musikcore-c-abi-lld-foundation.md#library-result-and-playback-semantics).

## Approved execution order

See [plugin execution order](musikcore-c-abi-lld-plugins.md#approved-execution-order),
with its completed and still-open work distinguished in the checklist.

## Related documents

- [Detailed public API and C SDK design](musikcore-c-abi-design.md)
- [Testing design](testing-design.md)
- [Migration journal](musikcore-migration-journal.md)
