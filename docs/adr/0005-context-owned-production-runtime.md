# Context-owned production runtime

## Status

Accepted and implemented for musikcube.

## Context

The application previously supplied its UI message queue and attached a mutable library selector and playback service after creating the public context. That startup path required private C++ interfaces, and a host using the C API alone could not start the same services.

## Decision

The production context can own the configured catalogs, indexer, plugins, playback engine, and core message queue. Creation with engine services binds the calling thread as the dispatcher. A readiness thread wakes the host when a core message is due, and the host drains that queue through public dispatch. The runtime stops its readiness thread and producers as part of shutdown. Retained library handles keep their original catalog and connection generation.

The default context owns the complete engine. Callers may instead request an
immutable set of Library, Plugins, Indexer, and Playback services at creation.
Indexer and Playback each require Library and Plugins; Playback does not require
Indexer. Prerequisites are expanded automatically. An explicit empty service set
retains request processing for caller-owned catalogs or remote sessions. The
standalone ABI fixture implements that configuration for boundary tests.

This replaces the managed-runtime switch because ownership alone did not explain
which services started, and a dispatcher-only default made the common host do
extra setup. Naming actual services gives hosts a useful choice without exposing
internal startup order. Library creation therefore cannot implicitly construct
an indexer, and playback must tolerate a catalog without one. Registry discovery
is separate from legacy service attachment: providers receive catalog bindings
only when Library is available, and playback remotes attach only with Playback.

## Alternatives

Sharing the host's queue would preserve the private attachment and require the host to understand core message types. Dispatching core messages on a worker thread would change playback and notification thread ownership. Polling the queue from the host would require a timer even when the runtime is idle.

A dynamic start/stop API was considered. Fixing selection at creation keeps
startup rollback, dispatcher ownership, and teardown ordering tractable; callers
recreate the context when changing its services. Automatic dependency expansion
avoids a manual prerequisite sequence or errors for otherwise useful requests.
A negative query-only or engine-disabled switch would retain the same ambiguity
about individually selectable engine components.

## Consequences

musikcube and musikcubed start core services through the same public entry point as other production clients. Its UI queue is separate. Local and remote selection use concrete catalogs, and a playback queue resets when its catalog changes. The readiness thread adds one thread per managed context and performs no delivery.

The public-only managed-runtime test covers startup, delayed wakeups, restart, and retained handles. The managed loopback test covers local and remote selection, stale queue snapshots, and same-endpoint reconnect.

Startup restores local playback preferences and the saved queue before publishing
the managed context, so later dispatch cannot overwrite caller commands. Remote
restoration keeps its asynchronous path because the transport needs host dispatch.
Owner-thread violations have a distinct error from reentrant calls.

Teardown attempts all independent phases and every provider hook after a failure.
It reports an error after draining callbacks, leaves the stopped owner retryable,
and destroys provider-defined exceptions before leaving their cleanup phase.
Rollback and destructors do not throw. This hardening preserves the current
process-lifetime plugin registry; it does not introduce module unloading.
Session-scoped stream, codec, and visualizer caches retire after their workers
stop, so a replacement session rediscovers enabled providers. Concurrent stream
lookups retain a synchronized cache snapshot through factory use.

Migration-required indexing belongs to the catalog rather than a process-local
startup flag. A schema upgrade records that work transactionally even when
Indexer is absent; a later indexing session resumes it after host bindings and
subscriptions are ready. Completion retires the record with the scan transaction,
so shutdown or a failed completion write cannot lose required work across
processes. Provider cache teardown follows the provider session's lifetime,
including visualizers acquired without a playback engine.
