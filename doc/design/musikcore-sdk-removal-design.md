# Remove the remaining C++ SDK object graph

Status: proposed implementation design. The C plugin ABI and its header-only
C++ convenience wrappers remain the supported plugin boundary. The old C++
plugin factory and provider interfaces have already been removed; the work
here removes the former SDK abstractions that still shape core internals and
any incidental machinery they introduced. The [LLD tasks](musikcore-c-abi-lld-plugins.md#remaining-sdk-removal)
track implementation and acceptance. Existing [C ABI design](musikcore-c-abi-design.md),
[HLD](musikcore-c-abi-hld.md), and ADRs remain authoritative for public contracts.

## Goal and boundaries

Remove the remaining `src/musikcore/sdk/I*.h` interfaces and SDK-era ownership,
adapters, and indirection when their original reason has disappeared. Make
internal APIs express the owning subsystem's actual needs. Do not recreate the
same hierarchy under another name or route private calls through the public C
ABI merely to eliminate a C++ type. Keep abstractions that still serve a real
boundary, multiple backends, or testable behavior, and explain that purpose.

Preserve C API behavior, plugin ABI contracts, local and remote library behavior,
audio real-time constraints, and persisted identities. Do not retain C++ plugin
compatibility hooks. Any necessary C ABI change must be surfaced with its
compatibility impact as required by repository guidance.

## Current inventory

The former `sdk/I*.h` interfaces and their plugin-era wrappers have been
removed. Audio buffers and playback operations now use their internal owners;
track, metadata, category and album results use concrete objects or standard
containers. Local and remote library backends still need their internal
polymorphic boundary. The C API and plugin ABI are unchanged.

The former SDK utilities have been moved to internal shared code. The HTTP
client remains private because it depends on libcurl; filesystem, string and
version helpers are also private. Unused SDK constants are removed; remaining
playback and indexing types belong to their internal subsystems. The fixed-capacity
ring queue is available as an installed, header-only plugin utility. The old
`ReplayGain` type has been replaced by `mc_replay_gain_values`, and the LAME
encoder uses standard vectors in place of `DataBuffer`.

The scoped retired-SDK source check prevents the removed interfaces and plugin
entry points from returning. Native-platform and packaging acceptance remains
open in the LLD; Linux test results do not establish those guarantees.

## Replacement approach

1. **Inventory and characterization.** Trace each interface from creation to
   last use, ownership transfer, shutdown, thread and backend boundaries. List
   callers that actually require polymorphism and characterize behavior through
   existing public/runtime entry points. Identify wrapper allocations, retain/
   release pairs, proxy editors, duplicate representations and lookup caches;
   keep only those justified by an observed requirement.
2. **Leaf cleanup.** Remove unused setting and helper surfaces after tracing
   all platforms. Move genuinely shared implementation helpers to a clearly
   owned internal/common location, with direct includes and no SDK umbrella.
3. **Audio ownership.** Convert buffer storage and its stream/player/output
   handoff together. An output can borrow a buffer until its completion or
   cancellation; the owner must keep storage alive and recycle it exactly once.
   Keep locks, allocation and blocking I/O off real-time callbacks. Preserve the
   existing fixed pool and backpressure behavior unless measurement supports a
   change.
4. **Playback and track lists.** Replace interface calls with operations owned
   by the playback or list implementation. Reassess the editor proxy and each
   public method instead of copying the virtual surface. Maintain queue snapshots,
   edits, notifications, crossfade/gapless transitions and cancellation.
5. **Metadata and query graph.** Work from typed query boundaries through local
   and remote implementations. Remove SDK wrappers and manual resource
   lifetimes alongside callers; preserve custom metadata keys, value lifetimes,
   artwork, track identity, paged snapshots and bound SQL. Do not specialize
   generic database wrappers for application queries.
6. **Erase the boundary.** Remove unused interfaces, aliases, wrappers, source
   files, PCH entries, and build/install references. Add a scoped source/build
   boundary check to prevent the old plugin entry points and removed interfaces
   from returning. Verify both normal and testing-disabled builds.

The order is a starting dependency order, not a requirement to replace one
interface in isolation. A slice can cross these areas when that removes an
adapter safely. Prefer a small number of coherent changes over transitional
compatibility layers.

## Acceptance and risks

Each slice adds or updates a meaningful regression at the boundary it changes;
for a reproduced bug, first demonstrate the test fails before the fix. Run
existing focused tests through real consumers and providers, including lifecycle
and cancellation; use gates and bounded deadlines for races. Run platform
specific builds and tests on their native platforms and leave missing evidence
open in the LLD. Finish with the installed external C/C++ consumer and plugin
checks, a full application build, and relevant standalone dependency scanning.

The highest risks are buffer reuse while an output still borrows data, stale
track or metadata references during async work, lost remote-backend routing,
changed queue/editor notification order, and inadvertent persisted identity or
setting changes. Test these behaviors rather than testing the replacement
class layout. Do not promise performance gains without measuring representative
paths. Physical-device and platform-specific validation cannot be inferred from
a Linux headless test.
