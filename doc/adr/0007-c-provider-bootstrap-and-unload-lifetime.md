# ADR-0007: use a C plugin boundary with session-owned lifetime

Status: accepted

## Context

Provider code must be callable without a matching C++ toolchain or allocator.
Safe loading also needs explicit rules for partial initialization, retained
factory tables, executing calls, and teardown. Merely exporting a C entry-point
name does not establish those rules.

## Decision and rationale

Keep the plugin C ABI separate from the application ABI, including independent
version negotiation. Providers receive a host table and register typed capability
tables transactionally: no capability is callable before initialization succeeds.
The engine owns the registry and each accepted native image for one session.
There is one plugin architecture, without an old-SDK fallback or runtime adapter.

Entry negotiation allocates no persistent session state. Initialization publishes
per-session state before further fallible work, and shutdown runs once after
initialization is entered, including failure. Host-owned errors copy diagnostics
so they do not retain provider memory after unload. Providers obtain services
through explicit tables rather than importing core implementation symbols.

Factory tables are non-owning session views. The runtime stops admission,
interrupts work, drains scoped callbacks, joins workers, and destroys instances
before provider shutdown and native unload on the owner thread. Individual live
plugin unload is unsupported. Independent application and plugin versions do not
imply compatibility of arbitrary releases or wire protocols.

Cleanup failure or lifecycle reentry that would unload executing code is fatal:
diagnose and terminate before further mutation or unload. A normal busy result or
catch-and-continue path cannot prove that provider workers stopped. Wrong-thread
lifecycle calls reject without disturbing the session. Expected provider and I/O
failures remain recoverable when a valid ownership state is preserved.

## Alternatives considered

C-linkage entry points returning C++ interfaces preserve toolchain coupling.
Parallel SDK architectures split discovery and cleanup across competing systems.
Publishing registrations incrementally exposes partially initialized factories.
Per-plugin leases and draining add a second lifetime protocol without replacing
ordered shutdown and can deadlock on handles needing owner-thread cleanup.
Keeping images until process exit avoids some unload races but does not prove
repeated embedding or session cleanup. One shared ABI version would conflate
independent compatibility boundaries.

## Consequences

Providers have one entry contract and a deterministic cleanup obligation.
The engine must establish quiescence; passive factory views neither retain images
nor delay shutdown. Header-only C++ helpers may generate C tables without adding
a compiled C++ ABI. Native plugins remain trusted code: timeouts or forced unload
cannot make a nonreturning provider safe.

The [SDK lifecycle design](../design/musikcore-c-abi-design.md#plugin-descriptor-and-lifecycle)
owns registry lookup, helpers, detailed call rules, and error handling. The LLD
owns migration, rebuild requirements, and native-platform verification.
