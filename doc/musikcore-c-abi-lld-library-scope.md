# C ABI LLD adendum: library query scopes

Status: **planned; required before application ABI v1 freeze**.

[LLD index](musikcore-c-abi-lld.md) · [C ABI design](musikcore-c-abi-design.md) ·
[Release gates](musikcore-c-abi-lld-release.md#phase-10-migration-cleanup-and-abi-v1-freeze)

## Goal

Provide a reusable, explicit way to restrict library browsing and queries by
indexer source ID before the application C ABI is frozen. Adding another
common filter later should not require adding a separate argument to every
query function. The mechanism must work with local and remote libraries and
must not expose SQL or database implementation details. A user must be able
to change the selected source set while the application is running, without
restarting the application or rescanning the library.

The initial scope contains only a set of source IDs. Visibility, directory,
metadata facets, playlist membership, rating, time and availability are
possible future filters and are outside this adendum. Do not add them to the
initial implementation or turn this into a general-purpose query language.
The shared mechanism must be extensible: adding a later filter should not
require changing every public query signature, replacing library ownership,
or inventing a separate filtering API.

## Starting point

Library operations already receive an opaque `mc_library` or derive a library
identity from a result or track reference. They are not all parameterized the
same way: track search and page query use different structs, while category,
facet, album, directory and playlist operations use other arguments. Their
local implementations use several typed queries; remote operations serialize
those queries to the server. `MC_TRACK_FIELD_SOURCE_ID` exposes a track's
source ID, but there is no public source-listing operation.

Source ID zero identifies ordinary file-indexed tracks; indexer providers use
catalog IDs derived from their plugin ID and stable source key. Source IDs
belong to a catalog, not to all libraries globally. The indexer's scan-source
sentinels do not define the public filtering contract.

## Proposed shape and decisions

Attach an optional, mutable scope to the existing `mc_library` handle. Provide
explicit operations to set, replace and clear that scope while the application
is running; an unscoped library keeps its current behavior. Existing query
entry points continue to receive `mc_library`. Do not introduce a separate
public scoped-library type. A sized C scope struct carries the selected source
IDs and leaves room for future filter fields, and the header-only C++ wrapper
may provide matching convenience methods.

For forward-compatible scope updates, prefer a sized C struct with typed
fields and a fixed-width enabled-filter bitmask over a generic ID/`void*` pair.
For example, the initial version can contain `struct_size`, an enabled-filter
mask, and a source-ID pointer/count. Future fields can be appended, with a new
mask bit for each filter. An older caller supplies a shorter struct and newer
libraries treat absent fields as unset. A newer caller sets the corresponding
bit when it requests a future filter; an older library rejects an unknown bit
rather than accepting the request without enforcing that filter. Validate that
every enabled field is present within `struct_size` before reading it.

This is a proposed compatibility pattern, not a finalized declaration. Confirm
it against the repository's existing ABI versioning and struct-size rules,
including the order of size and mask validation, before freezing the API. Do
not require a generic pointer-based filter registry unless a concrete use case
needs it. Carry the same reject-unknown rule through the remote protocol.

Each accepted request captures an immutable snapshot of the library's scope at
submission. Updating the attached scope changes subsequent operations rather
than retargeting accepted work or modifying previously returned results. The
current `mc_library` stores a shared immutable `LibraryIdentity`; determine how
to add mutable scope state and snapshots while preserving backend ownership,
remote routing, context selection and the result/track lifetimes. Specify
whether updates are visible through aliases or separately obtained handles,
and how they affect operations using the context's selected library. Different
catalogs must not share scope state accidentally.

A successful scope change must publish a library scope-changed event so
multiple observers can requery and redraw without polling. Delivery must occur
after the new scope is visible to subsequent queries. Reuse existing event or
subscription primitives where their ownership and threading semantics fit;
define the typed event, its originating library, callback thread and lifetime,
unsubscribe/shutdown behavior, and behavior for rejected or unchanged updates.
Provide a way for an observer to associate a refresh with the scope snapshot
and reject stale query completions. Specify how registration and the initial
scope read avoid missing a concurrent change. Do not call observers while
holding a lock they could need to query or update the scope.

The default must preserve today's unfiltered behavior. The contract must
separately represent no restriction, an explicitly empty source selection,
and selection of source ID zero. Decide whether empty selection means no
matches or is rejected; do not silently treat it as all sources. Validate IDs,
counts and pointer/count pairs at the owning boundary and copy caller-owned
arrays before returning from a scope update or query submission. Specify supported
ID range and a bounded selection size.

Define how a scope combines with existing search, category, facet and directory
predicates. Source selections normally mean any of the selected sources;
other query conditions narrow that result. Apply restrictions before grouping,
counts, album-boundary calculations, sorting, offset and limit. A category or
album shared by several sources must reflect only matching tracks without
splitting its catalog identity or double-counting it.

Define semantics explicitly for each operation, especially:

- Searches, category and facet lists, albums, directories and track pages.
- Saved playlists: whether a scoped read omits out-of-scope entries while
  retaining order and duplicates, and how membership changes between reads.
- Direct track references, caller-supplied IDs and artwork: whether each is
  scope-limited or explicitly catalog-wide, including missing/out-of-scope IDs.
- Playlist and metadata mutations: whether they are catalog-wide, rejected on
  scoped libraries, or have another explicit contract. Filtering must not silently
  change or delete entries outside the user's intended operation.
- Result and track-reference lifetimes, equality/identity, cancellation, backend
  changes and shutdown. Existing accepted operations keep their original scope
  snapshot, even when the attached library scope changes.

A scope is a query facility, not an authorization boundary. Server access
control remains independent. Do not infer visibility, offline availability,
source ownership or write permission from a source filter.

## Implementation checklist

- [ ] **SCOPE-001** Inventory the public library operations and their typed
  local/remote query paths. Classify each as scope-filtered, catalog-wide, or
  unsupported on scoped libraries; resolve the operation semantics above before
  changing public declarations. Record the decision and alternatives in an
  individual numbered ADR, linked from the HLD and this adendum.
- [ ] **SCOPE-002** Define the application C library-attached scope contract,
  including set, replace, clear, initialization, source ID zero, unrestricted
  versus empty, input limits, validation, copying, aliasing and lifetime.
  Add only fields required for the initial source filter; design extension
  through the existing sized-struct rules. Evaluate the typed-field and
  enabled-filter mask pattern above; define how missing fields default and how
  older libraries reject unknown requested filters rather than silently
  widening results. Keep ABI versions experimental.
- [ ] **SCOPE-003** Provide a usable catalog-scoped source-discovery operation
  or identify an existing public operation that supplies stable IDs and human
  labels. Specify ordinary file tracks, disabled or absent providers, sources
  with no tracks, and remote catalogs. Do not infer IDs from display labels.
- [ ] **SCOPE-004** Implement runtime source-selection changes and propagate
  immutable scope snapshots through library identities, async submissions,
  results and track references. Update the existing library handle; do not
  require creating another library type. A change applies to subsequent
  operations without restarting or rescanning. Specify and test how separately
  obtained handles and the context-selected library observe updates. Publish
  a scope-changed event after each effective change.
- [ ] **SCOPE-005** Apply the filter to local typed queries for track search,
  projections, category/facet and album browsing, directory reads and any
  other scope-filtered operations from SCOPE-001. Share predicate construction
  where it simplifies callers; bind every source ID, including each `IN` value,
  and choose SQL identifiers from fixed fragments. Do not push application
  query types into the generic database wrapper.
- [ ] **SCOPE-006** Implement the decided playlist, direct-ID, artwork and
  mutation behavior. Preserve playlist order and duplicates, missing-row
  behavior, track identity and explicit catalog-wide operations as specified.
- [ ] **SCOPE-007** Carry scopes through remote typed queries and the existing
  server query registry. Validate inputs on the boundary that owns them;
  unsupported/malformed filters must fail explicitly, never silently broaden a
  request. Leave a structured extension point for future filter fields without
  requiring a new wire operation for each filter. Clients and servers update in
  lockstep for 4.0; do not add an old
  server fallback or expose raw SQL.
- [ ] **SCOPE-008** Update the header-only C++ convenience API and real callers
  that require source filtering. Keep ownership explicit and wrappers dependent
  only on the public C ABI. Do not add unrelated filters to this task.
- [ ] **SCOPE-009** Add boundary tests for invalid sizes, ID/count limits,
  null/count mismatches, empty and unrestricted selection, source zero, copied
  caller storage, ownership, cancellation and shutdown. Test older-sized input
  against a newer library and an unknown enabled-filter bit against the current
  library, including a set bit whose field lies beyond `struct_size`. Assert
  rejection occurs before backend/provider work and scoped requests cannot
  escape their catalog.
- [ ] **SCOPE-010** Add local and real remote parity tests with multiple sources,
  shared artists/albums, duplicate playlist entries, hidden or missing tracks,
  distinct library handles, scope updates, grouped counts, ordering and page
  boundaries.
  Test the explicitly decided ID, artwork and mutation semantics. Gate and
  reverse request completions while changing sources to prove older responses
  cannot overwrite newer selections or mix scope membership.
- [ ] **SCOPE-011** Update public header/default-initializer checks, exported
  symbols, ABI-only and installed C/C++ consumer tests, wire fixtures, examples
  and contract documentation. Validate named-platform builds and tests under
  the normal project gates; keep missing platform evidence open rather than
  assuming it from another platform.
- [ ] **SCOPE-012** Add a user-facing source selector to the TUI so users can
  toggle the active sources while browsing. Specify all-sources and empty-set
  behavior, how selection changes refresh dependent results, and whether a
  selection persists across restarts. Observe scope-changed events to refresh
  results and redraw. Keep existing content visible during refresh when
  practical, without presenting stale results as the new scope.
  Add a UI scenario for switching sources and rejecting late old results.
- [ ] **SCOPE-013** Implement and test library scope-changed notifications.
  Cover multiple observers, update ordering, unchanged and rejected updates,
  reentrant queries, subscription lifetime and shutdown, registration racing
  an update, and identifying stale asynchronous results. Verify callbacks run
  outside internal locks and no observer sees an event for an uncommitted scope.
- [ ] **SCOPE-014** Complete the scope acceptance gate: all applicable tasks
  above are complete, the ADR and public contract match implementation, and
  the maintainer has reviewed any SDK/API contract decisions. Only then may the
  application ABI v1 freeze proceed under FIN-008/FIN-009 and GATE-1000.
