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

Lifecycle belongs to the loading/runtime owner thread. Capability acquisition
and admitted calls may run on workers. Shutdown first prevents new acquisition
and invocation, then waits for retained references and admitted calls. Only
after they drain does it invoke provider shutdown and release the native image.
A scoped internal lease pins both the table and code during calls, including
when a callback drops the caller's original reference. Provider workers must
stop before their shutdown callback returns.

Owner-thread lifecycle reentry from a provider call is rejected rather than
waiting for itself. Wrong-thread lifecycle calls report a distinct error and
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
- Import core helpers directly from providers. This makes bootstrap depend on
  loader paths and exported implementation symbols. A host table supplies the
  required services explicitly.
- Publish registrations as initialization progresses. A failed initializer
  would leave partially usable factories and complicated rollback ownership.
- Unload when the plugin record is removed. A factory table or executing call
  can still point into that image. Removing discoverability is not reclamation.
- Keep all providers mapped until process exit. This avoids some unload races
  but leaves session cleanup and repeated embedding unproven, and does not
  release provider-owned state at the runtime boundary.
- Force all capability use onto the owner thread. This conflicts with decoder,
  stream, and other worker-side operations. Restrict lifecycle and registration
  instead, with explicit call admission and draining.

## Consequences

Provider authors have one entry point, explicit ownership, one initialization
transaction, and a deterministic shutdown obligation. Every typed family must
retain its provider lease while its factories, objects, and calls can be used.
This lifetime machinery is not a real-time dispatch mechanism: playback must
not acquire registry locks, allocate leases, or perform lifecycle work in an
audio callback.

The host can validate malformed tables and contain accidental C++ exceptions,
but native plugins remain trusted code, not a security sandbox. A provider that
never returns or fails to stop its workers can still prevent safe shutdown.
Neither a timeout nor forced unload makes executing native code safe.

Public application calls continue to expose music-domain operations. They do
not expose generic provider pointers or require clients to manage native
images. Typed stream/decoder contracts remain responsible for their own frame,
I/O, cancellation, and instance ownership rules.
