# ADR-0008: C provider bootstrap and unload lifetime

## Status

Accepted.

## Context

The application C API makes musikcore usable across runtimes, but it does not
make provider interfaces portable. A decoder or other provider must be callable
by the core without sharing C++ vtables, compiler-specific object layouts, or
allocation conventions. Loading a DLL safely also requires an answer for failed
initialization, retained factory tables, calls already executing, and teardown.
A C entry point alone does not provide those guarantees.

## Decision

Keep provider declarations in `musikcore/plugin.h`, separately from the
application umbrella header. Each provider exports `mc_plugin_get_api` and
receives a sized, versioned host table. The entry returns a sized plugin table
with identity, metadata, and initialization/shutdown callbacks. The provider
ABI has its own version; it need not advance with the application API.

The engine has one plugin architecture. `PluginRegistry` discovers candidates,
identifies and initializes them transactionally, and uniquely owns accepted
`Plugin` records for the engine session. `Plugin` handles one DLL's entry table,
state, and cleanup. Engine code consumes the C contracts, optionally through
private C++ wrappers. There is no runtime fallback or interface adapter to a
different C++ SDK. Loading and ownership share the same thread and session, so
one registry keeps publication and lifetime rules together without a separate
loader that mutates registry internals. Failed candidates still have one cleanup
path through their temporary `Plugin` owner.

The caller-facing registry initializes one directory once, lists copied
`PluginInfo` values, finds typed factories, and shuts down. Discovery and
single-file loading stay private. `Find<T>(pluginId)` maps supported C factory
types to their capability IDs internally and rejects unsupported types at
compile time; callers cannot pair an arbitrary ID with an unrelated table
cast. Generic string IDs remain in C registration for extensibility, not in
the normal engine lookup API. Individual candidate failures are logged and
skipped; directory/session failures return a status. This keeps orchestration,
validation, and diagnostic details behind the registry's session contract.

Missing optional families are valid: DSP bypasses, absent observers receive no
events, and unavailable encoding/decoding returns unsupported. Headless output
is supplied by the bundled `nullout` C plugin, loaded through the same entry and
lifetime rules as native outputs. This keeps the core free of a separate null
backend and makes a real plugin prove output ownership and teardown. Its purpose
is paced discard on the calling producer worker, with an interruptible wait
and no owned thread, queue, or device-timing estimator. Output delivery may block
on a producer worker; Stop must interrupt it and quiesce callbacks. Real-time
hardware callbacks remain nonblocking. Without an
output plugin, the engine remains usable for queries and idle operation, while
playback reports unavailable. Null behavior must not fabricate successful work.

C++ plugin authors can use optional header-only SDK helpers to generate these C
tables from implementation classes. The output helper registers one stateless
factory, creates/destroys typed instances, and translates ordinary implementation
exceptions without private headers or core linkage. The host validates inputs
and owns stop/reprepare/error-cleanup/destruction order; thin helpers trust that
contract rather than injecting validation or lifecycle calls. Tests must prove
checks and ordering at the owning engine boundary before downstream code runs.
Queue methods must be noexcept so failure cannot be reported after PCM was
accepted; cleanup must be noexcept, and the owning engine must not delete/unload state
unless cleanup succeeded and calls/callbacks have finished.
No C++ base class, object layout, vtable, or exception crosses the plugin ABI.
This keeps handwritten C/C++ forwarding code out of plugins without introducing
another runtime architecture. Other language/family helpers follow real callers.

Bootstrap permits logging and host-owned error creation/destruction. Providers
use callback tables rather than importing core implementation functions. The
core copies diagnostic text and owns error allocation/destruction, so errors
can survive native unload without referencing provider memory.

Initialization registers capability descriptors. Registration copies their
identifiers while the immutable typed tables remain provider-owned. The
provider ABI version governs those tables; table byte sizes protect prefix
access. Identifiers route capabilities, while each provider family defines and
validates its actual function table. This registration mechanism is internal
provider extensibility, not an untyped function-call API for applications.

Registration is transactional: no capability is callable before initialization
succeeds. Entry allocates no persistent engine state needing rollback.
Initialize receives the host table and returns opaque per-session state through
an output slot. It assigns that slot before further fallible work. Shutdown
receives the returned state and runs exactly once after initialization is
entered, including on failure; it accepts null for failed allocation. Rejected
negotiation invokes neither lifecycle callback.

Returning state from initialize avoids requiring entry-time allocation or a
mutable global host pointer. Two host sessions sharing one mapped image can own
separate state and callbacks. This does not change the application runtime's
existing process-wide ownership policy.

