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
context providing Plugins. It owns its transport/output, uses context wakeups
and dispatch for foreign callbacks, and requires no catalog services. Its C++
wrapper remains header-only over that C contract. Default contexts still enable
all services; lightweight callers explicitly select Plugins.

Destroying a URI player stops that resource and waits only for its workers.
Context shutdown closes all child players before provider retirement. Retained
handles are invalidated safely. Player resources use the creating thread;
callbacks never enter a foreign runtime from a decoder or audio output thread.

Keep generic provider environment bindings separate from optional catalog and
playback-remote attachments. Stream providers may offer an optional legacy
`CreateDataStream` export that returns an unopened resource. Publishing it before
`Open` makes interruption possible during network startup without modifying the
existing provider vtable. Existing factories retain their original entry path.

## Alternatives

A completely independent player runtime would duplicate provider-session,
configuration, notification, and teardown ownership. Context attachment provides
manual construction while sharing those established lifetimes. Extending catalog
playback with synthetic tracks would obscure origin validation and preserve
unnecessary library coupling. Waiting on the process-wide player counter during
individual destruction could block on an unrelated paused player.

## Consequences

Callers can use musikcore for standalone playback without opening a collection.
Releasing a catalog playback handle and destroying an owned URI player have
different semantics and must be documented explicitly. Provider preferences and
discovery remain process-wide; this contract promises neither hardware mixing
nor sample-accurate multitrack scheduling. Provider opens without the optional
hook remain subject to the legacy provider's own blocking limits.
