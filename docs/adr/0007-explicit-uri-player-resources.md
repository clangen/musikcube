# Explicit URI playback resources

## Status

Accepted.

## Context

The catalog playback API consumes origin-bound library results and queues. A
host playing an arbitrary file or stream should not need a fabricated track,
a database, an indexer, or the application playback session. The existing audio
transport and decoders already operate on URI strings.

## Decision

Expose an explicitly constructed, owned C `mc_player` resource attached to a
context providing Plugins. Each URI player owns its output and runs one selected track through the
shared playback worker implementation. Catalog transports handle queues and
track transitions, neither of which a single URI requires. The player uses
context wakeups and dispatch for foreign callbacks and requires no catalog services. Its C++
wrapper remains header-only over that C contract. Default contexts still enable
all services; lightweight callers explicitly select Plugins.

Destroying a URI player stops that resource and waits only for its workers.
Context shutdown closes all child players before provider retirement. Retained
handles are invalidated safely. Player resources use the creating thread;
callbacks never enter a foreign runtime from a decoder or audio output thread.

Keep generic provider environment bindings separate from optional catalog and
playback-remote attachments. Require `IDataStreamFactory::Create()` and
`IEnvironment::CreateDataStream(uri)` to return owned unopened resources without
blocking I/O. The caller publishes the resource before `Open`, allowing
`Interrupt` to reach startup work, including nested provider streams. Cancellation
must latch when it arrives before opening.

Replace the old factory-open and environment-open contracts in SDK 23. This major
release permits deliberate SDK breaks: requiring one creation path makes the
cancellation guarantee explicit instead of depending on plugin-specific exports.
The loader rejects other SDK versions before querying changed interfaces and
releases rejected plugin objects before unloading their code.

## Alternatives

A completely independent player runtime would duplicate provider-session,
configuration, notification, and teardown ownership. Context attachment provides
manual construction while sharing those established lifetimes. Extending catalog
playback with synthetic tracks would obscure origin validation and preserve
unnecessary library coupling. Waiting on the process-wide player counter during
individual destruction could block on an unrelated paused player.

An optional creation export would preserve old plugin binaries, but leave two
opening paths with different cancellation guarantees. Requiring create-before-open
removes that ambiguity and avoids carrying compatibility scaffolding into the
future C plugin SDK.

## Consequences

Callers can use musikcore for standalone playback without opening a collection.
Releasing a catalog playback handle and destroying an owned URI player have
different semantics and must be documented explicitly. Provider preferences and
discovery remain process-wide; this contract promises neither hardware mixing
nor sample-accurate multitrack scheduling. All existing C++ plugins must be
rebuilt against SDK 23. Providers remain responsible for making blocking opens
interruptible; publishing a resource enables cancellation but does not make an
uncooperative OS or provider operation interruptible by itself. This changes the
current C++ SDK, not the planned C plugin API or the public C ABI version.
