# ADR-0017: Library scope and query lifetime

Status: accepted.

## Context

Users need to change source selection while browsing a library. Queries may
complete out of order, and filtering playlist contents could hide entries that
users expect to remain in their saved playlists.

## Decision

Attach scope to the library instance. Handles referring to that instance share
the selection; independently created instances remain independent, including
instances addressing the same catalog.

Scope-filtered queries capture immutable snapshots at submission. An effective
scope change invalidates pending queries from the previous selection. Those
requests complete through the existing cancellation contract rather than
delivering successful stale results. Interrupt backend work where practical.
Equivalent source sets, regardless of ordering or duplicates, do not invalidate
requests or publish a scope-changed event. An unrestricted selection includes
all sources, including newly added ones. An explicitly empty selection returns
no matches for scope-filtered queries; playlist contents remain unaffected; empty selection lists only global playlists.

Internal typed queries carry a shared `LibraryQueryScope` value type rather than
separate source-ID fields on each query. Each copied value owns its selection
storage and exposes read-only access; replacing an instance's current scope
does not mutate query snapshots. Initially the value contains only source
selection. It is independent of the header-only public C++ convenience wrapper,
which must continue to depend solely on the public C ABI.

Already delivered results retain their original snapshot, including derived
page reads and track references. Scope changes publish notifications so callers
can refresh displayed content. Completion admission and scope replacement are
ordered under the same lock. If replacement wins, the request is cancelled; if
completion admission wins, its callback may finish after the change. Admission
does not hold that lock while running user callbacks. Callers need not manage
public scope revisions. Invalidation uses normal request cancellation immediately,
without waiting for a backend reply, and discards pending remote transport work
so it does not retain a queue slot until timeout.

Playlist ownership is either global or one catalog source. Discovery lists
all playlists when unrestricted; with a source selection it lists global
playlists plus those owned by selected sources. An empty selection lists only
global playlists. Pending discovery requests follow scope invalidation.

Contents and paging retain every saved entry, preserving order and duplicates.
Source-owned playlists accept only tracks with the owner's source ID; global
playlists accept mixed sources. Creation specifies ownership explicitly, never
inferring it from the active scope. Replacement and append preserve ownership
and reject an entire invalid batch before committing any change.

Persist ownership as nullable `playlists.source_id`: NULL means global, zero
means ordinary files. Existing playlists migrate to global ownership. Database
constraints enforce matching entry sources for inserts, updates and ownership
changes. Public C and WebSocket operations must validate the whole mutation and
report expected input errors without partial writes. Source ownership does not
imply provider-managed or read-only status.

The public C scope uses nullable pointers to typed filters. Every non-null
filter applies; a null source filter is unrestricted, and a present empty
source filter matches nothing. Older-sized scope inputs default absent fields
to null. Unknown larger scope or filter layouts are rejected so an older
library cannot silently ignore a future filter. Remote scope objects use the
same null-versus-array semantics and reject unknown filter keys.

Source discovery combines loaded provider descriptions with catalog references,
without a new sources table. Loaded providers expose empty sources. When a
provider is absent, referenced source IDs remain selectable with `Source N`
fallback labels; historical labels and unreferenced empty sources are not
persisted. The ordinary-file source is always labelled Files. Descriptions are
immutable library data so discovery does not borrow an indexer during shutdown.

## Alternatives and rationale

Allowing old queries to complete successfully with public scope revisions would
preserve their results but require every refreshing caller to reject stale
responses. Invalidation centralizes that responsibility at the library boundary.

Retargeting pending queries would change their original intent. Per-handle scope
would make aliases disagree about their library instance's selection. Filtering
playlists would hide saved entries and complicate editing them safely.

## Consequences

Scope changes can cancel useful pending work. Backends may finish work that
cannot be interrupted, but the completion boundary must suppress stale success.
Submission, invalidation and completion delivery require consistent ordering;
internal generations may enforce it without exposing revisions publicly.

The [scope LLD](../../doc/musikcore-c-abi-lld-library-scope.md) records remaining
operation classifications, completion-race details, implementation tasks and
acceptance tests.
