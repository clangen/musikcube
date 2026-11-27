# ADR-0003: separate origin-bound result membership from paged metadata

Status: accepted

Date: 2026-11-20

## Context

A music collection can be large, and the TUI needs only a window of its metadata
at a time. Migrating through a C API must not turn scrolling into one query or
string allocation per field. At the same time, a result must remain meaningful
when its library selection changes or tracks are edited or removed.

Track IDs alone are insufficient identity: another library, or a new remote
connection, can return the same number. Reinterpreting retained IDs against the
currently selected backend could repaint the wrong track or edit the wrong
playback queue.

## Decision and rationale

Keep immutable result membership separately from metadata pages. A result owns
its originating library identity and ordered IDs, including duplicates and
album grouping. Page reads fetch current metadata for a range without changing
that membership. A removed track retains its position with missing metadata;
a direct missing-track read reports not-found.

Return metadata in owned, immutable pages. Views refer to explicit page storage
rather than independently allocated strings. C callers retain the page while
using its views. The C++ TrackView retains shared page ownership, so storing a
track view is safe across page moves or cache eviction without copying its
metadata. Raw string views still require a live owner.

Dispatch library operations as typed queries through the selected backend.
The public operation should have the same semantics locally and remotely;
ad-hoc SQL in the public boundary would create a second local implementation
that remote libraries could not execute. Keep connection-generation identity
with remote results and reject stale operations rather than retargeting them.

Use the existing typed batch query for optional lossless metadata projection.
Projected remote requests require a projected response so that NULL and empty
values remain distinct.

Do not expose database handles or arbitrary SQL, including as a separately
unstable extension. Such an escape hatch would create a second storage-specific
contract that remote libraries cannot naturally implement. Add typed operations
when a concrete consumer needs additional behavior.

## Alternatives considered

- Offer raw SQL for storage-specific tools: callers become coupled to schema
  and local storage, defeating the shared local/remote operation boundary.
- Return all metadata with every search: unnecessary work and memory for rows
  that may never be displayed.
- Query individual fields or return owning strings from every accessor: extra
  calls, allocations, and copies in scrolling and display paths.
- Expose unretained pointers into transient backend objects: lifetime hazards
  when a query, page, or view is replaced.
- Resolve IDs through the current selection: stale results can silently refer
  to another catalog or connection generation.

## Consequences and implementation

A result freezes membership, not the whole database. A later page can show
updated metadata. Missing, explicit empty, and hidden values need distinct
representation and parity tests. Materialized pages remain readable after
remote disconnect; future reads still honor connection validity.

Paging and a bounded cache control ordinary scrolling memory. A caller can
intentionally retain more pages through TrackViews, so the cache budget does
not bound every object retained by application code. Shared ownership adds a
small measured allocation when adopting a page; it does not promise a wholly
allocation-free wrapper. Network serialization and initial page construction
still copy data; the goal is to avoid redundant per-access metadata copies.

Playback edits validate origin and captured queue revision before mutating the
live queue. The [LLD](../design/musikcore-c-abi-lld.md#library-result-and-playback-semantics)
records backend limits and current commands; its baseline section owns measured
budgets. Result, library-switch, indexed-query, and loopback tests verify this
contract through real consumers.
