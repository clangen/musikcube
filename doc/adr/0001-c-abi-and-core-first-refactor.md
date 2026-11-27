# ADR-0001: use a C ABI and migrate the core application API first

Status: accepted

Date: 2026-11-20

## Context

The goal is to make musikcore usable outside musikcube: as a shared library,
from different languages and runtimes, and eventually through plugins that do
not require a compatible C++ toolchain. The immediate deliverable is a complete
core application API that musikcube itself uses.

The existing C++ implementation is valuable and need not be rewritten. The
problem is its boundary. A consumer coupled to private C++ classes inherits
compiler and standard-library ABI requirements, allocation conventions, and
internal implementation changes. Plugin entry points with C linkage still
return C++ interfaces, so their names alone do not make the SDK usable from C.

The previous C API demonstrated embedding, but largely projected the C++ object
model. Ownership, errors, callback context, and threading were inconsistent,
and it did not expose the async query/cache behavior the app needed. Keeping
that previous API compatible is not required. That gives us room to design a coherent
replacement instead of accumulating exceptions around the old interface.

A shared library alone would not solve this. In particular, converting the
private core directly into a Windows DLL would expose C++/CRT compatibility
problems before a usable public contract existed. The new build foundation
makes it possible to validate a separate experimental artifact while keeping
the production app working.

## Decision and rationale

Use a versioned C ABI as the native interoperability boundary, keep C++ inside
musikcore, and expose domain operations rather than an internal class hierarchy.
C is the common calling interface supported by the intended native language
bindings; it lets those bindings share one ownership, error, and async model.
It does not require applications or the implementation to be written in C.

Opaque handles, fixed-layout sized structures, explicit destroy functions,
string views, status codes, and callbacks with user data make the contract
expressible without C++ types or cross-module allocation. Platform calling
conventions and exports remain deliberate and testable. C is not automatically
safe: the hardening and executable contracts are part of this decision.

The application API and provider SDK are separate contracts with opposite call
directions. Prove the application contract with musikcube before implementing and
migrating provider families. Otherwise we would multiply the surface under
change before either side had a complete production consumer.

Frontends, including musikcube and musikcubed, consume the production runtime
through the public C ABI and header-only wrapper. Private core dependencies would
let the primary consumers bypass the contract they are intended to prove. Use
one production runtime rather than a second narrow implementation for frontends.

## Alternatives considered

- Keep the public C++ interfaces and add bindings per language. This leaves
  compiler/toolchain coupling at the shared boundary and requires each binding
  to adapt the same private lifetimes and async behavior.
- Extend the previous C wrappers compatibly. Their missing async operations and
  inconsistent contracts would remain constraints even though compatibility
  with that API is not a requirement.
- Put all access behind a process/service protocol first. This can be useful
  later, but introduces deployment, transport, and serialization work before
  solving native embedding. Such a service can consume the C API afterward.
- Enforce the boundary only through source conventions. Transitive includes and
  implicit binary dependencies can bypass that convention; separately compiled
  consumers and import checks make the boundary executable.
- Rewrite the implementation in C. This adds risk without being necessary to
  expose a C ABI or remove C++ types from the public boundary.

## Consequences

The internals remain C++, while consumers depend on explicit C contracts instead
of their implementation hierarchy. The shared library and individual bindings
still need platform-specific validation; adopting C does not establish Windows
or Linux correctness by itself. The LLD records temporary adapters, build routes,
and migration gates rather than making them part of this boundary.

ABI0 remains experimental. ABI version negotiation is sufficient; a separate
contract fingerprint is not retained. Same-version development consumers and
libraries must be rebuilt together. Final compatibility commitments follow
consumer validation, rather than preventing useful design corrections now.

The [HLD](../design/musikcore-c-abi-hld.md) describes the architecture. The
[LLD](../design/musikcore-c-abi-lld.md#active-next-steps) owns current tasks,
foundation acceptance, and migration status; this ADR does not duplicate them.