Plugin initialization registers factories and allocates per-plugin state.
Scoped, typed engine services are supplied when family instances are created;
active workers belong to those instances or lifecycle extensions started after
required services are ready. The runtime stops active work, interrupts I/O,
cancels/drains scoped host callbacks through owner dispatch, joins workers, and
destroys instances before final registry teardown. In particular, the owner
must not join a worker that still needs an owner-thread completion.

Lifecycle belongs to the loading/runtime owner thread. The registry uniquely
owns every accepted plugin until engine-session teardown; individual plugin
unload during a running session is not supported. Capability values are
non-owning table views, valid only during that session. They do not retain the
DLL, count workers, or delay shutdown. After the engine quiesces users and destroys
instances, registry shutdown invokes provider cleanup and unloads on the owner
thread without waiting for handles. Provider workers must stop before their
shutdown callback returns.

This ownership order matches actual use: plugins serve the whole engine run.
Per-plugin leases and draining would permit finer unload decisions but would
not stop workers, finish callbacks, or destroy instances. Requiring that same
ordered cleanup while also waiting for arbitrary retained handles adds a
second lifetime mechanism and can deadlock shutdown. Session ownership keeps
that responsibility in one place.

A private per-owner call state detects same-thread lifecycle reentry while
plugin calls or host callbacks execute. Nested scopes restore the previous state;
workers never read or modify the owner-only flag. Independent registries do not
block one another. This state does not count workers or establish quiescence.
Harmless empty/completed cleanup is allowed. Reentry that invalidates an active
transaction or unloads executing code is a fatal contract violation: diagnose
and terminate before mutation, in Debug and Release. Returning a normal busy
status would hide a lifecycle bug that the host must fix. The state covers
native DLL destructors as well as explicit plugin shutdown.

Exception policy is separate from the reentry guard. Actual plugin entry and
initialization exceptions become contextual plugin errors; host factory-callback
exceptions propagate normally, and selection callback failures are host errors.
A throwing shutdown is a fatal contract violation before native unload: catching
it and continuing cannot establish that workers stopped. Retaining failed
images would require another lifetime mechanism without making the engine safe
to continue, so cleanup failure does not return as a recoverable plugin error.

Owner-thread lifecycle reentry into the same live session terminates rather than
waiting for itself or pretending recovery succeeded. Wrong-thread lifecycle calls report a distinct error and
leave the session intact. Destruction on an invalid thread or from an active
provider call cannot safely unload code and is a contract violation.

Discovery selects the contract by its entry point. Finding the C entry point
commits to C negotiation; failure does not silently fall back to another
interface in the same library. Native image constructors are not lifecycle
callbacks: discovery may map an image to inspect its exports. Engine resources
belong in initialize/shutdown.

## Alternatives considered

- Export C++ interfaces behind C-linkage entry names. This preserves compiler,
  standard-library, and allocator coupling at the boundary.
- Keep different plugin SDK architectures active together and bridge them.
  This splits discovery, ownership, configuration, and teardown across systems
  and makes temporary coupling part of the engine's interface model.
- Import core helpers directly from providers. This makes bootstrap depend on
  loader paths and exported implementation symbols. A host table supplies the
  required services explicitly.
- Publish registrations as initialization progresses. A failed initializer
  would leave partially usable factories and complicated rollback ownership.
- Unload when the plugin record is removed. A factory table or executing call
  can still point into that image. Removing discoverability is not reclamation.
- Support independent plugin unload using capability leases and call counters.
  This adds lifetime coordination without replacing ordered engine teardown,
  and can block the owner on handles whose cleanup needs that same thread.
  There is no current requirement for unloading individual plugins mid-session.
- Keep all providers mapped until process exit. This avoids some unload races
  but leaves session cleanup and repeated embedding unproven, and does not
  release provider-owned state at the runtime boundary.
- Force all capability use onto the owner thread. This conflicts with decoder,
  stream, and other worker-side operations. Restrict lifecycle and registration
  instead, with engine-owned quiescence before unload.

## Consequences

Provider authors have one entry point, explicit ownership, one initialization
transaction, and a deterministic shutdown obligation. Every typed family must
finish calls/callbacks and destroy its instances before registry teardown.
Factory-view copies are cheap non-owning views; using one after shutdown is a
contract violation. Session shutdown does not wait for passive views or detect
concurrent misuse. Engine-owned ordering is essential and needs integration
tests as asynchronous families are connected. Playback must not acquire registry
locks or perform lifecycle work in an audio callback.

The host can validate malformed tables and contain accidental C++ exceptions,
but native plugins remain trusted code, not a security sandbox. A provider that
never returns or fails to stop its workers can still prevent safe shutdown.
Neither a timeout nor forced unload makes executing native code safe.

Public application calls continue to expose music-domain operations. They do
not expose generic provider pointers or require clients to manage native
images. Typed stream/decoder contracts remain responsible for their own frame,
I/O, cancellation, and instance ownership rules.
