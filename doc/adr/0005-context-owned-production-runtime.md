# ADR-0005: let the context own the runtime and its selected services

Status: accepted

## Context

A public C host must be able to start the same runtime as musikcube without
constructing private queues, attaching mutable services, or reproducing internal
startup and shutdown order. Lightweight consumers should not need an indexer or
catalog playback merely to use providers.

## Decision and rationale

The context owns its configured engine services and their message queue.
Default creation starts the full engine. Callers may instead select an immutable
set of Library, Plugins, Indexer, and Playback services at creation. Prerequisites
expand automatically; Playback does not require Indexer. An empty set retains
request processing for caller-owned catalogs and remote sessions.

The host controls dispatch on one owner thread. Readiness wakes the host without
delivering callbacks from workers. Context shutdown stops admission and producers,
settles accepted callbacks, and retires dependent resources before their provider
session. Retained service handles cannot call retired runtime objects; owned
results retain their documented lifetime after shutdown. Plugin cleanup and unload follow [ADR-0007](0007-c-provider-bootstrap-and-unload-lifetime.md).

## Alternatives considered

Borrowing the host's internal queue requires private integration knowledge.
Worker-side dispatch changes callback ownership, while polling requires a timer
when idle. Dynamic service start/stop adds dependency and rollback transitions
without a current consumer requirement. Manual prerequisite sequencing makes
otherwise valid creation requests unnecessarily easy to misuse.

## Consequences

Applications use one public startup path and do not manage internal attachment
order. Changing the service set requires a new context. Resource teardown must
respect dependencies, including provider retirement after workers and callbacks
finish. Expected recoverable failures do not justify unloading code whose
cleanup or quiescence failed.

The [runtime design](../design/musikcore-c-abi-design.md#context-and-runtime-lifecycle)
owns startup restoration, current limits, and detailed shutdown behavior; the
LLD owns implementation work and platform acceptance.
