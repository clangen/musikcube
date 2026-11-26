# ADR-0013 — Indexer source identity and checkpoints

**Status** Accepted

## Context

Indexer plugins discover tracks outside the ordinary file index. Their external
IDs and numeric source IDs connect those tracks to existing catalog records and
playlists. Older plugins derived numeric IDs from `std::hash`, whose result is
not a portable persistent identity. A long scan can also be interrupted after
it has already written many records.

## Decision

Identify an indexer source by its plugin ID and stable source key, and derive
a deterministic numeric catalog ID from that pair. A database migration
rewrites historical GME and OpenMPT track and playlist source IDs using their
virtual URI prefixes. This preserves the normal built-in cases without making
every indexer startup resolve a historical identity mapping.

Give each source a writer scoped to its own scan and records. It can save and
remove its tracks, inspect their stored modification times, and commit a
checkpoint; it cannot issue arbitrary SQL or address another source's tracks.
Each source also supplies a nonempty UTF-8 display name, independently of its
stable key and the plugin's decoder metadata. The host validates and copies the
name before publishing source lists; changing it does not change persistent IDs.
Source selectors show that name locally and over the library protocol. The
built-in file source is named "Music files".

Keep existing modification-time values in the source's units and epoch, and
report a missing value separately from a negative timestamp.

A checkpoint commits the current batch. Cancellation, a provider failure, or
an indexer writer's i/o or internal error rolls back the uncommitted batch;
earlier checkpoints remain. A checkpoint ends the transaction and releases its
write lock; the next mutation starts another batch. Source startup and filesystem
reads after a checkpoint do not reserve the database writer. The built-in GME
and OpenMPT sources checkpoint after a short elapsed interval or their record
limit, whichever is reached first. The interval is checked between files, so one
slow file can exceed it. Ordinary file indexing also checks elapsed time between
completed files while retaining its batched writes. Existing-track validation
releases each read statement before calling the source and checkpoints mutations
between entries, so filesystem checks do not retain a catalog reader or reserve
its writer.

Use private SQLite caches for catalog connections. Competing writes can wait
through the busy handler instead of failing immediately on shared-cache table
locks. This leaves SQLite responsible for writer exclusion and avoids adding
application locks around network or plugin calls.

A provider can request another scan through a source-scoped host service. Stop
its worker and destroy the source instance before unloading its plugin.

If an enabled plugin advertises an indexer source but that source cannot be
created or identified, log the failure and terminate startup. The source key is
unavailable when creation fails, so the indexer cannot safely identify its saved
tracks before orphan cleanup.

## Alternatives and consequences

Reusing the old standard-library hash would be simpler but could orphan tracks
when toolchains change. The migration handles the built-in formats; unknown or
unusual legacy source IDs are not guaranteed to migrate. Stable IDs can collide,
so the indexer rejects two active sources with the same numeric ID.

Making an entire scan atomic would avoid partial progress, but large collections
could hold a transaction open for a long time and repeat all work after an
interruption. Checkpoints bound that work; source implementations must tolerate
resuming after a committed batch. Restricting writers to one source removes a
powerful raw-SQL escape hatch, so new indexing operations require explicit host
APIs when a real plugin needs them.

Skipping a failed source would keep the app running, but could remove its tracks
as orphans. Deferring all orphan cleanup would preserve the tracks while also
leaving unrelated obsolete tracks in the catalog. Terminating makes the failure
visible and prevents either outcome. Disabling the source allows startup, but
its existing tracks may be removed as orphans on a later scan.
