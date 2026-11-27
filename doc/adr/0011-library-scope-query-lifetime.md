# ADR-0011: bind library scope and query intent to the library instance

Status: accepted

## Context

Users change source selections while queries are in flight. Treating scope as
global state, silently retargeting requests, or requiring every frontend to reject
stale success can produce inconsistent results. Playlist filtering also risks
hiding saved entries or allowing invalid source-owned edits.

## Decision and rationale

Scope belongs to a library instance. Aliases share its selection; independently
created instances remain independent, including those addressing the same catalog.
Filtered queries capture immutable intent at submission. An effective scope change
invalidates pending work through normal cancellation rather than delivering stale
success or retargeting it. Equivalent selections do not invalidate work.

Already delivered results keep their captured membership and scope, including
derived reads and track references. Completion admission is ordered with scope
replacement: a completion admitted first may finish afterward, while replacement
first selects cancellation. Callbacks run without the admission lock.

Playlists are globally owned or owned by one source. Discovery includes global
playlists and those belonging to selected sources; contents always retain every
saved entry in order, including duplicates. Source-owned playlists accept only
that source's tracks. Creation names ownership explicitly rather than inferring
it from active scope. Invalid mutation batches fail atomically.

## Alternatives considered

Public revisions would let old queries succeed but force every refreshing
client to reject stale replies. Retargeting changes original intent. Per-handle
scope makes aliases disagree, while process-global scope couples independent
library consumers. Filtering saved contents hides data and complicates editing.
Inferring playlist ownership from current selection makes creation ambiguous.

## Consequences

Scope changes can cancel useful work. Backend work may finish if interruption
is impractical, but the completion boundary must suppress stale success. No public
revision counter is required. Playlist ownership constrains writes independently
of view selection and does not imply provider management or read-only status.

The [scope and playlist contract](../design/musikcore-c-abi-design.md#library-scope-and-source-owned-playlists)
owns filter representation, empty selections, discovery, schema details, and
wire validation. The [scope LLD](../design/musikcore-c-abi-lld-library-scope.md)
owns tasks and acceptance evidence.
