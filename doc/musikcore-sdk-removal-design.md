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

The following are concrete starting points, not a claim that every caller has
been mapped. Search includes, inherited types, aliases, virtual overrides,
callbacks, wrapper construction, exports, tests, and build manifests for each
slice before changing it.

| Area | Remaining SDK types and machinery | Intended direction |
| --- | --- | --- |
| Audio | `IBuffer`; `audio::Buffer`, stream/player/output handoff and recycling | Use an owned internal PCM buffer and explicit borrowing/return. Preserve token ownership, ordering, bounded allocation and cancellation. |
| Playback and queue | `IPlaybackService`, `ITrackList`, `ITrackListEditor`, `ITrack`; playback helpers and editor proxies | Use concrete playback operations and task-oriented queue editing with ownership and locking at the owning layer. Preserve remote/local backend behavior and public callback ordering. |
| Metadata and library | `IResource`, `IMap`, `IMapList`, `IValue`, `IValueList`, `ITrack`, `ITagStore`; `GetSdkValue()`, `SdkWrapper`, `Retain()`/`Release()`, query SDK wrappers | Replace SDK-shaped object graphs with internal owned data and typed query/results as appropriate. Remove wrapper allocations and manual lifetime protocols only after all actual callers move. |
| Preferences | `IPreferences`, `Preferences::Unmanaged()` and plugin-era accessor surface | Use the existing concrete preference store and scoped C services. Determine actual callers and ownership before removing transient/unmanaged behavior. |
| General helpers | `DataBuffer`, filesystem and string utilities, HTTP helper, replay-gain definitions, constants and version | Keep useful shared functionality in a suitable private/common location for real core and in-tree plugin users. Remove unused or SDK-specific surfaces; do not delete functioning utilities solely due to their directory. |

Some in-tree C plugins still include SDK-directory utility headers. Those are
implementation dependencies, not old provider ABI entry points. Moving a helper
must account for Windows, macOS, Linux, standalone packaging, and the plugins
that use it. C++ convenience wrappers under `include/musikcore/plugin/` are the
new supported wrappers and are not part of this removal.

## Replacement approach

1. **Inventory and characterization.** Trace each interface from creation to
   last use, ownership transfer, shutdown, thread and backend boundaries. List
   callers that actually require polymorphism and characterize behavior through
   existing public/runtime entry points. Identify wrapper allocations, retain/
   release pairs, proxy editors, duplicate representations and lookup caches;
   keep only those justified by an observed requirement.
2. **Leaf cleanup.** Remove unused preference and helper surfaces after tracing
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
preference changes. Test these behaviors rather than testing the replacement
class layout. Do not promise performance gains without measuring representative
paths. Physical-device and platform-specific validation cannot be inferred from
a Linux headless test.
