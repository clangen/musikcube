# Context-owned production runtime

## Status

Accepted and implemented for musikcube.

## Context

The application previously supplied its UI message queue and attached a mutable library selector and playback service after creating the public context. That startup path required private C++ interfaces, and a host using the C API alone could not start the same services.

## Decision

The production context can own the configured catalogs, indexer, plugins, playback engine, and core message queue. Managed creation binds the calling thread as the dispatcher. A readiness thread wakes the host when a core message is due, and the host drains that queue through public dispatch. The runtime stops its readiness thread and producers as part of shutdown. Retained library handles keep their original catalog and connection generation.

The dispatcher-only configuration remains available for clients that supply their own catalog or remote session. The standalone ABI fixture implements that configuration for boundary tests.

## Alternatives

Sharing the host's queue would preserve the private attachment and require the host to understand core message types. Dispatching core messages on a worker thread would change playback and notification thread ownership. Polling the queue from the host would require a timer even when the runtime is idle.

## Consequences

Musikcube starts core services through the same public entry point as other production clients. Its UI queue is separate. Local and remote selection use concrete catalogs, and a playback queue resets when its catalog changes. The readiness thread adds one thread per managed context and performs no delivery.

The public-only managed-runtime test covers startup, delayed wakeups, restart, and retained handles. The managed loopback test covers local and remote selection, stale queue snapshots, and same-endpoint reconnect.
