# ADR-0010: give source-scoped indexing stable identity and durable checkpoints

Status: accepted

## Context

Provider tracks and playlists need identities that survive toolchain and
session changes. A source must not modify another source's records or depend on
the storage schema. Long scans can block other writers and lose substantial
progress if cancellation requires a scan-wide rollback.

## Decision and rationale

Identify a source by its plugin ID and stable source key, and derive a
deterministic numeric catalog identity. Display names are independent metadata;
renaming a source does not change its saved identity. Reject active identity
collisions rather than silently merging sources.

Expose restricted source-scoped writers rather than SQL. A checkpoint commits
a batch and releases its write lock; cancellation or failure rolls back only the
uncommitted batch. Completed checkpoints remain available, and source reads do
not reserve the database writer between batches. Sources must tolerate resuming
from committed progress.

If an enabled advertised source cannot be created or identified, log the failure
and terminate startup before orphan cleanup. Without its identity, continuing could delete
valid saved tracks as orphans. Explicitly disabling a source is a separate user
choice and can make its records eligible for cleanup.

## Alternatives considered

Standard-library hashes are not stable persistent identifiers. Raw SQL gives
providers schema access and authority outside their own source. A scan-wide
transaction is simpler but retains locks and repeats work after interruption.
Skipping an unidentified source risks data loss; deferring all orphan cleanup
preserves stale records from unrelated sources. Termination exposes the failure
without either silent consequence.

## Consequences

Indexing is resumable rather than all-or-nothing for the whole scan. New writer
operations require explicit host APIs. Stable IDs still need collision detection;
built-in historical IDs require deliberate migration, not an implicit universal
recovery mapping. Provider workers and source instances retire before unload.

The [source contract](../design/musikcore-c-abi-design.md#indexer-sources-and-source-scoped-writes)
owns discovery labels, timestamp representation, validation, batching, storage
configuration, and migration limitations.
